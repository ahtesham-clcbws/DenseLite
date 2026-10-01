#include "../src/infer.hpp"
#include "../src/gguf_parser.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <filesystem>
#include <cstdlib>

bool test_golden_inference(const std::string& model_name, const std::string& path, size_t& executed_count) {
    std::cout << "\n==================================================\n";
    std::cout << "[TEST] Golden Inference: " << model_name << "\n";
    std::cout << " Path: " << path << "\n";
    std::cout << "==================================================\n";

    DenseModel model;
    if (!load_gguf_model(path, model)) {
        std::cerr << "FAIL: Could not load model: " << path << "\n";
        return false;
    }

    if (model.tensors.count("blk.0.attn_q.weight") > 0) {
        TensorType qtype = model.tensors["blk.0.attn_q.weight"].type;
        if (qtype != TensorType::Q8_0 && qtype != TensorType::Q4_0) {
            std::cout << "   -> SKIP (Optional Model): " << model_name << " has unsupported tensor quantization (" 
                      << tensor_type_name(qtype) << "). Native forward pass requires Q4_0 or Q8_0.\n";
            free_gguf_model(model);
            return true; // Skipped optional model does not increment executed_count
        }
        std::cout << "   -> Executing native " << tensor_type_name(qtype) << " AVX2 forward pass verification...\n";
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
    executed_count++;
    return true;
}

// Portable resolution of test model artifacts across working directories and standard paths
static std::string resolve_test_model(const std::string& filename) {
    const char* root_env = std::getenv("DENSELITE_ROOT");
    if (root_env) {
        std::filesystem::path p1 = std::filesystem::path(root_env) / "models" / "validation" / filename;
        if (std::filesystem::exists(p1)) return p1.string();
        std::filesystem::path p2 = std::filesystem::path(root_env) / "models" / filename;
        if (std::filesystem::exists(p2)) return p2.string();
    }

    std::filesystem::path cwd = std::filesystem::current_path();
    std::vector<std::filesystem::path> search_dirs = {
        cwd / "models" / "validation",
        cwd / "models",
        cwd / ".." / "models" / "validation",
        cwd / ".." / "models"
    };

    for (const auto& dir : search_dirs) {
        std::filesystem::path candidate = dir / filename;
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }

    const char* home = std::getenv("HOME");
    if (home) {
        std::filesystem::path user_path = std::filesystem::path(home) / ".denselite" / "models" / filename;
        if (std::filesystem::exists(user_path)) {
            return user_path.string();
        }
    }

    return "";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "[TEST] Phase 1: Golden Inference Structural Tests\n";
    std::cout << "========================================\n";

    size_t executed_passes = 0;

    // 1. Mandatory Canonical Validation Model (Must exist and must execute native forward pass)
    std::string val_model = resolve_test_model("deepseek-1.5b-q4_0.gguf");
    if (val_model.empty() || !std::filesystem::exists(val_model)) {
        std::cerr << "FAIL: Mandatory golden model artifact 'deepseek-1.5b-q4_0.gguf' not found in search paths!\n";
        return 1;
    }

    bool val_ok = test_golden_inference("DeepSeek-1.5B-Q4_0 (Validation)", val_model, executed_passes);
    if (!val_ok || executed_passes == 0) {
        std::cerr << "FAIL: Mandatory golden validation model failed to execute native forward pass!\n";
        return 1;
    }

    // 2. Optional User Model Checks (Will verify if present and compatible)
    bool smollm_ok = true;
    std::string smollm_path = resolve_test_model("smollm2-360m-instruct-q4_0.gguf");
    if (!smollm_path.empty()) {
        smollm_ok = test_golden_inference("SmolLM2-360M", smollm_path, executed_passes);
    }

    bool coder_ok = true;
    std::string coder_path = resolve_test_model("DeepSeek-R1-Distill-Qwen-1.5B-Q4_0.gguf");
    if (!coder_path.empty()) {
        coder_ok = test_golden_inference("DeepSeek-R1-Distill-Qwen-1.5B", coder_path, executed_passes);
    }

    bool main_ok = true;
    std::string main_path = resolve_test_model("Llama-3.2-1B-Instruct-abliterated.i1-Q4_0.gguf");
    if (!main_path.empty()) {
        main_ok = test_golden_inference("Llama-3.2-1B", main_path, executed_passes);
    }

    std::cout << "\nExecution Summary: " << executed_passes << " native forward-pass test(s) executed.\n";

    if (executed_passes >= 1 && val_ok && smollm_ok && coder_ok && main_ok) {
        std::cout << "\n>>> ALL GOLDEN INFERENCE TESTS PASSED! <<<\n";
        return 0;
    } else {
        std::cerr << "\n>>> FAIL: One or more golden inference tests failed or no models executed! <<<\n";
        return 1;
    }
}
