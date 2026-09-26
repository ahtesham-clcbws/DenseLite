#pragma once
#include "session_memory.hpp"
#include "persistent_memory.hpp"
#include "memory_store.hpp"
#include <string>
#include <vector>

struct ConsolidationResult {
    std::string archive_id;
    std::string session_id;
    std::string summary;
    std::vector<MemoryEntry> extracted_memories;
    size_t ignored_count = 0;
    size_t updated_count = 0;
    size_t added_count = 0;
};

class MemoryConsolidator {
public:
    explicit MemoryConsolidator(MemoryStore* store, PersistentMemory* persistent_mem = nullptr);

    // Consolidates a session: compresses turns into archive summary and extracts durable facts
    ConsolidationResult consolidate(const SessionMemory& session);

    // Deterministic candidate detection: inspects text for durable signals
    static bool is_durable_candidate(const std::string& text);

    // Lightweight deterministic fact extraction from text
    static std::vector<MemoryEntry> extract_facts(const std::string& text);

private:
    MemoryStore* store_ = nullptr;
    PersistentMemory* persistent_mem_ = nullptr;
};
