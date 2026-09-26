#pragma once
#include "search_result.hpp"
#include "structural_chunk.hpp"
#include <string>
#include <vector>

class StructuralSearch {
public:
    static std::vector<SearchResult> search_class(
        const std::string& class_name,
        const std::vector<StructuralChunk>& chunks);

    static std::vector<SearchResult> search_methods_in_scope(
        const std::string& scope_name,
        const std::vector<StructuralChunk>& chunks);

    static std::vector<SearchResult> search_all(
        const std::string& query,
        const std::vector<StructuralChunk>& chunks);
};
