#include "infer.hpp"
#include "avx2_math.hpp"
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
        
        // Replace all instances of Ġ (0xC4 0xA0) with space
        size_t pos = 0;
        while ((pos = s.find("\xC4\xA0", pos)) != std::string::npos) {
            s.replace(pos, 2, " ");
            pos += 1;
        }
        
        // Replace all instances of Ċ (0xC4 0x8A) with newline
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
    
    state.inv_freq.resize(config.head_dim / 2);
    float base = 1000000.0f; // Qwen2.5 base
    for (int i = 0; i < (int)config.head_dim; i += 2) {
        state.inv_freq[i / 2] = 1.0f / std::pow(base, (float)i / config.head_dim);
    }
    
    std::cout << "[Infer] Initializing KV Cache for max context: " << max_context << std::endl;
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

void matvec_q8(const Tensor& w, const float* x, float* out, int in_features, int out_features) {
    int nb = in_features / 32;
    const block_q8_0* w_data = (const block_q8_0*)w.data;
    #pragma omp parallel for
    for (int i = 0; i < out_features; ++i) {
        out[i] = math::dot_product_q8_fp32(&w_data[i * nb], x, nb);
    }
}

void forward_pass(DenseModel& model, InferenceState& state, int token_id, std::vector<float>& logits) {
    auto& config = model.config;
    
    // 1. Token Embedding Lookup (Q8_0 dequantize)
    if (model.tensors.count("token_embd.weight") == 0) {
        std::cerr << "Missing token_embd.weight" << std::endl;
        return;
    }
    
    const block_q8_0* embd_data = (const block_q8_0*)model.tensors["token_embd.weight"].data;
    int embd_blocks = config.embedding_length / 32;
    dequantize_q8_row(&embd_data[token_id * embd_blocks], state.x.data(), embd_blocks);
    
    // Qwen2.5 1.5B specific dims
    int head_dim = config.head_dim;       // 128
    int num_kv_features = config.num_kv_heads * head_dim; // 2 * 128 = 256
    int mlp_hidden_dim = 8960;
    int kv_groups = config.num_heads / config.num_kv_heads; // 12 / 2 = 6
    
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
        math::rmsnorm(state.x.data(), state.x.data(), config.embedding_length,
                      config.rms_norm_eps,
                      (float*)model.tensors[lp + "attn_norm.weight"].data);
        
        // Q, K, V Projections (matvec)
        matvec_q8(model.tensors[lp + "attn_q.weight"], state.x.data(), q.data(),
                  config.embedding_length, config.embedding_length);
        matvec_q8(model.tensors[lp + "attn_k.weight"], state.x.data(), k.data(),
                  config.embedding_length, num_kv_features);
        matvec_q8(model.tensors[lp + "attn_v.weight"], state.x.data(), v.data(),
                  config.embedding_length, num_kv_features);
        
        // Add biases ONCE
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
        
        // Apply RoPE (Standard/type-0: consecutive pairs (x0,x1),(x2,x3)...)
        // GGUF conversion de-interleaves Q/K weights so standard RoPE is correct
        math::rope(q.data(), state.current_pos, config.num_heads, head_dim, state.inv_freq.data());
        math::rope(k.data(), state.current_pos, config.num_kv_heads, head_dim, state.inv_freq.data());
        
        // -----------------------------------------------------------------
        // GQA KV CACHE ATTENTION
        // -----------------------------------------------------------------
        int pos = state.current_pos;
        
        // Store K and V into cache at current position
        int cache_offset = pos * num_kv_features;
        std::memcpy(&state.k_cache[l][cache_offset], k.data(), num_kv_features * sizeof(float));
        std::memcpy(&state.v_cache[l][cache_offset], v.data(), num_kv_features * sizeof(float));
        
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
        math::rmsnorm(state.x.data(), state.x.data(), config.embedding_length,
                      config.rms_norm_eps,
                      (float*)model.tensors[lp + "ffn_norm.weight"].data);
        
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
    math::rmsnorm(state.x.data(), state.x.data(), config.embedding_length,
                  config.rms_norm_eps,
                  (float*)model.tensors["output_norm.weight"].data);
    
    // 4. LM Head (Vocab Projection)
    // Some models (e.g. Qwen2.5-1.5B, SmolLM2) tie word embeddings and omit output.weight
    const Tensor& output_weight = model.tensors.count("output.weight")
        ? model.tensors.at("output.weight")
        : model.tensors.at("token_embd.weight");
    matvec_q8(output_weight, state.x.data(), logits.data(),
              config.embedding_length, config.vocab_size);
}

