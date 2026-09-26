#include "lexical_search.hpp"
#include <cctype>
#include <cmath>
#include <algorithm>
#include <unordered_set>

std::vector<std::string> LexicalSearch::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            current.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (!current.empty()) {
            if (current.size() >= 2) {
                tokens.push_back(current);
            }
            current.clear();
        }
    }
    if (current.size() >= 2) {
        tokens.push_back(current);
    }
    return tokens;
}

float LexicalSearch::score_content(const std::vector<std::string>& query_terms,
                                  const std::string& content) {
    if (query_terms.empty() || content.empty()) return 0.0f;

    auto doc_terms = tokenize(content);
    if (doc_terms.empty()) return 0.0f;

    std::unordered_set<std::string> unique_query(query_terms.begin(), query_terms.end());
    size_t hits = 0;
    for (const auto& term : unique_query) {
        for (const auto& doc_term : doc_terms) {
            if (doc_term == term) {
                hits++;
                break;
            }
        }
    }

    float match_ratio = static_cast<float>(hits) / static_cast<float>(unique_query.size());
    return std::min(1.0f, match_ratio);
}

std::vector<SearchResult> LexicalSearch::search_chunks(
    const std::string& query,
    const std::vector<StructuralChunk>& chunks,
    float threshold) {

    std::vector<SearchResult> results;
    auto query_terms = tokenize(query);
    if (query_terms.empty()) return results;

    for (const auto& chunk : chunks) {
        float score = score_content(query_terms, chunk.content);
        if (score >= threshold) {
            SearchResult res;
            res.id = chunk.file_path + ":" + std::to_string(chunk.start_line);
            res.source = SearchSource::CODE;
            res.file_path = chunk.file_path;
            res.symbol_name = chunk.symbol;
            res.content = chunk.content;
            res.line_start = chunk.start_line;
            res.line_end = chunk.end_line;
            res.lexical_score = score;
            bool is_class = chunk.parent_symbol.empty() || chunk.parent_symbol == chunk.symbol;
            res.structural_importance = is_class ? 0.9f : 0.7f;
            results.push_back(res);
        }
    }
    return results;
}

std::vector<SearchResult> LexicalSearch::search_texts(
    const std::string& query,
    const std::vector<std::pair<std::string, std::string>>& id_and_texts,
    float threshold) {

    std::vector<SearchResult> results;
    auto query_terms = tokenize(query);
    if (query_terms.empty()) return results;

    for (const auto& item : id_and_texts) {
        float score = score_content(query_terms, item.second);
        if (score >= threshold) {
            SearchResult res;
            res.id = item.first;
            res.source = SearchSource::TEXT;
            res.content = item.second;
            res.lexical_score = score;
            results.push_back(res);
        }
    }
    return results;
}
