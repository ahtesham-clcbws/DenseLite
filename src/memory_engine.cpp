#include "memory_engine.hpp"
#include <sstream>

MemoryEngine::MemoryEngine()
    : recall_(&persistent_, &store_), consolidator_(&store_, &persistent_) {}

bool MemoryEngine::init(const std::string& sqlite_path, const std::string& zvec_path) {
    if (!store_.init(sqlite_path, zvec_path)) {
        return false;
    }
    // Load persistent rules from SQLite into in-RAM persistent memory
    auto loaded = store_.load_all_persistent();
    for (const auto& entry : loaded) {
        persistent_.set_entry(entry);
    }
    return true;
}

std::string MemoryEngine::assemble_memory_context(const std::string& current_query) {
    std::ostringstream oss;

    // 1. Working Memory (Current objective, task, constraints)
    std::string wm = working_.format_context_block();
    if (!wm.empty()) {
        oss << wm << "\n";
    }

    // 2. Persistent Memory (Active system rules & conventions)
    std::string pm = persistent_.format_context_rules();
    if (!pm.empty()) {
        oss << pm << "\n";
    }

    // 3. Recalled Relevant Memories (Targeted by query)
    if (!current_query.empty()) {
        auto recalled = recall_.recall(current_query, 5);
        std::string rm = recall_.format_recalled_context(recalled);
        if (!rm.empty()) {
            oss << rm << "\n";
        }
    }

    return oss.str();
}

ConsolidationResult MemoryEngine::end_session_and_archive() {
    auto result = consolidator_.consolidate(session_);
    session_.clear();
    working_.reset();
    return result;
}
