#include "turboquant_index.hpp"
#include <immintrin.h>
#include <cmath>
#include <random>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <queue>

static const float LLOYD_MAX_16[16] = {
    -2.7326f, -2.0690f, -1.6181f, -1.2562f,
    -0.9424f, -0.6568f, -0.3881f, -0.1284f,
     0.1284f,  0.3881f,  0.6568f,  0.9424f,
     1.2562f,  1.6181f,  2.0690f,  2.7326f
};

static const float BOUNDARIES_16[15] = {
    -2.4008f, -1.8435f, -1.4371f, -1.0993f,
    -0.7996f, -0.5224f, -0.2582f,  0.0000f,
     0.2582f,  0.5224f,  0.7996f,  1.0993f,
     1.4371f,  1.8435f,  2.4008f
};

static inline uint8_t quantize_scalar(float v) {
    uint8_t bin = 0;
    while (bin < 15 && v > BOUNDARIES_16[bin]) { bin++; }
    return bin;
}

TurboQuantIndex::TurboQuantIndex(size_t dim, uint32_t seed) : dim_(dim) {
    init_rotation_matrix(seed);
}

void TurboQuantIndex::init_rotation_matrix(uint32_t seed) {
    rotation_matrix_.resize(dim_ * dim_);
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (size_t i = 0; i < dim_ * dim_; ++i) {
        rotation_matrix_[i] = dist(rng);
    }

    // Modified Gram-Schmidt orthonormalization
    for (size_t i = 0; i < dim_; ++i) {
        float* vi = &rotation_matrix_[i * dim_];
        for (size_t j = 0; j < i; ++j) {
            const float* vj = &rotation_matrix_[j * dim_];
            float dot = 0.0f;
            for (size_t k = 0; k < dim_; ++k) dot += vi[k] * vj[k];
            for (size_t k = 0; k < dim_; ++k) vi[k] -= dot * vj[k];
        }
        float norm = 0.0f;
        for (size_t k = 0; k < dim_; ++k) norm += vi[k] * vi[k];
        norm = std::sqrt(norm);
        float inv_norm = norm > 1e-8f ? 1.0f / norm : 0.0f;
        for (size_t k = 0; k < dim_; ++k) vi[k] *= inv_norm;
    }
}

void TurboQuantIndex::rotate_vector(const float* in, float* out) const {
    for (size_t i = 0; i < dim_; ++i) {
        const float* row = &rotation_matrix_[i * dim_];
        float sum = 0.0f;
        for (size_t j = 0; j < dim_; ++j) sum += row[j] * in[j];
        out[i] = sum;
    }
}

void TurboQuantIndex::quantize_rotated(const float* rotated, QuantizedVector& qv) const {
    float mean = 0.0f;
    for (size_t i = 0; i < dim_; ++i) mean += rotated[i];
    mean /= static_cast<float>(dim_);

    float var = 0.0f;
    for (size_t i = 0; i < dim_; ++i) {
        float diff = rotated[i] - mean;
        var += diff * diff;
    }
    float std_dev = std::sqrt(var / static_cast<float>(dim_));
    if (std_dev < 1e-7f) std_dev = 1e-7f;

    qv.scale = std_dev;
    qv.offset = mean;
    qv.packed.resize((dim_ + 1) / 2, 0);

    for (size_t i = 0; i < dim_; i += 2) {
        float norm_0 = (rotated[i] - mean) / std_dev;
        uint8_t bin_0 = quantize_scalar(norm_0);
        uint8_t bin_1 = 0;
        if (i + 1 < dim_) {
            float norm_1 = (rotated[i + 1] - mean) / std_dev;
            bin_1 = quantize_scalar(norm_1);
        }
        qv.packed[i / 2] = (bin_0 & 0x0F) | ((bin_1 & 0x0F) << 4);
    }
}

void TurboQuantIndex::add(const std::string& id, const std::vector<float>& vec) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<float> padded_in(dim_, 0.0f);
    size_t copy_len = std::min(vec.size(), dim_);
    std::copy_n(vec.begin(), copy_len, padded_in.begin());

    std::vector<float> rot(dim_, 0.0f);
    rotate_vector(padded_in.data(), rot.data());

    QuantizedVector qv;
    quantize_rotated(rot.data(), qv);

    auto it = index_by_id_.find(id);
    if (it != index_by_id_.end()) {
        vectors_[it->second] = std::move(qv);
    } else {
        uint32_t idx = static_cast<uint32_t>(vectors_.size());
        vectors_.push_back(std::move(qv));
        id_by_index_.push_back(id);
        index_by_id_[id] = idx;
    }
}

