#include "infer.hpp"
#include "avx2_math.hpp"
#include "SessionKVCache.hpp"
#include "vulkan_compute.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <numeric>
#include <algorithm>
#include <cstring>

std::vector<int> tokenize(const Vocab& vocab, const std::string& text) {
    std::string bpe_text;
    for (char c : text) {
        if (c == ' ') {
            bpe_text += (char)0xC4;
            bpe_text += (char)0xA0;
        } else if (c == '\n') {
            bpe_text += (char)0xC4;
            bpe_text += (char)0x8A;
        } else {
            bpe_text += c;
        }
    }
    
    std::vector<int> tokens;
    size_t i = 0;
    while (i < bpe_text.length()) {
        int best_id = -1;
        size_t best_len = 0;
        
        TrieNode* curr = vocab.root.get();
        for (size_t j = i; j < bpe_text.length(); ++j) {
            char c = bpe_text[j];
            auto it = curr->children.find(c);
            if (it == curr->children.end()) {
                break;
            }
            curr = it->second.get();
            if (curr->token_id != -1) {
                best_id = curr->token_id;
                best_len = j - i + 1;
            }
        }
        
        if (best_id == -1) {
            // Fallback (should not happen with a proper BPE vocab that has byte tokens)
            std::cout << "[Tokenizer] Unmatched char: " << bpe_text[i] << std::endl;
            i += 1;
        } else {
            tokens.push_back(best_id);
            i += best_len;
        }
    }
    return tokens;
}

std::string detokenize(const Vocab& vocab, int token_id) {
    if (token_id >= 0 && token_id < (int)vocab.tokens.size()) {
        std::string s = vocab.tokens[token_id];
        size_t pos = 0;
        while ((pos = s.find("\xC4\xA0", pos)) != std::string::npos) {
            s.replace(pos, 2, " ");
            pos += 1;
        }
        pos = 0;
        while ((pos = s.find("\xC4\x8A", pos)) != std::string::npos) {
            s.replace(pos, 2, "\n");
            pos += 1;
        }
        return s;
    }
    return "";
}

void init_inference_state(const ModelConfig& config, int max_context, InferenceState& state) {
    state.x.resize(config.embedding_length, 0.0f);
    state.current_pos = 0;
    
    state.k_cache.resize(config.num_layers, std::vector<float>(max_context * config.num_kv_heads * config.head_dim, 0.0f));
    state.v_cache.resize(config.num_layers, std::vector<float>(max_context * config.num_kv_heads * config.head_dim, 0.0f));
    
    float base = (config.rope.base > 0.0f) ? config.rope.base : 10000.0f;
    // Dynamic NTK-aware RoPE frequency scaling for extended contexts (up to 64K)
    if (max_context > 8192 && config.head_dim > 2) {
        float scale = static_cast<float>(max_context) / 8192.0f;
        base = base * std::pow(scale, static_cast<float>(config.head_dim) / (config.head_dim - 2));
    }
    state.inv_freq.resize(config.head_dim / 2);
    for (int i = 0; i < (int)config.head_dim; i += 2) {
        state.inv_freq[i / 2] = 1.0f / std::pow(base, (float)i / config.head_dim);
    }

    
    std::cout << "[Infer] Initializing KV Cache for max context: " << max_context 
              << " | RoPE Base: " << base << " | Intermediate Dim: " << config.intermediate_dim << std::endl;
}

inline float fp16_to_fp32(uint16_t h) {
    __m128i h_vec = _mm_set1_epi16(h);
    __m128 f_vec = _mm_cvtph_ps(h_vec);
    return _mm_cvtss_f32(f_vec);
}

void dequantize_q8_row(const block_q8_0* x, float* y, int num_blocks) {
    for (int i = 0; i < num_blocks; ++i) {
        float d = fp16_to_fp32(x[i].d);
        for (int j = 0; j < 32; ++j) {
            y[i * 32 + j] = x[i].qs[j] * d;
        }
    }
}

