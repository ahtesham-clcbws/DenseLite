#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <cstdint>

class CodeChangeTracker {
public:
    CodeChangeTracker() = default;

    // Checks if the file content hash has changed compared to recorded state
    bool has_changed(const std::string& file_path, const std::string& content) const;

    // Updates the recorded hash for a file
    void update_hash(const std::string& file_path, const std::string& content);

    // Removes tracking for a file
    void remove_file(const std::string& file_path);

    uint64_t get_hash(const std::string& file_path) const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, uint64_t> file_hashes_;

    static uint64_t compute_hash(const std::string& content);
};
