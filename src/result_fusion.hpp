#pragma once
#include "search_result.hpp"
#include <vector>

struct FusionWeights {
    float w1_semantic{0.25f};
    float w2_lexical{0.25f};
    float w3_structural{0.15f};
    float w4_symbol{0.20f};
    float w5_recency{0.05f};
    float w6_task{0.10f};
};

class ResultFusion {
public:
    static float compute_score(const SearchResult& res, const FusionWeights& w = FusionWeights{});

    static std::vector<SearchResult> fuse(
        const std::vector<SearchResult>& multi_signal_results,
        size_t top_k = 10,
        const FusionWeights& weights = FusionWeights{});

    static bool is_duplicate(const SearchResult& a, const SearchResult& b);

    static SearchResult merge_results(const SearchResult& a, const SearchResult& b);
};
