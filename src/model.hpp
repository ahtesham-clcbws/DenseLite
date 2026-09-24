#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

// ============================================================================
// GGUF Block Structures (Low-Level Memory Layout)
// ============================================================================

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
    Q8_0 = 8
};

struct Tensor {
    std::string name;
    TensorType type;
    std::vector<uint32_t> shape; // e.g., [out_features, in_features]
    size_t data_offset;          // Offset into the mmap'd payload
    void* data;                  // Aligned pointer (validated to 32-byte boundary)
};

struct ModelConfig {
    uint32_t context_length;
    uint32_t embedding_length;
    uint32_t num_layers;
    uint32_t num_heads;
    uint32_t num_kv_heads;
    uint32_t head_dim;
    uint32_t vocab_size;
    float rms_norm_eps;
    uint32_t alignment; // Defaults to 32 in GGUF
};

// Tokenizer Trie Node
struct TrieNode {
    int token_id = -1;
    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
};

struct Vocab {
    std::vector<std::string> tokens;
    std::vector<float> scores;
    std::unique_ptr<TrieNode> root;
    
    Vocab() : root(std::make_unique<TrieNode>()) {}
};

// Represents the entire loaded model
struct DenseModel {
    ModelConfig config;
    Vocab vocab;
    std::unordered_map<std::string, Tensor> tensors;
    
    // The raw mmap'd file pointer and size
    void* mmap_data;
    size_t mmap_size;
};
