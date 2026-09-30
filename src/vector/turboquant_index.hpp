#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <mutex>

struct TurboQuantHit {
    std::string id;
    uint32_t internal_idx = 0;
    float score = 0.0f;
};

struct QuantizedVector {
    float scale = 1.0f;
    float offset = 0.0f;
    std::vector<uint8_t> packed; // dim / 2 bytes (4-bit Lloyd-Max)
    bool active = true;
};

// TurboQuantIndex - SIMD-accelerated exhaustive scan vector database.
// 
// [ARCHITECTURE ROADMAP - v4.0+]
// 1. Matrix Scaling: Currently uses a full dim x dim rotation matrix. 
//    Future: Transition to structured block rotation (e.g., HD) or sparse orthogonal transforms for >1024 dims.
// 2. Recall Benchmarks: Must benchmark against exact FP32/FP16 cosine search.
// 3. Scale Tests & Partitioning: Current SIMD exhaustive scan is optimal up to ~1M vectors.
//    Future: Implement coarse quantization (IVF) or HNSW for >1M vector retrieval.

class TurboQuantIndex {
public:
    explicit TurboQuantIndex(size_t dim = 128, uint32_t seed = 42);
    ~TurboQuantIndex() = default;

    // Insert or update dense vector in index (automatically rotated & 4-bit quantized)
    void add(const std::string& id, const std::vector<float>& vec);

    // Soft-deletes vector by external string ID
    bool remove(const std::string& id);

    // Queries top-k most similar vectors with in-kernel bitmask filtering
    std::vector<TurboQuantHit> search(
        const std::vector<float>& query_vec,
        size_t top_k = 10,
        const uint64_t* allowlist_mask = nullptr) const;

    // Build a 64-bit aligned allowlist mask for a subset of IDs
    std::vector<uint64_t> build_allowlist_mask(const std::vector<std::string>& allowed_ids) const;

    size_t size() const;
    size_t dimension() const { return dim_; }
    size_t memory_bytes() const;

    // Crash-safe atomic binary persistence (DLVQ format)
    bool save(const std::string& filepath) const;
    bool load(const std::string& filepath);

private:
    void init_rotation_matrix(uint32_t seed);
    void rotate_vector(const float* in, float* out) const;
    void quantize_rotated(const float* rotated, QuantizedVector& qv) const;

    size_t dim_;
    std::vector<float> rotation_matrix_; // dim * dim orthogonal projection
    std::vector<QuantizedVector> vectors_;
    std::vector<std::string> id_by_index_;
    std::unordered_map<std::string, uint32_t> index_by_id_;
    mutable std::mutex mtx_;
};
