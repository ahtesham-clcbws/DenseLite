#include "SessionKVCache.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>

constexpr uint32_t DLKV_MAGIC = 0x444C4B56; // "DLKV"
constexpr uint32_t DLKV_VERSION = 1;

SessionKVCacheManager& SessionKVCacheManager::instance() {
    static SessionKVCacheManager mgr;
    return mgr;
}

std::shared_ptr<SessionKVState> SessionKVCacheManager::get_or_create(const std::string& session_id, 
                                                                     const ModelConfig* config) {
    if (session_id.empty()) {
        return std::make_shared<SessionKVState>();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second;
    }
    
    auto new_state = std::make_shared<SessionKVState>();
    sessions_[session_id] = new_state;

    // Check if persistent disk cache exists
    if (config) {
        std::string path = cache_dir_ + "/" + session_id + ".kv";
        if (std::filesystem::exists(path)) {
            // Internal call with lock already held
            std::ifstream in(path, std::ios::binary);
            if (in.is_open()) {
                uint32_t magic = 0, version = 0;
                in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
                in.read(reinterpret_cast<char*>(&version), sizeof(version));
                if (magic == DLKV_MAGIC && version == DLKV_VERSION) {
                    int32_t num_layers = 0, num_kv_heads = 0, head_dim = 0, max_ctx = 0, cur_pos = 0;
                    in.read(reinterpret_cast<char*>(&num_layers), sizeof(num_layers));
                    in.read(reinterpret_cast<char*>(&num_kv_heads), sizeof(num_kv_heads));
                    in.read(reinterpret_cast<char*>(&head_dim), sizeof(head_dim));
                    in.read(reinterpret_cast<char*>(&max_ctx), sizeof(max_ctx));
                    in.read(reinterpret_cast<char*>(&cur_pos), sizeof(cur_pos));

                    uint32_t tok_count = 0;
                    in.read(reinterpret_cast<char*>(&tok_count), sizeof(tok_count));
                    new_state->cached_tokens.resize(tok_count);
                    if (tok_count > 0) {
                        in.read(reinterpret_cast<char*>(new_state->cached_tokens.data()), tok_count * sizeof(int));
                    }

                    init_inference_state(*config, max_ctx, new_state->state);
                    new_state->state.current_pos = cur_pos;
                    new_state->max_context_allocated = max_ctx;
                    new_state->is_initialized = true;

                    size_t active_floats = static_cast<size_t>(cur_pos) * num_kv_heads * head_dim;
                    for (int l = 0; l < num_layers; ++l) {
                        in.read(reinterpret_cast<char*>(new_state->state.k_cache[l].data()), active_floats * sizeof(float));
                        in.read(reinterpret_cast<char*>(new_state->state.v_cache[l].data()), active_floats * sizeof(float));
                    }
                    std::cout << "[SessionKVCache] Restored " << cur_pos << " tokens from disk for session: " << session_id << std::endl;
                }
            }
        }
    }

    return new_state;
}

bool SessionKVCacheManager::save_to_disk(const std::string& session_id) {
    if (session_id.empty()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end() || !it->second || !it->second->is_initialized) return false;

    std::filesystem::create_directories(cache_dir_);
    std::string path = cache_dir_ + "/" + session_id + ".kv";
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;

    const auto& s = it->second;
    uint32_t magic = DLKV_MAGIC;
    uint32_t version = DLKV_VERSION;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));

    int32_t num_layers = static_cast<int32_t>(s->state.k_cache.size());
    int32_t num_kv_heads = 2; // Default GQA for Qwen
    int32_t head_dim = 128;
    int32_t max_ctx = s->max_context_allocated;
    int32_t cur_pos = s->state.current_pos;

    out.write(reinterpret_cast<const char*>(&num_layers), sizeof(num_layers));
    out.write(reinterpret_cast<const char*>(&num_kv_heads), sizeof(num_kv_heads));
    out.write(reinterpret_cast<const char*>(&head_dim), sizeof(head_dim));
    out.write(reinterpret_cast<const char*>(&max_ctx), sizeof(max_ctx));
    out.write(reinterpret_cast<const char*>(&cur_pos), sizeof(cur_pos));

    uint32_t tok_count = static_cast<uint32_t>(s->cached_tokens.size());
    out.write(reinterpret_cast<const char*>(&tok_count), sizeof(tok_count));
    if (tok_count > 0) {
        out.write(reinterpret_cast<const char*>(s->cached_tokens.data()), tok_count * sizeof(int));
    }

    size_t active_floats = static_cast<size_t>(cur_pos) * num_kv_heads * head_dim;
    for (int l = 0; l < num_layers; ++l) {
        out.write(reinterpret_cast<const char*>(s->state.k_cache[l].data()), active_floats * sizeof(float));
        out.write(reinterpret_cast<const char*>(s->state.v_cache[l].data()), active_floats * sizeof(float));
    }

    return true;
}

bool SessionKVCacheManager::load_from_disk(const std::string& session_id, const ModelConfig& config) {
    auto state = get_or_create(session_id, &config);
    return (state && state->is_initialized);
}

void SessionKVCacheManager::evict(const std::string& session_id, bool persist_to_disk) {
    if (session_id.empty()) return;
    if (persist_to_disk) {
        save_to_disk(session_id);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session_id);
}

void SessionKVCacheManager::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
}