void dequantize_q4_row(const block_q4_0* x, float* y, int num_blocks) {
    for (int i = 0; i < num_blocks; ++i) {
        float d = fp16_to_fp32(x[i].d);
        for (int j = 0; j < 16; ++j) {
            int x0 = (x[i].qs[j] & 0x0F) - 8;
            int x1 = (x[i].qs[j] >> 4) - 8;
            y[i * 32 + j] = x0 * d;
            y[i * 32 + j + 16] = x1 * d;
        }
    }
}

void matvec(const Tensor& w, const float* x, float* out, int in_features, int out_features) {
    int nb = in_features / 32;
    if (w.type == TensorType::Q4_0) {
        const block_q4_0* w_data = static_cast<const block_q4_0*>(w.data);
        #pragma omp parallel for
        for (int i = 0; i < out_features; ++i) {
            out[i] = math::dot_product_q4_0_fp32(&w_data[i * nb], x, nb);
        }
    } else {
        const block_q8_0* w_data = static_cast<const block_q8_0*>(w.data);
        #pragma omp parallel for
        for (int i = 0; i < out_features; ++i) {
            out[i] = math::dot_product_q8_fp32(&w_data[i * nb], x, nb);
        }
    }
}

inline void matvec_q8(const Tensor& w, const float* x, float* out, int in_features, int out_features) {
    matvec(w, x, out, in_features, out_features);
}

static std::string native_tensor_error(const std::string& tensor_name, TensorType type) {
    return "Inference Error: Unsupported tensor representation.\n"
           "  => Failing Tensor: '" + tensor_name + "'\n"
           "  => Detected Type:  " + tensor_type_name(type) + "\n"
           "  => Resolution:     Native execution requires Q4_0/Q8_0 for matrices, and FP32 for 1D biases/norm.";
}

