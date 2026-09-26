#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>

enum class MemoryCategory {
    RULE,
    CONVENTION,
    DISCOVERY,
    ARCHITECTURAL_DECISION
};

struct MemoryEntry {
    std::string id;
    MemoryCategory category = MemoryCategory::DISCOVERY;
    std::string key;
    std::string value;
    float confidence = 1.0f;
    int64_t updated_at = 0;
};

class PersistentMemory {
public:
    PersistentMemory() = default;

    void set_entry(const MemoryEntry& entry);
    bool get_entry(const std::string& key, MemoryEntry& out) const;
    bool remove_entry(const std::string& key);

    std::vector<MemoryEntry> get_all_by_category(MemoryCategory cat) const;
    std::vector<MemoryEntry> get_all() const;
    size_t count() const;

    std::string format_context_rules() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::map<std::string, MemoryEntry> entries_;
};
