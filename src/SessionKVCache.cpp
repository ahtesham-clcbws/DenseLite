#include "SessionKVCache.hpp"
#include "path_service.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <limits>

constexpr uint32_t DLKV_MAGIC = 0x444C4B56; // "DLKV"
constexpr uint32_t DLKV_VERSION = 3;

static uint32_t compute_model_hash(const ModelConfig& cfg) {
    uint32_t hash = 2166136261u;
    auto add_str = [&](const std::string& s) {
        for (char c : s) { hash ^= static_cast<uint8_t>(c); hash *= 16777619u; }
    };
    auto add_u32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i) { hash ^= (v & 0xFF); hash *= 16777619u; v >>= 8; }
    };
    add_str(cfg.architecture);
    add_u32(cfg.vocab_size);
    add_u32(cfg.num_layers);
    add_u32(cfg.num_heads);
    add_u32(cfg.num_kv_heads);
    add_u32(cfg.head_dim);
    add_u32(cfg.intermediate_dim);
    return hash;
}

static int64_t current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

SessionKVCacheManager::SessionKVCacheManager() {
    cache_dir_ = PathService::instance().get_kv_cache_dir();
}

SessionKVCacheManager& SessionKVCacheManager::instance() {
    static SessionKVCacheManager mgr;
    return mgr;
}

bool SessionKVCacheManager::save_to_disk_internal(const std::string& session_id, const std::shared_ptr<SessionKVState>& s) {
    if (!s || !s->is_initialized) return false;
    std::unique_lock<std::mutex> slock(s->state_mutex);

    std::filesystem::create_directories(cache_dir_);
    std::string path = cache_dir_ + "/" + session_id + ".kv";
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;

    uint32_t magic = DLKV_MAGIC;
    uint32_t version = 3;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&s->model_hash), sizeof(s->model_hash));

    // Write model_identity
    uint32_t mi_len = s->model_identity.size();
    out.write(reinterpret_cast<const char*>(&mi_len), sizeof(mi_len));
    if (mi_len > 0) out.write(s->model_identity.c_str(), mi_len);
    
    // Write tokenizer_identity
    uint32_t ti_len = s->tokenizer_identity.size();
    out.write(reinterpret_cast<const char*>(&ti_len), sizeof(ti_len));
    if (ti_len > 0) out.write(s->tokenizer_identity.c_str(), ti_len);

    int32_t num_layers = static_cast<int32_t>(s->state.k_cache.size());
    int32_t num_kv_heads = (s->num_kv_heads > 0) ? s->num_kv_heads : 2;
    int32_t head_dim = (s->head_dim > 0) ? s->head_dim : 128;
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

void SessionKVCacheManager::evict_oldest_session_locked() {
    if (sessions_.empty()) return;
    auto oldest_it = sessions_.end();
    int64_t oldest_time = std::numeric_limits<int64_t>::max();

    for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
        if ((it->second.use_count() == 1 && !it->second->in_use) || oldest_it == sessions_.end()) {
            if (it->second->last_accessed_ms < oldest_time) {
                oldest_time = it->second->last_accessed_ms;
                oldest_it = it;
            }
        }
    }

    if (oldest_it != sessions_.end()) {
        std::string evicted_id = oldest_it->first;
        save_to_disk_internal(evicted_id, oldest_it->second);
        sessions_.erase(oldest_it);
        std::cout << "[SessionKVCache] LRU evicted session: " << evicted_id << " (RAM freed)" << std::endl;
    }
}

