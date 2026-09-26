#include "persistent_memory.hpp"
#include <chrono>
#include <sstream>

void PersistentMemory::set_entry(const MemoryEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    MemoryEntry e = entry;
    if (e.updated_at == 0) {
        e.updated_at = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    entries_[e.key] = std::move(e);
}

bool PersistentMemory::get_entry(const std::string& key, MemoryEntry& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        out = it->second;
        return true;
    }
    return false;
}

bool PersistentMemory::remove_entry(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.erase(key) > 0;
}

std::vector<MemoryEntry> PersistentMemory::get_all_by_category(MemoryCategory cat) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryEntry> result;
    for (const auto& pair : entries_) {
        if (pair.second.category == cat) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<MemoryEntry> PersistentMemory::get_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryEntry> result;
    result.reserve(entries_.size());
    for (const auto& pair : entries_) {
        result.push_back(pair.second);
    }
    return result;
}

size_t PersistentMemory::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

std::string PersistentMemory::format_context_rules() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (entries_.empty()) return "";

    std::ostringstream oss;
    oss << "[Persistent System Knowledge & Rules]\n";
    for (const auto& pair : entries_) {
        const auto& e = pair.second;
        std::string cat_str;
        switch (e.category) {
            case MemoryCategory::RULE: cat_str = "Rule"; break;
            case MemoryCategory::CONVENTION: cat_str = "Convention"; break;
            case MemoryCategory::DISCOVERY: cat_str = "Discovery"; break;
            case MemoryCategory::ARCHITECTURAL_DECISION: cat_str = "Decision"; break;
        }
        oss << "[" << cat_str << "] " << e.key << ": " << e.value << "\n";
    }
    return oss.str();
}

void PersistentMemory::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}
