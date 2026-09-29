#include "vector_search.hpp"
#include <cmath>
#include <cctype>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

float VectorSearch::cosine_similarity(const std::vector<float>& a,
                                     const std::vector<float>& b) {
    if (a.empty() || a.size() != b.size()) return 0.0f;

    double dot = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;

    for (size_t i = 0; i < a.size(); ++i) {
        dot += static_cast<double>(a[i]) * static_cast<double>(b[i]);
        norm_a += static_cast<double>(a[i]) * static_cast<double>(a[i]);
        norm_b += static_cast<double>(b[i]) * static_cast<double>(b[i]);
    }

    if (norm_a <= 1e-9 || norm_b <= 1e-9) return 0.0f;
    float sim = static_cast<float>(dot / (std::sqrt(norm_a) * std::sqrt(norm_b)));
    return std::max(0.0f, std::min(1.0f, (sim + 1.0f) * 0.5f));
}

#include <mutex>

static std::mutex g_emb_mutex;
static VectorSearch::EmbeddingFn g_embedding_fn = nullptr;
static DenseModel* g_embedding_model = nullptr;

void VectorSearch::set_embedding_fn(EmbeddingFn fn) {
    std::lock_guard<std::mutex> lock(g_emb_mutex);
    g_embedding_fn = std::move(fn);
}

void VectorSearch::set_embedding_model(DenseModel* model) {
    std::lock_guard<std::mutex> lock(g_emb_mutex);
    g_embedding_model = model;
}

DenseModel* VectorSearch::get_embedding_model() {
    std::lock_guard<std::mutex> lock(g_emb_mutex);
    return g_embedding_model;
}

static inline uint32_t fnv1a_hash(const std::string& str) {
    uint32_t hash = 2166136261u;
    for (char c : str) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

std::vector<float> VectorSearch::embed_text(const std::string& text, size_t dim) {
    // 1. Live neural embedding dispatch (Task 3.2 / Register 85-86)
    {
        std::lock_guard<std::mutex> lock(g_emb_mutex);
        if (g_embedding_fn) {
            auto neural = g_embedding_fn(text);
            if (!neural.empty()) return neural;
        }
    }

    // 2. Deterministic subword feature projection fallback
    if (dim < 64) dim = 64;
    std::vector<float> vec(dim, 0.0f);
    if (text.empty()) return vec;

    auto to_words = [](const std::string& s) {
        std::vector<std::string> words;
        std::string cur;
        for (char c : s) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                cur.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            } else if (!cur.empty()) {
                words.push_back(cur);
                cur.clear();
            }
        }
        if (!cur.empty()) words.push_back(cur);
        return words;
    };

    auto words = to_words(text);
    if (words.empty()) return vec;

    // Semantic cluster bases: maps conceptual terms to structured subspace directions
    static const std::unordered_map<std::string, int> cluster_map = {
        {"auth", 0}, {"login", 0}, {"authenticate", 0}, {"credential", 0}, {"password", 0}, {"token", 0}, {"session", 0}, {"jwt", 0}, {"signin", 0}, {"verify", 0},
        {"db", 1}, {"database", 1}, {"sql", 1}, {"sqlite", 1}, {"table", 1}, {"store", 1}, {"query", 1}, {"record", 1}, {"schema", 1},
        {"route", 2}, {"endpoint", 2}, {"url", 2}, {"api", 2}, {"handler", 2}, {"request", 2}, {"http", 2}, {"controller", 2}, {"client", 2},
        {"memory", 3}, {"cache", 3}, {"persist", 3}, {"recall", 3}, {"state", 3}, {"kv", 3}, {"buffer", 3},
        {"thread", 4}, {"async", 4}, {"mutex", 4}, {"lock", 4}, {"task", 4}, {"worker", 4}, {"pool", 4},
        {"code", 5}, {"ast", 5}, {"tree", 5}, {"symbol", 5}, {"class", 5}, {"function", 5}, {"method", 5}
    };

    for (const auto& w : words) {
        auto it = cluster_map.find(w);
        if (it != cluster_map.end()) {
            int cluster_idx = it->second;
            for (int k = 0; k < 6; ++k) {
                vec[cluster_idx * 6 + k] += 2.0f;
            }
        }

        // Subword n-gram feature hashing into remaining dimensions
        uint32_t w_hash = fnv1a_hash(w);
        size_t w_dim = 36 + (w_hash % (dim - 36));
        vec[w_dim] += 1.5f;

        if (w.size() >= 3) {
            for (size_t i = 0; i <= w.size() - 3; ++i) {
                std::string tri = w.substr(i, 3);
                uint32_t t_hash = fnv1a_hash(tri);
                size_t t_dim = 36 + (t_hash % (dim - 36));
                vec[t_dim] += 0.5f;
            }
        }
    }

    // L2 Normalization
    double norm_sq = 0.0;
    for (float v : vec) norm_sq += static_cast<double>(v) * v;
    if (norm_sq > 1e-9) {
        float inv_norm = static_cast<float>(1.0 / std::sqrt(norm_sq));
        for (float& v : vec) v *= inv_norm;
    }

    return vec;
}

float VectorSearch::semantic_overlap(const std::string& query, const std::string& target) {
    if (query.empty() || target.empty()) return 0.0f;
    auto q_vec = embed_text(query);
    auto t_vec = embed_text(target);
    return cosine_similarity(q_vec, t_vec);
}

std::vector<SearchResult> VectorSearch::search(
    const std::vector<float>& query_vec,
    const std::vector<std::pair<SearchResult, std::vector<float>>>& candidates,
    float threshold) {

    std::vector<SearchResult> results;
    for (const auto& pair : candidates) {
        float sim = cosine_similarity(query_vec, pair.second);
        if (sim >= threshold) {
            SearchResult res = pair.first;
            res.semantic_similarity = sim;
            results.push_back(res);
        }
    }
    return results;
}

std::vector<SearchResult> VectorSearch::search_semantic_text(
    const std::string& query,
    const std::vector<SearchResult>& candidates,
    float threshold) {

    if (query.empty() || candidates.empty()) return {};
    auto query_vec = embed_text(query);

    std::vector<SearchResult> results;
    for (auto res : candidates) {
        auto doc_vec = embed_text(res.content);
        float sim = cosine_similarity(query_vec, doc_vec);
        if (sim >= threshold) {
            res.semantic_similarity = sim;
            results.push_back(res);
        }
    }
    return results;
}

