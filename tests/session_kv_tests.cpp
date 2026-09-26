#include <iostream>
#include <cassert>
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

int main() {
    std::cout << "========================================\n";
    std::cout << "[TEST] Phase 9: Session KV & Tool Tests\n";
    std::cout << "========================================\n";
    test_session_tool_registry();
    test_session_kv_cache_disk_persistence();
    std::cout << "\n>>> ALL PHASE 9 SESSION TESTS PASSED! <<<\n";
    return 0;
}
