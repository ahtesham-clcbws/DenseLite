#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

// ============================================================================
// GGUF Block Structures (Low-Level Memory Layout)
// ============================================================================

#pragma pack(push, 1)
struct block_q4_0 {
    uint16_t d;     // 16-bit float (FP16) scaling factor
    uint8_t qs[16]; // 32 quantized 4-bit weights packed into 16 bytes
};
#pragma pack(pop)

static_assert(sizeof(block_q4_0) == 18, "block_q4_0 must be exactly 18 bytes");

// A Q8_0 block in GGUF is exactly 34 bytes.
// It contains a 16-bit float (FP16) scaling factor 'd',
// followed by 32 quantized 8-bit integers.
// We align it to 2 bytes since the largest scalar member is a 16-bit float.
#pragma pack(push, 1)
struct block_q8_0 {
    // 16-bit float (represented as uint16_t). We must convert this to FP32 in AVX2.
    uint16_t d; 
    
    // 32 quantized weights.
    int8_t qs[32]; 
};
#pragma pack(pop)

static_assert(sizeof(block_q8_0) == 34, "block_q8_0 must be exactly 34 bytes");

// ============================================================================
// Tensor and Model Metadata
// ============================================================================

enum class TensorType {
    FP32 = 0,
    FP16 = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    Q2_K = 10,
    Q3_K = 11,
    Q4_K = 12,
    Q5_K = 13,
    Q6_K = 14,
    Q8_K = 15
};

struct Tensor {
    std::string name;
    TensorType type;
    std::vector<uint32_t> shape; // e.g., [out_features, in_features]
    size_t data_offset;          // Offset into the mmap'd payload
    void* data;                  // Aligned pointer (validated to 32-byte boundary)
};

// Explicit RoPE Configuration struct (Phase 1 / Amendment 2)
struct RopeConfig {
    float base = 10000.0f;           // e.g. 10000.0 or 1000000.0
    float scale = 1.0f;              // default 1.0
    std::string type = "default";    // "default", "linear", "yarn", etc.
    float frequency_factor = 0.0f;   // for extended context variants
    float low_freq_factor = 0.0f;    // YaRN low-frequency cutoff
    float high_freq_factor = 0.0f;   // YaRN high-frequency cutoff
};

struct ModelConfig {
    std::string architecture;        // e.g. "qwen2", "llama"
    std::string model_id;            // e.g. "qwen2.5-0.5B"
    uint32_t context_length = 0;
    uint32_t embedding_length = 0;
    uint32_t num_layers = 0;
    uint32_t num_heads = 0;
    uint32_t num_kv_heads = 0;
    uint32_t head_dim = 0;
    uint32_t intermediate_dim = 0;   // Feed-forward hidden dimension (feed_forward_length)
    uint32_t vocab_size = 0;
    int eos_token_id = -1;           // Dynamic EOS token ID
    float rms_norm_eps = 1e-6f;
    uint32_t alignment = 32;         // Defaults to 32 in GGUF
    std::string chat_template;
    RopeConfig rope;
};

// Tokenizer Trie Node
struct TrieNode {
    int token_id = -1;
    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
};

struct Vocab {
    std::vector<std::string> tokens;
    std::vector<float> scores;
    std::shared_ptr<TrieNode> root;
    
    Vocab() : root(std::make_shared<TrieNode>()) {}
};

#include <sys/mman.h>

struct MmapBuffer {
    void* addr = nullptr;
    size_t size = 0;
    ~MmapBuffer() {
        if (addr && addr != MAP_FAILED) {
            munmap(addr, size);
        }
    }
};

enum class DeviceContext {
    CPU,
    GPU
};

// Represents the entire loaded model
struct DenseModel {
    ModelConfig config;
    Vocab vocab;
    std::unordered_map<std::string, Tensor> tensors;
    
    // The raw mmap'd file pointer and size
    void* mmap_data = nullptr;
    size_t mmap_size = 0;
    std::shared_ptr<MmapBuffer> mmap_buffer;

    DeviceContext execution_context = DeviceContext::CPU;

    DenseModel create_shared_reference() const {
        DenseModel ref;
        ref.config = this->config;
        ref.vocab = this->vocab;
        ref.tensors = this->tensors;
        ref.mmap_data = this->mmap_data;
        ref.mmap_size = this->mmap_size;
        ref.mmap_buffer = this->mmap_buffer;
        ref.execution_context = this->execution_context;
        return ref;
    }
};
