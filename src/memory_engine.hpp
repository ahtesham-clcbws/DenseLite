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
    std::string assemble_memory_context(const std::string& current_query);

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