std::shared_ptr<SessionKVState> SessionKVCacheManager::get_or_create(const std::string& session_id, 
                                                                     const ModelConfig* config) {
    if (session_id.empty()) {
        return std::make_shared<SessionKVState>();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second->last_accessed_ms = current_time_ms();
        return it->second;
    }
    
    while (sessions_.size() >= max_active_sessions_) {
        evict_oldest_session_locked();
    }

    auto new_state = std::make_shared<SessionKVState>();
    new_state->last_accessed_ms = current_time_ms();
    if (config) {
        new_state->num_kv_heads = config->num_kv_heads;
        new_state->head_dim = config->head_dim;
        new_state->model_hash = compute_model_hash(*config);
        new_state->model_identity = config->model_id;
    }
    sessions_[session_id] = new_state;

    // Check if persistent disk cache exists
    if (config) {
        std::string path = cache_dir_ + "/" + session_id + ".kv";
        if (std::filesystem::exists(path)) {
            std::ifstream in(path, std::ios::binary);
            if (in.is_open()) {
                uint32_t magic = 0, version = 0;
                in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
                in.read(reinterpret_cast<char*>(&version), sizeof(version));
                if (magic == DLKV_MAGIC && (version >= 1 && version <= 3)) {
                    uint32_t disk_model_hash = 0;
                    std::string disk_model_id = "";
                    std::string disk_tok_id = "";
                    
                    if (version >= 2) {
                        in.read(reinterpret_cast<char*>(&disk_model_hash), sizeof(disk_model_hash));
                    }
                    if (version >= 3) {
                        uint32_t mi_len = 0;
                        in.read(reinterpret_cast<char*>(&mi_len), sizeof(mi_len));
                        if (mi_len > 0 && mi_len < 4096) {
                            disk_model_id.resize(mi_len);
                            in.read(&disk_model_id[0], mi_len);
                        }
                        uint32_t ti_len = 0;
                        in.read(reinterpret_cast<char*>(&ti_len), sizeof(ti_len));
                        if (ti_len > 0 && ti_len < 4096) {
                            disk_tok_id.resize(ti_len);
                            in.read(&disk_tok_id[0], ti_len);
                        }
                    }

                    int32_t num_layers = 0, num_kv_heads = 0, head_dim = 0, max_ctx = 0, cur_pos = 0;
                    in.read(reinterpret_cast<char*>(&num_layers), sizeof(num_layers));
                    in.read(reinterpret_cast<char*>(&num_kv_heads), sizeof(num_kv_heads));
                    in.read(reinterpret_cast<char*>(&head_dim), sizeof(head_dim));
                    in.read(reinterpret_cast<char*>(&max_ctx), sizeof(max_ctx));
                    in.read(reinterpret_cast<char*>(&cur_pos), sizeof(cur_pos));

                    if (num_layers != static_cast<int32_t>(config->num_layers) ||
                        num_kv_heads != static_cast<int32_t>(config->num_kv_heads) ||
                        head_dim != static_cast<int32_t>(config->head_dim)) {
                        std::cout << "[SessionKVCache] Discarding incompatible disk cache for session " 
                                  << session_id << " (cached: " << num_layers << "L/" << num_kv_heads << "H/" << head_dim 
                                  << "D vs active: " << config->num_layers << "L/" << config->num_kv_heads << "H/" 
                                  << config->head_dim << "D)" << std::endl;
                        return new_state;
                    }

                    uint32_t expected_hash = compute_model_hash(*config);
                    if (version >= 2 && disk_model_hash != expected_hash) {
                        std::cout << "[SessionKVCache] Discarding disk cache for session " << session_id 
                                  << " (model hash mismatch: expected " << expected_hash << " got " << disk_model_hash << ")" << std::endl;
                        return new_state;
                    }

                    if (version >= 3 && !config->model_id.empty() && disk_model_id != config->model_id) {
                        std::cout << "[SessionKVCache] Discarding disk cache for session " << session_id 
                                  << " (model identity mismatch: expected " << config->model_id << " got " << disk_model_id << ")" << std::endl;
                        return new_state;
                    }

                    uint32_t tok_count = 0;
                    in.read(reinterpret_cast<char*>(&tok_count), sizeof(tok_count));
                    new_state->cached_tokens.resize(tok_count);
                    if (tok_count > 0) {
                        in.read(reinterpret_cast<char*>(new_state->cached_tokens.data()), tok_count * sizeof(int));
                    }

                    init_inference_state(*config, max_ctx, new_state->state);
                    new_state->state.current_pos = cur_pos;
                    new_state->max_context_allocated = max_ctx;
                    new_state->num_kv_heads = num_kv_heads;
                    new_state->head_dim = head_dim;
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
    if (it == sessions_.end()) return false;
    return save_to_disk_internal(session_id, it->second);
}

bool SessionKVCacheManager::load_from_disk(const std::string& session_id, const ModelConfig& config) {
    auto state = get_or_create(session_id, &config);
    return (state && state->is_initialized);
}

void SessionKVCacheManager::evict(const std::string& session_id, bool persist_to_disk) {
    if (session_id.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        if (persist_to_disk) {
            save_to_disk_internal(session_id, it->second);
        }
        sessions_.erase(it);
    }
}

void SessionKVCacheManager::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
}