bool forward_pass(DenseModel& model, InferenceState& state, int token_id, std::vector<float>& logits) {
    auto& config = model.config;
    for (const auto& entry : model.tensors) {
        if (!native_tensor_supported(entry.first, entry.second.type)) {
            std::cerr << "[Infer] " << native_tensor_error(entry.first, entry.second.type) << std::endl;
            return false;
        }
    }
    
    // 1. Token Embedding Lookup (Q4_0 / Q8_0 dequantize)
    if (token_id < 0 || token_id >= (int)config.vocab_size) {
        std::cerr << "[Infer] Invalid token_id: " << token_id << " (vocab_size: " << config.vocab_size << ")" << std::endl;
        return false;
    }
    if (model.tensors.count("token_embd.weight") == 0) {
        std::cerr << "Missing token_embd.weight" << std::endl;
        return false;
    }
    
    const auto& embd_tensor = model.tensors["token_embd.weight"];
    int embd_blocks = config.embedding_length / 32;
    if (embd_tensor.type == TensorType::Q4_0) {
        const block_q4_0* embd_data = static_cast<const block_q4_0*>(embd_tensor.data);
        dequantize_q4_row(&embd_data[token_id * embd_blocks], state.x.data(), embd_blocks);
    } else if (embd_tensor.type == TensorType::Q8_0) {
        const block_q8_0* embd_data = static_cast<const block_q8_0*>(embd_tensor.data);
        dequantize_q8_row(&embd_data[token_id * embd_blocks], state.x.data(), embd_blocks);
    } else {
        std::cerr << "[Infer] Error: Unsupported tensor type " << static_cast<int>(embd_tensor.type) 
                  << " for 'token_embd.weight'. DenseLite CPU kernel natively supports only Q4_0 and Q8_0." << std::endl;
        return false;
    }
    
    // Model-driven transformer layer dimensions (Level 1: Qwen2/Llama family)
    int head_dim = config.head_dim;
    int num_kv_features = config.num_kv_heads * head_dim;
    int mlp_hidden_dim = config.intermediate_dim;
    int kv_groups = (config.num_kv_heads > 0) ? (config.num_heads / config.num_kv_heads) : 1;
    
    // Pre-allocate per-token working buffers
    std::vector<float> q(config.embedding_length);
    std::vector<float> k(num_kv_features);
    std::vector<float> v(num_kv_features);
    std::vector<float> att_out(config.embedding_length);
    std::vector<float> mlp_gate(mlp_hidden_dim);
    std::vector<float> mlp_up(mlp_hidden_dim);
    std::vector<float> residual(config.embedding_length);
    std::vector<float> proj(config.embedding_length);
    std::vector<float> att_scores(state.current_pos + 2);
    
    // 2. Transformer Layers
    for (uint32_t l = 0; l < config.num_layers; ++l) {
        std::string lp = "blk." + std::to_string(l) + ".";
        
        // =====================================================================
        // ATTENTION BLOCK
        // =====================================================================
        std::memcpy(residual.data(), state.x.data(), config.embedding_length * sizeof(float));
        
        // Pre-attention RMSNorm
        if (model.execution_context == DeviceContext::GPU && state.vulkan_compute && state.vulkan_compute->is_ready()) {
            state.vulkan_compute->rmsnorm(state.x.data(), (const float*)model.tensors[lp + "attn_norm.weight"].data,
                                          state.x.data(), config.embedding_length, config.rms_norm_eps);
        } else {
            math::rmsnorm(state.x.data(), state.x.data(), config.embedding_length,
                          config.rms_norm_eps,
                          (float*)model.tensors[lp + "attn_norm.weight"].data);
        }
        
        // Q, K, V Projections (matvec)
        matvec_q8(model.tensors[lp + "attn_q.weight"], state.x.data(), q.data(),
                  config.embedding_length, config.embedding_length);
        matvec_q8(model.tensors[lp + "attn_k.weight"], state.x.data(), k.data(),
                  config.embedding_length, num_kv_features);
        matvec_q8(model.tensors[lp + "attn_v.weight"], state.x.data(), v.data(),
                  config.embedding_length, num_kv_features);
        
        // Add biases if present
        if (model.tensors.count(lp + "attn_q.bias")) {
            float* b = (float*)model.tensors[lp + "attn_q.bias"].data;
            for (int i = 0; i < (int)config.embedding_length; ++i) q[i] += b[i];
        }
        if (model.tensors.count(lp + "attn_k.bias")) {
            float* b = (float*)model.tensors[lp + "attn_k.bias"].data;
            for (int i = 0; i < num_kv_features; ++i) k[i] += b[i];
        }
        if (model.tensors.count(lp + "attn_v.bias")) {
            float* b = (float*)model.tensors[lp + "attn_v.bias"].data;
            for (int i = 0; i < num_kv_features; ++i) v[i] += b[i];
        }
        math::rope(q.data(), state.current_pos, config.num_heads, head_dim, state.inv_freq.data());
        math::rope(k.data(), state.current_pos, config.num_kv_heads, head_dim, state.inv_freq.data());
        
        // -----------------------------------------------------------------
        // GQA KV CACHE ATTENTION
        // -----------------------------------------------------------------
        int pos = state.current_pos;
        
        // Store K and V into cache at current position
        int cache_offset = pos * num_kv_features;
        if (static_cast<size_t>(cache_offset + num_kv_features) <= state.k_cache[l].size()) {
            std::memcpy(&state.k_cache[l][cache_offset], k.data(), num_kv_features * sizeof(float));
            std::memcpy(&state.v_cache[l][cache_offset], v.data(), num_kv_features * sizeof(float));
        }
        
        // Compute attention for each Q head
        
        for (uint32_t h = 0; h < config.num_heads; ++h) {
            uint32_t kv_h = h / kv_groups;  // Map Q head -> KV head
            float* q_head = q.data() + h * head_dim;
            
            // Dot product Q . K^T for all cached positions
            float max_score = -1e9f;
            for (int p = 0; p <= pos; ++p) {
                float* k_head = &state.k_cache[l][p * num_kv_features + kv_h * head_dim];
                float score = 0.0f;
                for (int d = 0; d < head_dim; ++d) {
                    score += q_head[d] * k_head[d];
                }
                score /= std::sqrt((float)head_dim);
                att_scores[p] = score;
                if (score > max_score) max_score = score;
            }
            
            // Softmax over attention scores
            float sum = 0.0f;
            for (int p = 0; p <= pos; ++p) {
                att_scores[p] = std::exp(att_scores[p] - max_score);
                sum += att_scores[p];
            }
            for (int p = 0; p <= pos; ++p) {
                att_scores[p] /= sum;
            }
            
            // Weighted sum of V
            float* out_head = att_out.data() + h * head_dim;
            for (int d = 0; d < head_dim; ++d) out_head[d] = 0.0f;
            
            for (int p = 0; p <= pos; ++p) {
                float* v_head = &state.v_cache[l][p * num_kv_features + kv_h * head_dim];
                float s = att_scores[p];
                for (int d = 0; d < head_dim; ++d) {
                    out_head[d] += s * v_head[d];
                }
            }
        }
        
        // Output Projection + Residual
        {
            matvec_q8(model.tensors[lp + "attn_output.weight"], att_out.data(), proj.data(),
                      config.embedding_length, config.embedding_length);
            for (int i = 0; i < (int)config.embedding_length; ++i) {
                state.x[i] = residual[i] + proj[i];
            }
        }
        
        // =====================================================================
        // FFN BLOCK (SwiGLU)
        // =====================================================================
        std::memcpy(residual.data(), state.x.data(), config.embedding_length * sizeof(float));
        
        // Pre-FFN RMSNorm
        if (model.execution_context == DeviceContext::GPU && state.vulkan_compute && state.vulkan_compute->is_ready()) {
            state.vulkan_compute->rmsnorm(state.x.data(), (const float*)model.tensors[lp + "ffn_norm.weight"].data,
                                          state.x.data(), config.embedding_length, config.rms_norm_eps);
        } else {
            math::rmsnorm(state.x.data(), state.x.data(), config.embedding_length,
                          config.rms_norm_eps,
                          (float*)model.tensors[lp + "ffn_norm.weight"].data);
        }
        
        // In GGUF/llama.cpp naming:
        //   ffn_gate = the projection that gets SiLU applied (w1 in the paper)
        //   ffn_up   = the projection that gets multiplied (w3 in the paper)
        //   ffn_down = the down projection (w2 in the paper)
        // SwiGLU: out = ffn_down( SiLU(ffn_gate(x)) * ffn_up(x) )
        
        matvec_q8(model.tensors[lp + "ffn_gate.weight"], state.x.data(), mlp_gate.data(),
                  config.embedding_length, mlp_hidden_dim);
        matvec_q8(model.tensors[lp + "ffn_up.weight"], state.x.data(), mlp_up.data(),
                  config.embedding_length, mlp_hidden_dim);
        
        // SwiGLU activation: result = SiLU(gate) * up
        math::swiglu(mlp_gate.data(), mlp_up.data(), mlp_hidden_dim);
        
        // Down Projection + Residual
        {
            matvec_q8(model.tensors[lp + "ffn_down.weight"], mlp_gate.data(), proj.data(),
                      mlp_hidden_dim, config.embedding_length);
            for (int i = 0; i < (int)config.embedding_length; ++i) {
                state.x[i] = residual[i] + proj[i];
            }
        }
    }
    
    // 3. Final RMSNorm
    if (model.execution_context == DeviceContext::GPU && state.vulkan_compute && state.vulkan_compute->is_ready()) {
        state.vulkan_compute->rmsnorm(state.x.data(), (const float*)model.tensors["output_norm.weight"].data,
                                      state.x.data(), config.embedding_length, config.rms_norm_eps);
    } else {
        math::rmsnorm(state.x.data(), state.x.data(), config.embedding_length,
                      config.rms_norm_eps,
                      (float*)model.tensors["output_norm.weight"].data);
    }
    
    // 4. LM Head (Vocab Projection)
    // Some models (e.g. Qwen2.5-1.5B, SmolLM2) tie word embeddings and omit output.weight
    const Tensor& output_weight = model.tensors.count("output.weight")
        ? model.tensors.at("output.weight")
        : model.tensors.at("token_embd.weight");
    matvec_q8(output_weight, state.x.data(), logits.data(),
              config.embedding_length, config.vocab_size);
    return true;
}

