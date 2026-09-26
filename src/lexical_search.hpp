#pragma once
#include "search_result.hpp"
#include "structural_chunk.hpp"
#include <string>
#include <vector>

class LexicalSearch {
public:
    static std::vector<std::string> tokenize(const std::string& text);

    static float score_content(const std::vector<std::string>& query_terms,
                               const std::string& content);

    static std::vector<SearchResult> search_chunks(
        const std::string& query,
        const std::vector<StructuralChunk>& chunks,
        float threshold = 0.1f);

    static std::vector<SearchResult> search_texts(
        const std::string& query,
        const std::vector<std::pair<std::string, std::string>>& id_and_texts,
        float threshold = 0.1f);
};
