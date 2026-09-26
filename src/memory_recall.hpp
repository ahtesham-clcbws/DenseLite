#pragma once
#include "persistent_memory.hpp"
#include "memory_store.hpp"
#include <string>
#include <vector>

struct RecalledMemory {
    MemoryEntry entry;
    float relevance_score = 0.0f;
    std::string match_source; // "persistent_cache", "canonical_store", "semantic_index"
};

class MemoryRecall {
public:
    MemoryRecall(PersistentMemory* persistent_mem, MemoryStore* memory_store);

    // Recalls the top-K relevant memories given a query text
    std::vector<RecalledMemory> recall(const std::string& query, size_t limit = 5);

    // Formats recalled memories into a prompt context section
    std::string format_recalled_context(const std::vector<RecalledMemory>& memories) const;

private:
    PersistentMemory* persistent_mem_ = nullptr;
    MemoryStore* memory_store_ = nullptr;

    float compute_lexical_similarity(const std::string& query, const std::string& target) const;
};