void generate(DenseModel& model, const std::vector<int>& prompt_tokens, StreamCallback callback,
              int max_tokens, float temperature, float repetition_penalty,
              SessionKVState* session_kv, int context_budget) {
    if (prompt_tokens.empty()) return;

    InferenceState local_state;
    InferenceState* state_ptr = nullptr;

    int base_ctx = context_budget > 0 ? context_budget : (model.config.context_length > 0 ? model.config.context_length : 4096);
    int ctx_len = std::min(base_ctx, 65536);
    
    std::vector<int> effective_tokens;
    size_t start_prefill_idx = 0;
    std::unique_lock<std::mutex> kv_lock;
    if (session_kv) {
        kv_lock = std::unique_lock<std::mutex>(session_kv->state_mutex);
        if (!session_kv->is_initialized || session_kv->max_context_allocated < ctx_len) {
            init_inference_state(model.config, ctx_len, session_kv->state);
            session_kv->max_context_allocated = ctx_len;
            session_kv->num_kv_heads = model.config.num_kv_heads;
            session_kv->head_dim = model.config.head_dim;
            session_kv->is_initialized = true;
            session_kv->cached_tokens.clear();
        } else {
            // Match common prefix with previous conversation turn
            size_t common_len = 0;
            while (common_len < session_kv->cached_tokens.size() &&
                   common_len < prompt_tokens.size() - 1 &&
                   session_kv->cached_tokens[common_len] == prompt_tokens[common_len]) {
                common_len++;
            }
            start_prefill_idx = common_len;
            session_kv->state.current_pos = static_cast<int>(common_len);
        }
        state_ptr = &session_kv->state;
    } else {
        init_inference_state(model.config, ctx_len, local_state);
        state_ptr = &local_state;
    }

    if (prompt_tokens.size() >= static_cast<size_t>(ctx_len)) {
        size_t keep = ctx_len - 1;
        effective_tokens.assign(prompt_tokens.end() - keep, prompt_tokens.end());
        start_prefill_idx = 0;
        if (session_kv) {
            session_kv->cached_tokens.clear();
            session_kv->state.current_pos = 0;
        }
    } else {
        effective_tokens = prompt_tokens;
    }

    InferenceState& state = *state_ptr;
    std::vector<float> logits(model.config.vocab_size);
    
    // RNG for sampling
    std::mt19937 rng(std::random_device{}());
    
    // Track generated tokens for repetition penalty
    std::vector<int> generated_tokens;
    generated_tokens.reserve(max_tokens);
    
    // 1. Delta Prefill: process only un-cached prompt tokens within allocated KV bounds
    for (size_t i = start_prefill_idx; i < effective_tokens.size() - 1; ++i) {
        if (state.current_pos >= ctx_len - 1) break;
        if (!forward_pass(model, state, effective_tokens[i], logits)) {
            std::cerr << "[Infer] Prefill aborted due to forward pass failure." << std::endl;
            callback("\n\n[DenseLite Runtime Error: " + native_tensor_error(model) + "]");
            return;
        }
        state.current_pos++;
    }

    if (session_kv) {
        session_kv->cached_tokens = effective_tokens;
    }
    
    // 2. Start generation from the last prompt token
    int current_token = effective_tokens.back();
    constexpr int TOP_K = 40;
    std::vector<std::pair<float, int>> candidates(model.config.vocab_size);
    
    for (int step = 0; step < max_tokens; ++step) {
        if (state.current_pos >= ctx_len - 1) break;
        if (!forward_pass(model, state, current_token, logits)) {
            std::cerr << "[Infer] Generation aborted due to forward pass failure." << std::endl;
            callback("\n\n[DenseLite Runtime Error: " + native_tensor_error(model) + "]");
            return;
        }
        
        // Repetition Penalty (multiplicative)
        if (repetition_penalty != 1.0f) {
            for (int tok : generated_tokens) {
                if (tok >= 0 && tok < (int)model.config.vocab_size) {
                    logits[tok] = (logits[tok] > 0.0f) ? (logits[tok] / repetition_penalty) : (logits[tok] * repetition_penalty);
                }
            }
        }
        
        int next_token;
        if (temperature <= 0.01f) {
            next_token = 0;
            float max_val = logits[0];
            for (int i = 1; i < (int)model.config.vocab_size; ++i) {
                if (logits[i] > max_val) { max_val = logits[i]; next_token = i; }
            }
        } else {
            // --- Top-K Sampling with Temperature ---
            
            // Build index array of top-k candidates
            for (int i = 0; i < (int)model.config.vocab_size; ++i) {
                candidates[i] = {logits[i], i};
            }
            
            // Partial sort to get top-k
            int k = std::min(TOP_K, (int)model.config.vocab_size);
            std::nth_element(candidates.begin(), candidates.begin() + k, candidates.end(),
                              [](const auto& a, const auto& b) { return a.first > b.first; });
            
            // Temperature-scaled softmax over top-k
            float max_logit = -1e9f;
            for (int i = 0; i < k; ++i) {
                if (candidates[i].first > max_logit) max_logit = candidates[i].first;
            }
            std::vector<float> probs(k);
            float sum = 0.0f;
            for (int i = 0; i < k; ++i) {
                probs[i] = std::exp((candidates[i].first - max_logit) / temperature);
                sum += probs[i];
            }
            if (sum <= 0.0f || std::isnan(sum) || std::isinf(sum)) {
                int best_idx = 0;
                float best_val = candidates[0].first;
                for (int i = 1; i < k; ++i) {
                    if (candidates[i].first > best_val) {
                        best_val = candidates[i].first;
                        best_idx = i;
                    }
                }
                next_token = candidates[best_idx].second;
            } else {
                for (int i = 0; i < k; ++i) {
                    probs[i] /= sum;
                }
                // Sample from the distribution
                std::discrete_distribution<int> dist(probs.begin(), probs.end());
                int sampled_idx = dist(rng);
                next_token = candidates[sampled_idx].second;
            }
        }
        
        // Stop on EOS tokens before streaming them (model-driven detection)
        bool is_eos = (next_token == model.config.eos_token_id);
        if (!is_eos && next_token >= 0 && next_token < (int)model.vocab.tokens.size()) {
            const std::string& tok_str = model.vocab.tokens[next_token];
            if (tok_str == "<|im_end|>" || tok_str == "<|endoftext|>" || tok_str == "</s>" || 
                tok_str == "<eos>" || tok_str == "<|im_start|>" || tok_str == "<|eot_id|>") {
                is_eos = true;
            }
        }
        if (is_eos) {
            break;
        }
        
        generated_tokens.push_back(next_token);
        if (session_kv) {
            session_kv->cached_tokens.push_back(next_token);
        }
        
        std::string text = detokenize(model.vocab, next_token);
        callback(text);
        
        state.current_pos++;
        current_token = next_token;
    }
}

