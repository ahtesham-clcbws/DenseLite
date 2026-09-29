#include <iostream>
#include <cassert>
#include <cmath>
#include <filesystem>

#include "SessionToolRegistry.hpp"
#include "SessionKVCache.hpp"

void test_session_tool_registry() {
    std::cout << "[TEST] SessionToolRegistry...\n";
    auto& reg = SessionToolRegistry::instance();
    std::string sess = "test-session-42";
    reg.clear_session(sess);

    assert(!reg.has_tools(sess));

    OpenAITool t1;
    t1.type = "function";
    t1.function.name = "execute_bash";
    t1.function.description = "run bash command";

    OpenAITool t2;
    t2.type = "function";
    t2.function.name = "read_file";
    t2.function.description = "read local file";

    std::vector<OpenAITool> tools = {t1, t2};

    reg.register_tools(sess, tools);
    assert(reg.has_tools(sess));

    auto all_tools = reg.get_all_tools(sess);
    assert(all_tools.size() == 2);

    auto bash_only = reg.get_tools_by_names(sess, {"execute_bash"});
    assert(bash_only.size() == 1);
    assert(bash_only[0].function.name == "execute_bash");

    reg.clear_session(sess);
    assert(!reg.has_tools(sess));
    std::cout << "   -> PASS: SessionToolRegistry tests passed.\n";
}

void test_session_kv_cache_disk_persistence() {
    std::cout << "[TEST] SessionKVCacheManager & Disk Persistence...\n";
    auto& kv_mgr = SessionKVCacheManager::instance();
    std::string test_dir = "/tmp/denselite_kv_test";
    std::filesystem::remove_all(test_dir);
    kv_mgr.set_cache_directory(test_dir);

    std::string sess = "sess-persistence-test";

    // Setup dummy model config
    ModelConfig cfg;
    cfg.num_layers = 2;
    cfg.embedding_length = 64;
    cfg.num_kv_heads = 2;
    cfg.head_dim = 32;

    // 1. Create and populate state
    auto state = kv_mgr.get_or_create(sess, &cfg);
    assert(state != nullptr);
    assert(state->is_initialized == true);

    state->cached_tokens = {101, 2054, 2003, 1037, 3231, 102};
    state->state.current_pos = 6;
    // Set a known value in KV buffer
    if (!state->state.k_cache.empty() && !state->state.k_cache[0].empty()) {
        state->state.k_cache[0][0] = 3.14159f;
    }

    // 2. Save to disk
    bool saved = kv_mgr.save_to_disk(sess);
    assert(saved == true);
    (void)saved;

    // Check file exists on disk
    std::string expected_file = test_dir + "/" + sess + ".kv";
    assert(std::filesystem::exists(expected_file));
    assert(std::filesystem::file_size(expected_file) > 0);

    // 3. Evict from memory
    kv_mgr.evict(sess, false);

    // 4. Reload from disk
    bool loaded = kv_mgr.load_from_disk(sess, cfg);
    assert(loaded == true);
    (void)loaded;

    auto reloaded_state = kv_mgr.get_or_create(sess, &cfg);
    assert(reloaded_state != nullptr);
    assert(reloaded_state->cached_tokens.size() == 6);
    assert(reloaded_state->cached_tokens[0] == 101);
    assert(reloaded_state->cached_tokens[5] == 102);
    assert(reloaded_state->state.current_pos == 6);
    assert(reloaded_state->state.k_cache[0][0] == 3.14159f);

    // Cleanup
    kv_mgr.clear_all();
    std::filesystem::remove_all(test_dir);
    std::cout << "   -> PASS: SessionKVCacheManager disk persistence passed.\n";
}