void generate(DenseModel& model, const std::vector<int>& prompt_tokens, StreamCallback callback,
              int max_tokens, float temperature, float repetition_penalty) {
    InferenceState state;
    int base_ctx_len = model.config.context_length > 0 ? model.config.context_length : 4096;
    int ctx_len = std::min(base_ctx_len, 8192); // Cap at 8192 to prevent OOM
    init_inference_state(model.config, ctx_len, state);
    
    std::vector<float> logits(model.config.vocab_size);
    
    if (prompt_tokens.empty()) return;
    
    // RNG for sampling
    std::mt19937 rng(std::random_device{}());
    
    // Track generated tokens for repetition penalty
    std::vector<int> generated_tokens;
    generated_tokens.reserve(max_tokens);
    
    // 1. Prefill: process all prompt tokens except the last
    for (size_t i = 0; i < prompt_tokens.size() - 1; ++i) {
        forward_pass(model, state, prompt_tokens[i], logits);
        state.current_pos++;
    }
    
    // 2. Start generation from the last prompt token
    int current_token = prompt_tokens.back();
    
    constexpr int TOP_K = 40;
    std::vector<std::pair<float, int>> candidates(model.config.vocab_size);
    
    constexpr int QWEN_EOS_TOKEN   = 151643;
    constexpr int QWEN_IM_START    = 151644;
    constexpr int QWEN_IM_END      = 151645;

    
    for (int step = 0; step < max_tokens; ++step) {
        forward_pass(model, state, current_token, logits);
        
        // --- Repetition Penalty ---
        // Multiplicative penalty: divide logits of previously seen tokens
        // if logit > 0, divide by penalty; if logit < 0, multiply by penalty
        if (repetition_penalty != 1.0f) {
            for (int tok : generated_tokens) {
                if (tok >= 0 && tok < (int)model.config.vocab_size) {
                    if (logits[tok] > 0.0f) {
                        logits[tok] /= repetition_penalty;
                    } else {
                        logits[tok] *= repetition_penalty;
                    }
                }
            }
        }
        
        int next_token;
        
        if (temperature <= 0.01f) {
            // Pure greedy (temperature ≈ 0)
            next_token = 0;
            float max_val = logits[0];
            for (int i = 1; i < (int)model.config.vocab_size; ++i) {
                if (logits[i] > max_val) {
                    max_val = logits[i];
                    next_token = i;
                }
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
            float max_logit = candidates[0].first;
            std::vector<float> probs(k);
            float sum = 0.0f;
            for (int i = 0; i < k; ++i) {
                probs[i] = std::exp((candidates[i].first - max_logit) / temperature);
                sum += probs[i];
            }
            for (int i = 0; i < k; ++i) {
                probs[i] /= sum;
            }
            
            // Sample from the distribution
            std::discrete_distribution<int> dist(probs.begin(), probs.end());
            int sampled_idx = dist(rng);
            next_token = candidates[sampled_idx].second;
        }
        
        // Stop on EOS tokens before streaming them
        if (next_token == QWEN_EOS_TOKEN || next_token == QWEN_IM_END || next_token == QWEN_IM_START) {
            break;
        }
        
        generated_tokens.push_back(next_token);
        
        std::string text = detokenize(model.vocab, next_token);
        callback(text);
        
        state.current_pos++;
        current_token = next_token;
    }
}
