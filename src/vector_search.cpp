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

float VectorSearch::semantic_overlap(const std::string& query, const std::string& target) {
    if (query.empty() || target.empty()) return 0.0f;

    static const std::unordered_map<std::string, std::vector<std::string>> synonyms = {
        {"login", {"auth", "authenticate", "session", "credential", "user", "password", "token"}},
        {"auth", {"login", "authentication", "authorize", "token", "session", "jwt"}},
        {"database", {"sql", "sqlite", "table", "store", "query", "record", "db"}},
        {"route", {"endpoint", "url", "api", "handler", "request", "http"}},
        {"memory", {"cache", "store", "persist", "recall", "state", "session"}}
    };

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

    auto q_words = to_words(query);
    auto t_words = to_words(target);
    std::unordered_set<std::string> target_set(t_words.begin(), t_words.end());

    size_t hits = 0;
    size_t total_semantic_terms = q_words.size();

    for (const auto& w : q_words) {
        if (target_set.count(w)) {
            hits += 2;
            continue;
        }
        auto it = synonyms.find(w);
        if (it != synonyms.end()) {
            for (const auto& syn : it->second) {
                if (target_set.count(syn)) {
                    hits += 1;
                    break;
                }
            }
        }
    }

    if (total_semantic_terms == 0) return 0.0f;
    float score = static_cast<float>(hits) / static_cast<float>(total_semantic_terms * 2);
    return std::min(1.0f, score);
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

    std::vector<SearchResult> results;
    for (auto res : candidates) {
        float sim = semantic_overlap(query, res.content);
        if (sim >= threshold) {
            res.semantic_similarity = sim;
            results.push_back(res);
        }
    }
    return results;
}
