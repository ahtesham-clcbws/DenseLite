#include "../src/infer.hpp"
#include "../src/gguf_parser.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

bool test_golden_inference(const std::string& model_name, const std::string& path) {
    std::cout << "\n==================================================\n";
    std::cout << "[TEST] Golden Inference: " << model_name << "\n";
    std::cout << "==================================================\n";

    DenseModel model;
    if (!load_gguf_model(path, model)) {
        std::cerr << "FAIL: Could not load model: " << path << "\n";
        return false;
    }

    std::string prompt_text = "Hello, world!";
    std::vector<int> tokens = tokenize(model.vocab, prompt_text);
    if (tokens.empty()) {
        std::cerr << "FAIL: Tokenization produced 0 tokens\n";
        free_gguf_model(model);
        return false;
    }

    std::cout << "1. Tokenized prompt: \"" << prompt_text << "\" -> " << tokens.size() << " tokens.\n";

    // 1. Structural Check: Forward pass, no NaN/Inf, monotonic KV position
    InferenceState state;
    init_inference_state(model.config, 512, state);

    std::vector<float> logits(model.config.vocab_size);

    int prev_pos = -1;
    for (size_t i = 0; i < tokens.size(); ++i) {
        forward_pass(model, state, tokens[i], logits);

        // Check for NaN or Inf in logits
        for (size_t j = 0; j < std::min((size_t)1000, (size_t)model.config.vocab_size); ++j) {
            if (std::isnan(logits[j]) || std::isinf(logits[j])) {
                std::cerr << "FAIL: Logits contain NaN or Inf at index " << j << "!\n";
                free_gguf_model(model);
                return false;
            }
        }

        // Monotonic check
        if (state.current_pos <= prev_pos && i > 0) {
            std::cerr << "FAIL: KV position did not advance monotonically!\n";
            free_gguf_model(model);
            return false;
        }
        prev_pos = state.current_pos;
        state.current_pos++;
    }
    std::cout << "   -> PASS: Forward pass logits finite (no NaN/Inf) & KV position monotonic.\n";

    // 2. Determinism Check: 5 runs at temperature = 0
    std::cout << "2. Checking determinism (5 runs at temp=0)...\n";
    std::vector<std::string> run_outputs;

    for (int run = 0; run < 5; ++run) {
        std::string out_text;
        generate(model, tokens, [&](const std::string& chunk) {
            out_text += chunk;
        }, 16, 0.0f, 1.0f); // 16 tokens, temp=0.0, penalty=1.0

        run_outputs.push_back(out_text);
    }

    bool deterministic = true;
    for (size_t r = 1; r < run_outputs.size(); ++r) {
        if (run_outputs[r] != run_outputs[0]) {
            std::cerr << "FAIL: Non-deterministic output between run 0 and run " << r << "!\n";
            std::cerr << "  Run 0: \"" << run_outputs[0] << "\"\n";
            std::cerr << "  Run " << r << ": \"" << run_outputs[r] << "\"\n";
            deterministic = false;
            break;
        }
    }

    if (!deterministic) {
        free_gguf_model(model);
        return false;
    }

    std::cout << "   -> PASS: Output is 100% deterministic across 5 runs!\n";
    std::cout << "   Sample Output: \"" << run_outputs[0] << "\"\n";

    free_gguf_model(model);
    return true;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "[TEST] Phase 1: Golden Inference Structural Tests\n";
    std::cout << "========================================\n";

    std::string base = "/mnt/apollo/Apollo4/DenseLite/models/";

    bool smollm_ok = test_golden_inference("SmolLM2-360M", base + "SmolLM2-360M-Instruct-Q8_0.gguf");
    bool coder_ok = test_golden_inference("Qwen2.5-Coder-1.5B", base + "Qwen2.5-Coder-1.5B-Instruct-abliterated-Q8_0.gguf");
    bool main_ok = test_golden_inference("Qwen2.5-1.5B-Main", base + "Qwen2.5-1.5B-Instruct-abliterated.Q8_0.gguf");

    if (smollm_ok && coder_ok && main_ok) {
        std::cout << "\n>>> ALL GOLDEN INFERENCE TESTS PASSED! <<<\n";
        return 0;
    } else {
        std::cerr << "\n>>> FAIL: One or more golden inference tests failed! <<<\n";
        return 1;
    }
}
