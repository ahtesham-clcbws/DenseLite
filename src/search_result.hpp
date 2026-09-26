#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class SearchSource {
    CODE,
    MEMORY,
    TEXT
};

struct SearchResult {
    std::string id;
    SearchSource source{SearchSource::CODE};
    std::string file_path;
    std::string symbol_name;
    std::string content;
    uint32_t line_start{0};
    uint32_t line_end{0};
    int64_t timestamp{0};

    // Component signals in [0.0, 1.0]
    float semantic_similarity{0.0f};
    float lexical_score{0.0f};
    float structural_importance{0.0f};
    float symbol_match{0.0f};
    float recency{0.0f};
    float task_relevance{0.0f};

    // Deterministic fused score
    float final_score{0.0f};
};
