#include "turboquant_ivf.hpp"
#include <immintrin.h>
#include <cmath>
#include <random>
#include <algorithm>
#include <queue>

TurboQuantIVF::TurboQuantIVF(size_t dim, size_t num_clusters, uint32_t seed)
    : dim_(dim), num_clusters_(std::max<size_t>(2, num_clusters)), base_index_(dim, seed) {
    init_centroids(seed);
}

void TurboQuantIVF::init_centroids(uint32_t seed) {
    clusters_.resize(num_clusters_);
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (size_t c = 0; c < num_clusters_; ++c) {
        clusters_[c].centroid.resize(dim_);
        float norm_sq = 0.0f;
        for (size_t d = 0; d < dim_; ++d) {
            float val = dist(rng);
            clusters_[c].centroid[d] = val;
            norm_sq += val * val;
        }
        float inv_norm = norm_sq > 1e-8f ? 1.0f / std::sqrt(norm_sq) : 0.0f;
        for (size_t d = 0; d < dim_; ++d) {
            clusters_[c].centroid[d] *= inv_norm;
        }
    }
}

size_t TurboQuantIVF::find_closest_centroid(const float* vec) const {
    size_t best_c = 0;
    float best_sim = -1e9f;

    for (size_t c = 0; c < clusters_.size(); ++c) {
        const float* cent = clusters_[c].centroid.data();
        float dot = 0.0f;
        size_t d = 0;
        __m256 sum_vec = _mm256_setzero_ps();
        for (; d + 8 <= dim_; d += 8) {
            __m256 v = _mm256_loadu_ps(&vec[d]);
            __m256 k = _mm256_loadu_ps(&cent[d]);
            sum_vec = _mm256_fmadd_ps(v, k, sum_vec);
        }
        alignas(32) float hsum[8];
        _mm256_store_ps(hsum, sum_vec);
        dot = hsum[0] + hsum[1] + hsum[2] + hsum[3] + hsum[4] + hsum[5] + hsum[6] + hsum[7];
        for (; d < dim_; ++d) {
            dot += vec[d] * cent[d];
        }

        if (dot > best_sim) {
            best_sim = dot;
            best_c = c;
        }
    }
    return best_c;
}

std::vector<size_t> TurboQuantIVF::find_top_centroids(const float* vec, size_t nprobe) const {
    nprobe = std::min(nprobe, clusters_.size());
    std::vector<std::pair<float, size_t>> scored(clusters_.size());

    for (size_t c = 0; c < clusters_.size(); ++c) {
        const float* cent = clusters_[c].centroid.data();
        float dot = 0.0f;
        size_t d = 0;
        __m256 sum_vec = _mm256_setzero_ps();
        for (; d + 8 <= dim_; d += 8) {
            __m256 v = _mm256_loadu_ps(&vec[d]);
            __m256 k = _mm256_loadu_ps(&cent[d]);
            sum_vec = _mm256_fmadd_ps(v, k, sum_vec);
        }
        alignas(32) float hsum[8];
        _mm256_store_ps(hsum, sum_vec);
        dot = hsum[0] + hsum[1] + hsum[2] + hsum[3] + hsum[4] + hsum[5] + hsum[6] + hsum[7];
        for (; d < dim_; ++d) {
            dot += vec[d] * cent[d];
        }
        scored[c] = {dot, c};
    }

    std::partial_sort(scored.begin(), scored.begin() + nprobe, scored.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<size_t> top_clusters(nprobe);
    for (size_t i = 0; i < nprobe; ++i) {
        top_clusters[i] = scored[i].second;
    }
    return top_clusters;
}

void TurboQuantIVF::add(const std::string& id, const std::vector<float>& vec) {
    std::lock_guard<std::mutex> lock(mtx_);
    uint32_t internal_idx = static_cast<uint32_t>(base_index_.size());
    base_index_.add(id, vec);

    std::vector<float> padded_v(dim_, 0.0f);
    size_t copy_len = std::min(vec.size(), dim_);
    std::copy_n(vec.begin(), copy_len, padded_v.begin());

    size_t closest_cluster = find_closest_centroid(padded_v.data());
    clusters_[closest_cluster].member_indices.push_back(internal_idx);
    cluster_assignment_[id] = closest_cluster;
}

bool TurboQuantIVF::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx_);
    return base_index_.remove(id);
}

std::vector<TurboQuantHit> TurboQuantIVF::search(
    const std::vector<float>& query_vec,
    size_t top_k,
    size_t nprobe,
    const uint64_t* allowlist_mask) const {
    std::lock_guard<std::mutex> lock(mtx_);
    size_t total_vecs = base_index_.size();
    if (total_vecs == 0 || top_k == 0) return {};

    // For small collections (< 500 vectors), flat exhaustive scan is faster than cluster partitioning
    if (total_vecs < 500 || nprobe >= clusters_.size()) {
        return base_index_.search(query_vec, top_k, allowlist_mask);
    }

    std::vector<float> padded_q(dim_, 0.0f);
    size_t copy_len = std::min(query_vec.size(), dim_);
    std::copy_n(query_vec.begin(), copy_len, padded_q.begin());

    std::vector<size_t> top_clusters = find_top_centroids(padded_q.data(), nprobe);

    // Build bitmask for member indices in probed clusters
    size_t mask_words = (total_vecs + 63) / 64;
    std::vector<uint64_t> cluster_mask(mask_words, 0ULL);

    for (size_t c_idx : top_clusters) {
        for (uint32_t idx : clusters_[c_idx].member_indices) {
            if (idx < total_vecs) {
                cluster_mask[idx / 64] |= (1ULL << (idx % 64));
            }
        }
    }

    // Intersect with user allowlist_mask if provided
    if (allowlist_mask) {
        for (size_t w = 0; w < mask_words; ++w) {
            cluster_mask[w] &= allowlist_mask[w];
        }
    }

    return base_index_.search(query_vec, top_k, cluster_mask.data());
}

size_t TurboQuantIVF::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return base_index_.size();
}

size_t TurboQuantIVF::memory_bytes() const {
    std::lock_guard<std::mutex> lock(mtx_);
    size_t bytes = base_index_.memory_bytes();
    for (const auto& c : clusters_) {
        bytes += c.centroid.size() * sizeof(float);
        bytes += c.member_indices.size() * sizeof(uint32_t);
    }
    return bytes;
}
