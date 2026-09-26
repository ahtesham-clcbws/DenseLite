#include "structural_search.hpp"
#include <algorithm>
#include <cctype>

static bool icase_contains(const std::string& str, const std::string& sub) {
    if (sub.empty()) return true;
    if (str.size() < sub.size()) return false;
    auto it = std::search(str.begin(), str.end(), sub.begin(), sub.end(),
        [](char c1, char c2) {
            return std::tolower(static_cast<unsigned char>(c1)) == std::tolower(static_cast<unsigned char>(c2));
        });
    return it != str.end();
}

static bool icase_equals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), [](char c1, char c2) {
        return std::tolower(static_cast<unsigned char>(c1)) == std::tolower(static_cast<unsigned char>(c2));
    });
}

static SearchResult chunk_to_result(const StructuralChunk& chunk,
                                    float structural_importance,
                                    float symbol_match) {
    SearchResult res;
    res.id = chunk.file_path + ":" + std::to_string(chunk.start_line);
    res.source = SearchSource::CODE;
    res.file_path = chunk.file_path;
    res.symbol_name = chunk.symbol;
    res.content = chunk.content;
    res.line_start = chunk.start_line;
    res.line_end = chunk.end_line;
    res.structural_importance = structural_importance;
    res.symbol_match = symbol_match;
    res.lexical_score = 0.5f;
    return res;
}

std::vector<SearchResult> StructuralSearch::search_class(
    const std::string& class_name,
    const std::vector<StructuralChunk>& chunks) {

    std::vector<SearchResult> results;
    if (class_name.empty()) return results;

    for (const auto& chunk : chunks) {
        bool is_class_def = (icase_equals(chunk.symbol, class_name) &&
            (chunk.parent_symbol.empty() || icase_equals(chunk.parent_symbol, class_name)));
        if (is_class_def) {
            results.push_back(chunk_to_result(chunk, 1.0f, 1.0f));
        }
    }
    return results;
}

std::vector<SearchResult> StructuralSearch::search_methods_in_scope(
    const std::string& scope_name,
    const std::vector<StructuralChunk>& chunks) {

    std::vector<SearchResult> results;
    if (scope_name.empty()) return results;

    for (const auto& chunk : chunks) {
        if (icase_equals(chunk.parent_symbol, scope_name) && !icase_equals(chunk.symbol, scope_name)) {
            results.push_back(chunk_to_result(chunk, 0.85f, 0.8f));
        }
    }
    return results;
}

std::vector<SearchResult> StructuralSearch::search_all(
    const std::string& query,
    const std::vector<StructuralChunk>& chunks) {

    std::vector<SearchResult> results;
    if (query.empty()) return results;

    for (const auto& chunk : chunks) {
        if (icase_contains(chunk.symbol, query) ||
            icase_contains(chunk.parent_symbol, query)) {
            float sym_match = icase_equals(chunk.symbol, query) ? 1.0f : 0.6f;
            bool is_class = chunk.parent_symbol.empty() || icase_equals(chunk.parent_symbol, chunk.symbol);
            float imp = is_class ? 1.0f : 0.75f;
            results.push_back(chunk_to_result(chunk, imp, sym_match));
        }
    }
    return results;
}