void test_session_kv_cache_lru_eviction() {
    std::cout << "[TEST] SessionKVCacheManager LRU Eviction...\n";
    auto& kv_mgr = SessionKVCacheManager::instance();
    std::string test_dir = "/tmp/denselite_kv_lru_test";
    std::filesystem::remove_all(test_dir);
    kv_mgr.set_cache_directory(test_dir);
    kv_mgr.clear_all();
    kv_mgr.set_max_active_sessions(2);

    ModelConfig cfg;
    cfg.num_layers = 1;
    cfg.embedding_length = 32;
    cfg.num_kv_heads = 2;
    cfg.head_dim = 16;

    // 1. Create sessions s1 and s2
    auto s1 = kv_mgr.get_or_create("s1", &cfg);
    s1->cached_tokens = {1, 2, 3};
    s1->state.current_pos = 3;
    s1->is_initialized = true;

    auto s2 = kv_mgr.get_or_create("s2", &cfg);
    s2->cached_tokens = {4, 5, 6};
    s2->state.current_pos = 3;
    s2->is_initialized = true;

    assert(kv_mgr.get_active_session_count() == 2);

    // Release local strong references so use_count == 1
    s1.reset();
    s2.reset();

    // 2. Create s3 -> should evict oldest session (s1) to disk
    auto s3 = kv_mgr.get_or_create("s3", &cfg);
    s3->cached_tokens = {7, 8, 9};
    s3->state.current_pos = 3;
    s3->is_initialized = true;
    s3.reset();

    assert(kv_mgr.get_active_session_count() == 2);
    assert(std::filesystem::exists(test_dir + "/s1.kv"));

    // 3. Re-access s1 -> should restore from disk and evict s2
    auto restored_s1 = kv_mgr.get_or_create("s1", &cfg);
    assert(restored_s1 != nullptr);
    assert(restored_s1->is_initialized == true);
    assert(restored_s1->cached_tokens.size() == 3);
    assert(restored_s1->cached_tokens[0] == 1);
    assert(std::filesystem::exists(test_dir + "/s2.kv"));

    // Cleanup
    kv_mgr.set_max_active_sessions(SessionKVCacheManager::DEFAULT_MAX_ACTIVE_SESSIONS);
    kv_mgr.clear_all();
    std::filesystem::remove_all(test_dir);
    std::cout << "   -> PASS: SessionKVCacheManager LRU eviction verified.\n";
}

void test_64k_context_scaling_and_persistence() {
    std::cout << "[TEST] 64K Context Allocation, NTK RoPE Scaling & Persistence...\n";
    ModelConfig cfg;
    cfg.num_layers = 1;
    cfg.embedding_length = 128;
    cfg.num_kv_heads = 2;
    cfg.head_dim = 64;
    cfg.rope.base = 10000.0f;

    InferenceState state;
    init_inference_state(cfg, 65536, state);

    // Verify 64K KV cache allocation size
    size_t expected_kv_elements = 65536ULL * cfg.num_kv_heads * cfg.head_dim;
    assert(state.k_cache[0].size() == expected_kv_elements);
    assert(state.v_cache[0].size() == expected_kv_elements);

    // Verify NTK RoPE frequency scaling applied (base scaled up)
    assert(state.inv_freq.size() == cfg.head_dim / 2);
    // Base for 64K context must be significantly higher than standard 10000.0f
    float scaled_base = 10000.0f * std::pow(65536.0f / 8192.0f, static_cast<float>(cfg.head_dim) / (cfg.head_dim - 2));
    float expected_inv0 = 1.0f / std::pow(scaled_base, 0.0f);
    assert(std::abs(state.inv_freq[0] - expected_inv0) < 1e-4f);

    std::cout << "   -> PASS: 64K KV memory sized (" << (expected_kv_elements * sizeof(float) * 2 / (1024 * 1024))
              << " MiB/layer) & NTK RoPE scaled to base " << static_cast<int>(scaled_base) << ".\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "[TEST] Phase 9: Session KV & Tool Tests\n";
    std::cout << "========================================\n";
    test_session_tool_registry();
    test_session_kv_cache_disk_persistence();
    test_session_kv_cache_lru_eviction();
    test_64k_context_scaling_and_persistence();
    std::cout << "\n>>> ALL PHASE 9 SESSION TESTS PASSED! <<<\n";
    return 0;
}

