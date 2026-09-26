#pragma once
#include "search_result.hpp"
#include "exact_search.hpp"
#include "lexical_search.hpp"
#include "vector_search.hpp"
#include "structural_search.hpp"
#include "result_fusion.hpp"
#include "code_indexer.hpp"
#include "memory_engine.hpp"
#include <mutex>
#include <string>
#include <vector>

class SearchEngine {
public:
    explicit SearchEngine(CodeIndexer* code_indexer = nullptr,
                          MemoryEngine* memory_engine = nullptr);

    void set_code_indexer(CodeIndexer* indexer);
    void set_memory_engine(MemoryEngine* engine);

    std::vector<SearchResult> search(const std::string& query,
                                     const std::string& active_task = "",
                                     size_t top_k = 10);

    std::vector<SearchResult> search_code(const std::string& query,
                                          size_t top_k = 10);

    std::vector<SearchResult> search_memory(const std::string& query,
                                            size_t top_k = 10);

    void add_custom_chunk(const StructuralChunk& chunk);
    void clear_custom_chunks();

private:
    std::mutex mutex_;
    CodeIndexer* code_indexer_ = nullptr;
    MemoryEngine* memory_engine_ = nullptr;
    std::vector<StructuralChunk> custom_chunks_;
};