bool TurboQuantIndex::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = index_by_id_.find(id);
    if (it == index_by_id_.end()) return false;
    vectors_[it->second].active = false;
    return true;
}

std::vector<TurboQuantHit> TurboQuantIndex::search(
    const std::vector<float>& query_vec, size_t top_k, const uint64_t* allowlist_mask) const {
    std::lock_guard<std::mutex> lock(mtx_);
    if (vectors_.empty() || top_k == 0) return {};

    std::vector<float> padded_q(dim_, 0.0f);
    size_t copy_len = std::min(query_vec.size(), dim_);
    std::copy_n(query_vec.begin(), copy_len, padded_q.begin());

    std::vector<float> q_rot(dim_, 0.0f);
    rotate_vector(padded_q.data(), q_rot.data());

    float q_sum = 0.0f;
    float q_norm_sq = 0.0f;
    for (float v : q_rot) {
        q_sum += v;
        q_norm_sq += v * v;
    }
    float q_norm = std::sqrt(q_norm_sq);
    if (q_norm < 1e-8f) return {};

    using HitPair = std::pair<float, uint32_t>;
    std::priority_queue<HitPair, std::vector<HitPair>, std::greater<HitPair>> min_heap;

    for (uint32_t j = 0; j < vectors_.size(); ++j) {
        if (allowlist_mask && !(allowlist_mask[j / 64] & (1ULL << (j % 64)))) continue;
        const auto& qv = vectors_[j];
        if (!qv.active) continue;

        const uint8_t* p = qv.packed.data();
        __m256 sum_vec0 = _mm256_setzero_ps();
        __m256 sum_vec1 = _mm256_setzero_ps();

        size_t i = 0;
        for (; i + 16 <= dim_; i += 16) {
            alignas(32) float c_vals[16];
            for (int k = 0; k < 8; ++k) {
                uint8_t byte = p[i / 2 + k];
                c_vals[2 * k] = LLOYD_MAX_16[byte & 0x0F];
                c_vals[2 * k + 1] = LLOYD_MAX_16[(byte >> 4) & 0x0F];
            }
            __m256 q0 = _mm256_loadu_ps(&q_rot[i]);
            __m256 c0 = _mm256_load_ps(&c_vals[0]);
            sum_vec0 = _mm256_fmadd_ps(q0, c0, sum_vec0);

            __m256 q1 = _mm256_loadu_ps(&q_rot[i + 8]);
            __m256 c1 = _mm256_load_ps(&c_vals[8]);
            sum_vec1 = _mm256_fmadd_ps(q1, c1, sum_vec1);
        }

        __m256 sum_tot = _mm256_add_ps(sum_vec0, sum_vec1);
        alignas(32) float hsum[8];
        _mm256_store_ps(hsum, sum_tot);
        float code_dot = hsum[0] + hsum[1] + hsum[2] + hsum[3] + hsum[4] + hsum[5] + hsum[6] + hsum[7];

        // Residual elements
        for (; i < dim_; ++i) {
            uint8_t byte = p[i / 2];
            uint8_t bin = (i % 2 == 0) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
            code_dot += q_rot[i] * LLOYD_MAX_16[bin];
        }

        float dot_prod = qv.scale * code_dot + qv.offset * q_sum;
        float score = dot_prod / (q_norm * std::max(1e-6f, qv.scale * std::sqrt(static_cast<float>(dim_))));

        if (min_heap.size() < top_k) {
            min_heap.push({score, j});
        } else if (score > min_heap.top().first) {
            min_heap.pop();
            min_heap.push({score, j});
        }
    }

    std::vector<TurboQuantHit> results;
    results.reserve(min_heap.size());
    while (!min_heap.empty()) {
        auto top = min_heap.top();
        min_heap.pop();
        results.push_back({id_by_index_[top.second], top.second, top.first});
    }
    std::reverse(results.begin(), results.end());
    return results;
}

size_t TurboQuantIndex::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    size_t count = 0;
    for (const auto& v : vectors_) if (v.active) count++;
    return count;
}

