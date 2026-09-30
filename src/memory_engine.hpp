#pragma once
#include "working_memory.hpp"
#include "session_memory.hpp"
#include "persistent_memory.hpp"
#include "memory_store.hpp"
#include "memory_recall.hpp"
#include "memory_consolidator.hpp"
#include <string>
#include <memory>

class MemoryEngine {
public:
    MemoryEngine();

    // Initialize canonical SQLite store and load persistent rules into cache
    bool init(const std::string& sqlite_path, const std::string& zvec_path = "");

    WorkingMemory& working() { return working_; }
    SessionMemory& session() { return session_; }
    PersistentMemory& persistent() { return persistent_; }
    MemoryStore& store() { return store_; }
    MemoryRecall& recall() { return recall_; }
    MemoryConsolidator& consolidator() { return consolidator_; }

    // Generates a complete memory context block (Working + Persistent + Recalled) for prompt injection
    std::string assemble_memory_context(const std::string& current_query, const MemoryScopeFilter& filter = MemoryScopeFilter{});

    std::vector<std::pair<MemoryEntry, float>> query_memories_vector(
        const std::vector<float>& query_vec, size_t limit = 10, float threshold = 0.2f,
        const MemoryScopeFilter& filter = MemoryScopeFilter{}) {
        return store_.query_memories_vector(query_vec, limit, threshold, filter);
    }

    bool supersede_memory(const std::string& old_key, const std::string& new_key) {
        return store_.supersede_memory(old_key, new_key);
    }

    bool touch_memory(const std::string& key) {
        return store_.touch_memory(key);
    }

    // Consolidates active session to archive and extracts durable memories
    ConsolidationResult end_session_and_archive();

private:
    WorkingMemory working_;
    SessionMemory session_;
    PersistentMemory persistent_;
    MemoryStore store_;
    MemoryRecall recall_;
    MemoryConsolidator consolidator_;
};
