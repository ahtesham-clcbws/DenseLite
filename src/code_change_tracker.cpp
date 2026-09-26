#include "code_change_tracker.hpp"
#include <functional>

uint64_t CodeChangeTracker::compute_hash(const std::string& content) {
    // 64-bit FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    for (char c : content) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool CodeChangeTracker::has_changed(const std::string& file_path, const std::string& content) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = file_hashes_.find(file_path);
    if (it == file_hashes_.end()) {
        return true; // Not seen before -> changed
    }
    return it->second != compute_hash(content);
}

void CodeChangeTracker::update_hash(const std::string& file_path, const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_hashes_[file_path] = compute_hash(content);
}

void CodeChangeTracker::remove_file(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_hashes_.erase(file_path);
}

uint64_t CodeChangeTracker::get_hash(const std::string& file_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = file_hashes_.find(file_path);
    if (it != file_hashes_.end()) {
        return it->second;
    }
    return 0;
}

void CodeChangeTracker::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    file_hashes_.clear();
}
