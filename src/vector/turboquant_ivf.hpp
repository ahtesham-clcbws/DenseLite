#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include "turboquant_index.hpp"

struct IVFCluster {
    std::vector<float> centroid; // FP32 centroid (dim)
    std::vector<uint32_t> member_indices; // Global vector indices assigned to this cluster
};

class TurboQuantIVF {
public:
    explicit TurboQuantIVF(size_t dim = 128, size_t num_clusters = 16, uint32_t seed = 42);
    ~TurboQuantIVF() = default;

    // Add dense vector (automatically assigned to closest centroid posting list)
    void add(const std::string& id, const std::vector<float>& vec);

    // Soft-deletes vector by ID
    bool remove(const std::string& id);

    // Sub-linear query: probes top `nprobe` candidate clusters
    std::vector<TurboQuantHit> search(
        const std::vector<float>& query_vec,
        size_t top_k = 10,
        size_t nprobe = 4,
        const uint64_t* allowlist_mask = nullptr) const;

    // Train coarse centroids using spherical k-means on sample vectors
    void train(const std::vector<std::vector<float>>& training_vectors, size_t max_iters = 10);
    bool is_trained() const { return is_trained_; }

    size_t size() const;
    size_t dimension() const { return dim_; }
    size_t num_clusters() const { return clusters_.size(); }
    size_t memory_bytes() const;

private:
    void init_centroids(uint32_t seed);
    size_t find_closest_centroid(const float* vec) const;
    std::vector<size_t> find_top_centroids(const float* vec, size_t nprobe) const;

    size_t dim_;
    size_t num_clusters_;
    bool is_trained_ = false;
    TurboQuantIndex base_index_; // Flat rotated quantized storage
    std::vector<IVFCluster> clusters_; // Coarse partition posting lists
    std::unordered_map<std::string, size_t> cluster_assignment_; // ID -> cluster index
    mutable std::mutex mtx_;
};