size_t TurboQuantIndex::memory_bytes() const {
    std::lock_guard<std::mutex> lock(mtx_);
    size_t bytes = rotation_matrix_.size() * sizeof(float);
    for (const auto& v : vectors_) bytes += sizeof(v) + v.packed.capacity();
    return bytes;
}

bool TurboQuantIndex::save(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::string tmp_path = filepath + ".tmp";
    std::ofstream out(tmp_path, std::ios::binary);
    if (!out.is_open()) return false;

    uint32_t magic = 0x444C5651; // "DLVQ"
    uint32_t version = 2; // Bump version for EOF marker
    uint32_t d = static_cast<uint32_t>(dim_);
    uint32_t n = static_cast<uint32_t>(vectors_.size());

    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&d), sizeof(d));
    out.write(reinterpret_cast<const char*>(&n), sizeof(n));

    for (size_t i = 0; i < n; ++i) {
        const auto& v = vectors_[i];
        const auto& id = id_by_index_[i];
        uint32_t id_len = static_cast<uint32_t>(id.size());
        out.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        if (id_len > 0) {
            out.write(id.data(), id_len);
        }
        out.write(reinterpret_cast<const char*>(&v.scale), sizeof(v.scale));
        out.write(reinterpret_cast<const char*>(&v.offset), sizeof(v.offset));
        uint8_t active_b = v.active ? 1 : 0;
        out.write(reinterpret_cast<const char*>(&active_b), sizeof(active_b));
        out.write(reinterpret_cast<const char*>(v.packed.data()), v.packed.size());
    }

    uint32_t eof_marker = 0x454F4621; // "EOF!"
    out.write(reinterpret_cast<const char*>(&eof_marker), sizeof(eof_marker));

    out.close();
    std::filesystem::rename(tmp_path, filepath);
    return true;
}

bool TurboQuantIndex::load(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ifstream in(filepath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;
    std::streamsize file_size = in.tellg();
    in.seekg(0, std::ios::beg);

    if (file_size < 16) return false;

    uint32_t magic = 0, version = 0, d = 0, n = 0;
    in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&d), sizeof(d));
    in.read(reinterpret_cast<char*>(&n), sizeof(n));

    if (magic != 0x444C5651 || (version != 1 && version != 2) || d != dim_) return false;
    
    // Hard limit on maximum vectors to prevent OOM
    if (n > 10000000) return false;

    size_t packed_len = (dim_ + 1) / 2;
    std::streamsize min_expected_size = 16 + static_cast<std::streamsize>(n) * (13 + packed_len);
    if (file_size < min_expected_size) return false;
    
    if (version == 2 && file_size < min_expected_size + 4) return false; // Account for EOF marker

    vectors_.clear();
    id_by_index_.clear();
    index_by_id_.clear();

    try {
        vectors_.reserve(n);
        id_by_index_.reserve(n);
        index_by_id_.reserve(n);
    } catch (const std::bad_alloc&) {
        return false;
    }

    for (uint32_t i = 0; i < n; ++i) {
        if (in.eof() || in.fail()) goto load_error;

        uint32_t id_len = 0;
        in.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
        if (id_len == 0 || id_len > 4096 || in.eof() || in.fail()) goto load_error;

        std::string id(id_len, '\0');
        in.read(&id[0], id_len);

        QuantizedVector v;
        in.read(reinterpret_cast<char*>(&v.scale), sizeof(v.scale));
        in.read(reinterpret_cast<char*>(&v.offset), sizeof(v.offset));
        uint8_t active_b = 0;
        in.read(reinterpret_cast<char*>(&active_b), sizeof(active_b));
        v.active = (active_b != 0);

        v.packed.resize(packed_len);
        in.read(reinterpret_cast<char*>(v.packed.data()), packed_len);

        if (in.fail()) goto load_error;

        index_by_id_[id] = static_cast<uint32_t>(vectors_.size());
        id_by_index_.push_back(id);
        vectors_.push_back(std::move(v));
    }
    
    if (version == 2) {
        uint32_t eof_marker = 0;
        in.read(reinterpret_cast<char*>(&eof_marker), sizeof(eof_marker));
        if (eof_marker != 0x454F4621 || in.fail()) {
            goto load_error;
        }
    }
    
    return true;

load_error:
    vectors_.clear(); 
    id_by_index_.clear(); 
    index_by_id_.clear();
    return false;
}