std::vector<float> compute_embedding(DenseModel& model, const std::vector<int>& tokens) {
    if (tokens.empty()) return {};

    InferenceState state;
    int ctx = std::max(static_cast<int>(tokens.size()) + 16, 256);
    init_inference_state(model.config, ctx, state);

    std::vector<float> logits(model.config.vocab_size > 0 ? model.config.vocab_size : 1);
    std::vector<float> pooled(model.config.embedding_length, 0.0f);

    for (int t : tokens) {
        if (!forward_pass(model, state, t, logits)) {
            std::cerr << "[Infer] Embedding aborted due to forward pass failure." << std::endl;
            return {};
        }
        for (size_t i = 0; i < model.config.embedding_length; ++i) {
            pooled[i] += state.x[i];
        }
        state.current_pos++;
    }

    // Mean pooling & L2 normalization
    double norm_sq = 0.0;
    float inv_tokens = 1.0f / static_cast<float>(tokens.size());
    for (size_t i = 0; i < model.config.embedding_length; ++i) {
        pooled[i] *= inv_tokens;
        norm_sq += static_cast<double>(pooled[i]) * pooled[i];
    }

    if (norm_sq > 1e-9) {
        float inv_norm = static_cast<float>(1.0 / std::sqrt(norm_sq));
        for (size_t i = 0; i < model.config.embedding_length; ++i) {
            pooled[i] *= inv_norm;
        }
    }
    return pooled;
}

