#pragma once

#include "model.hpp"
#include <vector>
#include <functional>

// ============================================================================
// State and Inference Logic
// ============================================================================

// Holds the activations and KV cache for a single generation step
struct InferenceState {
    std::vector<float> x;
    std::vector<std::vector<float>> k_cache;
    std::vector<std::vector<float>> v_cache;
    std::vector<float> inv_freq;
    
    int current_pos = 0;
};

// Initializes the inference state based on the model config
void init_inference_state(const ModelConfig& config, int max_context, InferenceState& state);

// Tokenization
std::vector<int> tokenize(const Vocab& vocab, const std::string& text);
std::string detokenize(const Vocab& vocab, int token_id);

// The core forward pass (Transformer Decoder)
// Takes the input token ID and returns the logits for the next token prediction
void forward_pass(DenseModel& model, InferenceState& state, int token_id, std::vector<float>& logits);

// Generate text (sampling loop) with a streaming callback
using StreamCallback = std::function<void(const std::string&)>;
void generate(DenseModel& model, const std::vector<int>& prompt_tokens, StreamCallback callback,
              int max_tokens = 512, float temperature = 0.7f, float repetition_penalty = 1.15f);
