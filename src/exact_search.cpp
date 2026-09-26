#include "exact_search.hpp"
#include <algorithm>
#include <cctype>

static bool icase_equals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), [](char c1, char c2) {
        return std::tolower(static_cast<unsigned char>(c1)) == std::tolower(static_cast<unsigned char>(c2));
    });
}

static bool icase_contains(const std::string& str, const std::string& sub) {
    if (sub.empty()) return true;
    if (str.size() < sub.size()) return false;
    auto it = std::search(str.begin(), str.end(), sub.begin(), sub.end(),
        [](char c1, char c2) {
            return std::tolower(static_cast<unsigned char>(c1)) == std::tolower(static_cast<unsigned char>(c2));
        });
    return it != str.end();
}

bool ExactSearch::is_exact_symbol(const std::string& query, const std::string& target) {
    if (query.empty() || target.empty()) return false;
    return icase_equals(query, target);
}

std::vector<SearchResult> ExactSearch::search_code(
    const std::string& query,
    const std::vector<StructuralChunk>& chunks) {

    std::vector<SearchResult> results;
    if (query.empty()) return results;

    for (const auto& chunk : chunks) {
        bool symbol_hit = is_exact_symbol(query, chunk.symbol) || is_exact_symbol(query, chunk.parent_symbol);
        bool substring_hit = icase_contains(chunk.content, query);

        if (symbol_hit || substring_hit) {
            SearchResult res;
            res.id = chunk.file_path + ":" + std::to_string(chunk.start_line);
            res.source = SearchSource::CODE;
            res.file_path = chunk.file_path;
            res.symbol_name = chunk.symbol;
            res.content = chunk.content;
            res.line_start = chunk.start_line;
            res.line_end = chunk.end_line;
            res.symbol_match = symbol_hit ? 1.0f : 0.5f;
            res.lexical_score = substring_hit ? 0.7f : 0.0f;
            bool is_class = chunk.parent_symbol.empty() || chunk.parent_symbol == chunk.symbol;
            res.structural_importance = is_class ? 1.0f : 0.8f;
            results.push_back(res);
        }
    }
    return results;
}

std::vector<SearchResult> ExactSearch::search_text(
    const std::string& query,
    const std::string& text,
    const std::string& file_path) {

    std::vector<SearchResult> results;
    if (query.empty() || text.empty()) return results;

    size_t pos = text.find(query);
    if (pos != std::string::npos) {
        SearchResult res;
        res.id = file_path + ":exact";
        res.source = SearchSource::TEXT;
        res.file_path = file_path;
        res.content = text;
        res.lexical_score = 0.8f;
        res.symbol_match = (query.size() > 3) ? 0.6f : 0.3f;
        results.push_back(res);
    }
    return results;
}
