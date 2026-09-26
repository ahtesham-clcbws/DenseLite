#pragma once
#include "search_result.hpp"
#include "structural_chunk.hpp"
#include <string>
#include <vector>

class ExactSearch {
public:
    static std::vector<SearchResult> search_code(
        const std::string& query,
        const std::vector<StructuralChunk>& chunks);

    static std::vector<SearchResult> search_text(
        const std::string& query,
        const std::string& text,
        const std::string& file_path = "");

    static bool is_exact_symbol(const std::string& query, const std::string& target);
};
