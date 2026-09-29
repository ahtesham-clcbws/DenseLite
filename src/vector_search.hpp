#pragma once
#include "search_result.hpp"
#include <vector>
#include <string>

class VectorSearch {
public:
    static float cosine_similarity(const std::vector<float>& vec_a,
                                   const std::vector<float>& vec_b);

    // Continuous subword & semantic domain dense vector projection in R^128
    static std::vector<float> embed_text(const std::string& text, size_t dim = 128);

    static float semantic_overlap(const std::string& query,
                                  const std::string& target);

    static std::vector<SearchResult> search(
        const std::vector<float>& query_vec,
        const std::vector<std::pair<SearchResult, std::vector<float>>>& candidates,
        float threshold = 0.2f);

    static std::vector<SearchResult> search_semantic_text(
        const std::string& query,
        const std::vector<SearchResult>& candidates,
        float threshold = 0.15f);
};

