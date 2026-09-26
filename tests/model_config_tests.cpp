#include "../src/gguf_parser.hpp"
#include <iostream>
#include <cassert>

bool test_model_config(const std::string& name, const std::string& path,
                       uint32_t exp_embd, uint32_t exp_heads, uint32_t exp_kv_heads,
                       uint32_t exp_head_dim, uint32_t exp_intermediate,
                       uint32_t exp_layers, uint32_t exp_vocab, float exp_rope_base) {
    std::cout << "\nTesting ModelConfig for: " << name << "\n";
    DenseModel model;
    if (!load_gguf_model(path, model)) {
        std::cerr << "FAIL: Could not load " << path << "\n";
        return false;
    }

    const auto& c = model.config;
    std::cout << "  Architecture:     " << c.architecture << "\n";
    std::cout << "  Embedding Length: " << c.embedding_length << " (expected " << exp_embd << ")\n";
    std::cout << "  Heads:            " << c.num_heads << " (expected " << exp_heads << ")\n";
    std::cout << "  KV Heads:         " << c.num_kv_heads << " (expected " << exp_kv_heads << ")\n";
    std::cout << "  Head Dim:         " << c.head_dim << " (expected " << exp_head_dim << ")\n";
    std::cout << "  Intermediate Dim: " << c.intermediate_dim << " (expected " << exp_intermediate << ")\n";
    std::cout << "  Layers:           " << c.num_layers << " (expected " << exp_layers << ")\n";
    std::cout << "  Vocab Size:       " << c.vocab_size << " (expected " << exp_vocab << ")\n";
    std::cout << "  RoPE Base:        " << c.rope.base << " (expected " << exp_rope_base << ")\n";

    bool pass = true;
    if (c.embedding_length != exp_embd) { std::cerr << "  MISMATCH: embedding_length\n"; pass = false; }
    if (c.num_heads != exp_heads) { std::cerr << "  MISMATCH: num_heads\n"; pass = false; }
    if (c.num_kv_heads != exp_kv_heads) { std::cerr << "  MISMATCH: num_kv_heads\n"; pass = false; }
    if (c.head_dim != exp_head_dim) { std::cerr << "  MISMATCH: head_dim\n"; pass = false; }
    if (c.intermediate_dim != exp_intermediate) { std::cerr << "  MISMATCH: intermediate_dim\n"; pass = false; }
    if (c.num_layers != exp_layers) { std::cerr << "  MISMATCH: num_layers\n"; pass = false; }
    if (c.vocab_size != exp_vocab) { std::cerr << "  MISMATCH: vocab_size\n"; pass = false; }
    if (std::abs(c.rope.base - exp_rope_base) > 1.0f) { std::cerr << "  MISMATCH: rope.base\n"; pass = false; }

    free_gguf_model(model);
    if (pass) {
        std::cout << "  -> PASS: All ModelConfig dimensions match expectations!\n";
    }
    return pass;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "[TEST] Phase 1: ModelConfig & Tensor-Shape Validation\n";
    std::cout << "========================================\n";

    std::string base = "/mnt/apollo/Apollo4/DenseLite/models/";

    bool smollm_ok = test_model_config(
        "SmolLM2-360M", base + "SmolLM2-360M-Instruct-Q8_0.gguf",
        960, 15, 5, 64, 2560, 32, 49152, 100000.0f
    );

    bool coder_ok = test_model_config(
        "Qwen2.5-Coder-1.5B", base + "Qwen2.5-Coder-1.5B-Instruct-abliterated-Q8_0.gguf",
        1536, 12, 2, 128, 8960, 28, 151936, 1000000.0f
    );

    bool main_ok = test_model_config(
        "Qwen2.5-1.5B-Main", base + "Qwen2.5-1.5B-Instruct-abliterated.Q8_0.gguf",
        1536, 12, 2, 128, 8960, 28, 151936, 1000000.0f
    );

    if (smollm_ok && coder_ok && main_ok) {
        std::cout << "\n>>> ALL MODEL CONFIG TESTS PASSED! <<<\n";
        return 0;
    } else {
        std::cerr << "\n>>> FAIL: One or more ModelConfig validations failed! <<<\n";
        return 1;
    }
}
