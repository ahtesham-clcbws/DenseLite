#pragma once
#include "infer.hpp"
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <memory>

struct SessionKVState {
    InferenceState state;
    std::vector<int> cached_tokens;
    int max_context_allocated = 0;
    bool is_initialized = false;
};

class SessionKVCacheManager {
public:
    static SessionKVCacheManager& instance();

    // Get or initialize persistent KV cache state for a session
    std::shared_ptr<SessionKVState> get_or_create(const std::string& session_id, const ModelConfig* config = nullptr);

    // Save session KV state to disk
    bool save_to_disk(const std::string& session_id);

    // Load session KV state from disk
    bool load_from_disk(const std::string& session_id, const ModelConfig& config);

    // Configure cache directory
    void set_cache_directory(const std::string& dir) { cache_dir_ = dir; }
    std::string get_cache_directory() const { return cache_dir_; }

    // Evict/release session KV cache (optionally flush to disk)
    void evict(const std::string& session_id, bool persist_to_disk = true);

    // Clear all session states
    void clear_all();

private:
    SessionKVCacheManager();
    mutable std::mutex mutex_;
    std::string cache_dir_;
    std::map<std::string, std::shared_ptr<SessionKVState>> sessions_;
};
