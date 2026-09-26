#include "memory_consolidator.hpp"
#include <sstream>
#include <chrono>
#include <algorithm>

MemoryConsolidator::MemoryConsolidator(MemoryStore* store, PersistentMemory* persistent_mem)
    : store_(store), persistent_mem_(persistent_mem) {}

bool MemoryConsolidator::is_durable_candidate(const std::string& text) {
    if (text.size() < 10) return false;

    const std::vector<std::string> triggers = {
        "remember", "always", "never", "rule:", "decision:", "convention:",
        "stack:", "database:", "framework:", "architecture:", "configured",
        "using version", "updated to"
    };

    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& trig : triggers) {
        if (lower.find(trig) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<MemoryEntry> MemoryConsolidator::extract_facts(const std::string& text) {
    std::vector<MemoryEntry> entries;
    std::istringstream iss(text);
    std::string line;

    while (std::getline(iss, line)) {
        if (!is_durable_candidate(line)) continue;

        // Check for explicit key-value patterns, e.g., "framework: Laravel 12"
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos && colon_pos > 2 && colon_pos + 2 < line.size()) {
            std::string key = line.substr(0, colon_pos);
            std::string val = line.substr(colon_pos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t-*\r\n"));
            key.erase(key.find_last_not_of(" \t-*\r\n") + 1);
            val.erase(0, val.find_first_not_of(" \t\r\n"));
            val.erase(val.find_last_not_of(" \t\r\n") + 1);

            if (!key.empty() && !val.empty()) {
                MemoryEntry entry;
                entry.id = "mem_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                entry.key = key;
                entry.value = val;
                entry.category = MemoryCategory::DISCOVERY;

                std::string k_lower = key;
                std::transform(k_lower.begin(), k_lower.end(), k_lower.begin(), ::tolower);
                if (k_lower.find("rule") != std::string::npos) entry.category = MemoryCategory::RULE;
                else if (k_lower.find("decision") != std::string::npos) entry.category = MemoryCategory::ARCHITECTURAL_DECISION;
                else if (k_lower.find("convention") != std::string::npos) entry.category = MemoryCategory::CONVENTION;

                entries.push_back(std::move(entry));
            }
        }
    }
    return entries;
}

ConsolidationResult MemoryConsolidator::consolidate(const SessionMemory& session) {
    ConsolidationResult result;
    result.session_id = session.get_session_id();
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    result.archive_id = "arch_" + std::to_string(now);

    auto turns = session.get_turns();
    auto decisions = session.get_decisions();

    // 1. Generate structured archive summary
    std::ostringstream summary_oss;
    summary_oss << "Session Archive [" << result.session_id << "] - Turns: " << turns.size() << "\n";
    if (!decisions.empty()) {
        summary_oss << "Decisions:\n";
        for (const auto& d : decisions) {
            summary_oss << " - " << d << "\n";
        }
    }
    result.summary = summary_oss.str();

    // 2. Extract durable facts from turns and decisions
    std::ostringstream facts_oss;
    for (const auto& d : decisions) {
        auto facts = extract_facts(d);
        for (auto& f : facts) {
            result.extracted_memories.push_back(std::move(f));
        }
    }
    for (const auto& t : turns) {
        if (t.role == "user" || t.role == "assistant") {
            auto facts = extract_facts(t.content);
            for (auto& f : facts) {
                result.extracted_memories.push_back(std::move(f));
            }
        }
    }

    // 3. Commit to store and persistent memory
    for (const auto& mem : result.extracted_memories) {
        facts_oss << "[" << mem.key << "]: " << mem.value << "\n";
        if (store_) {
            MemoryEntry existing;
            if (store_->get_memory(mem.key, existing)) {
                result.updated_count++;
            } else {
                result.added_count++;
            }
            store_->put_memory(mem);
        }
        if (persistent_mem_) {
            persistent_mem_->set_entry(mem);
        }
    }

    // 4. Save archive record in store
    if (store_) {
        store_->save_archive(result.archive_id, result.session_id, result.summary, facts_oss.str());
    }

    return result;
}
