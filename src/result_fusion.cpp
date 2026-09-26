#include "result_fusion.hpp"
#include <algorithm>
#include <unordered_map>

float ResultFusion::compute_score(const SearchResult& res, const FusionWeights& w) {
    return (w.w1_semantic * res.semantic_similarity) +
           (w.w2_lexical * res.lexical_score) +
           (w.w3_structural * res.structural_importance) +
           (w.w4_symbol * res.symbol_match) +
           (w.w5_recency * res.recency) +
           (w.w6_task * res.task_relevance);
}

bool ResultFusion::is_duplicate(const SearchResult& a, const SearchResult& b) {
    if (!a.id.empty() && a.id == b.id) return true;
    if (a.source == b.source && !a.file_path.empty() && a.file_path == b.file_path) {
        if (a.line_start <= b.line_end && b.line_start <= a.line_end) {
            return true;
        }
    }
    return false;
}

SearchResult ResultFusion::merge_results(const SearchResult& a, const SearchResult& b) {
    SearchResult merged = a;
    if (merged.content.size() < b.content.size()) {
        merged.content = b.content;
    }
    merged.semantic_similarity = std::max(a.semantic_similarity, b.semantic_similarity);
    merged.lexical_score = std::max(a.lexical_score, b.lexical_score);
    merged.structural_importance = std::max(a.structural_importance, b.structural_importance);
    merged.symbol_match = std::max(a.symbol_match, b.symbol_match);
    merged.recency = std::max(a.recency, b.recency);
    merged.task_relevance = std::max(a.task_relevance, b.task_relevance);
    merged.timestamp = std::max(a.timestamp, b.timestamp);
    return merged;
}

std::vector<SearchResult> ResultFusion::fuse(
    const std::vector<SearchResult>& multi_signal_results,
    size_t top_k,
    const FusionWeights& weights) {

    if (multi_signal_results.empty()) return {};

    std::vector<SearchResult> deduplicated;
    for (const auto& res : multi_signal_results) {
        bool found = false;
        for (auto& existing : deduplicated) {
            if (is_duplicate(existing, res)) {
                existing = merge_results(existing, res);
                found = true;
                break;
            }
        }
        if (!found) {
            deduplicated.push_back(res);
        }
    }

    for (auto& item : deduplicated) {
        item.final_score = compute_score(item, weights);
    }

    std::sort(deduplicated.begin(), deduplicated.end(),
              [](const SearchResult& x, const SearchResult& y) {
                  return x.final_score > y.final_score;
              });

    if (deduplicated.size() > top_k) {
        deduplicated.resize(top_k);
    }
    return deduplicated;
}
