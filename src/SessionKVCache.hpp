#pragma once
#include "infer.hpp"
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <memory>

struct SessionKVState {
    std::mutex state_mutex;
    InferenceState state;
    std::vector<int> cached_tokens;
    int max_context_allocated = 0;
    int num_kv_heads = 0;
    int head_dim = 0;
    bool is_initialized = false;
    int64_t last_accessed_ms = 0;
    bool in_use = false;
};

class SessionKVCacheManager {
public:
    static constexpr size_t DEFAULT_MAX_ACTIVE_SESSIONS = 8;

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

    // Configure maximum active in-RAM KV sessions before LRU disk spill
    void set_max_active_sessions(size_t limit) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_active_sessions_ = limit;
    }
    size_t get_max_active_sessions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return max_active_sessions_;
    }
    size_t get_active_session_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.size();
    }

    // Evict/release session KV cache (optionally flush to disk)
    void evict(const std::string& session_id, bool persist_to_disk = true);

    // Clear all session states
    void clear_all();

private:
    SessionKVCacheManager();
    bool save_to_disk_internal(const std::string& session_id, const std::shared_ptr<SessionKVState>& s);
    void evict_oldest_session_locked();

    mutable std::mutex mutex_;
    std::string cache_dir_;
    size_t max_active_sessions_ = DEFAULT_MAX_ACTIVE_SESSIONS;
    std::map<std::string, std::shared_ptr<SessionKVState>> sessions_;
};
