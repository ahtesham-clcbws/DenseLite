#include "memory_engine.hpp"
#include "vector_search.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <chrono>

void test_memory_add_and_retrieve() {
    std::cout << "[Test 1] Memory ADD & Retrieve..." << std::endl;
    std::string test_db = "/tmp/test_memory_p4a.db";
    std::filesystem::remove(test_db);

    MemoryEngine engine;
    assert(engine.init(test_db));

    MemoryEntry e;
    e.id = "mem_1";
    e.key = "framework";
    e.value = "Laravel 11";
    e.category = MemoryCategory::DISCOVERY;
    e.confidence = 1.0f;

    assert(engine.store().put_memory(e));

    MemoryEntry retrieved;
    assert(engine.store().get_memory("framework", retrieved));
    assert(retrieved.key == "framework");
    assert(retrieved.value == "Laravel 11");

    std::cout << "  -> PASSED" << std::endl;
}

void test_memory_update_no_conflict() {
    std::cout << "[Test 2] Memory UPDATE (Laravel 11 -> Laravel 12)..." << std::endl;
    std::string test_db = "/tmp/test_memory_p4a.db";

    MemoryEngine engine;
    assert(engine.init(test_db));

    // Update existing key
    MemoryEntry e_updated;
    e_updated.id = "mem_2";
    e_updated.key = "framework";
    e_updated.value = "Laravel 12";
    e_updated.category = MemoryCategory::DISCOVERY;
    e_updated.confidence = 1.0f;

    assert(engine.store().put_memory(e_updated));

    MemoryEntry retrieved;
    assert(engine.store().get_memory("framework", retrieved));
    assert(retrieved.value == "Laravel 12"); // Clean replacement without duplicate key conflict

    std::cout << "  -> PASSED" << std::endl;
}

void test_memory_delete() {
    std::cout << "[Test 3] Memory DELETE..." << std::endl;
    std::string test_db = "/tmp/test_memory_p4a.db";

    MemoryEngine engine;
    assert(engine.init(test_db));

    assert(engine.store().delete_memory("framework"));

    MemoryEntry retrieved;
    assert(!engine.store().get_memory("framework", retrieved));

    std::cout << "  -> PASSED" << std::endl;
}

void test_memory_ignore_noise() {
    std::cout << "[Test 4] Memory IGNORE (Noise Filtering)..." << std::endl;
    assert(MemoryConsolidator::is_durable_candidate("hi") == false);
    assert(MemoryConsolidator::is_durable_candidate("what time is it?") == false);
    assert(MemoryConsolidator::is_durable_candidate("thanks for the code") == false);

    assert(MemoryConsolidator::is_durable_candidate("Rule: Always use AVX2 kernels") == true);
    assert(MemoryConsolidator::is_durable_candidate("Remember that database: PostgreSQL 16") == true);

    std::cout << "  -> PASSED" << std::endl;
}

void test_session_compaction_and_archive() {
    std::cout << "[Test 5] Session Compaction & Fact Extraction..." << std::endl;
    std::string test_db = "/tmp/test_memory_p4a.db";

    MemoryEngine engine;
    assert(engine.init(test_db));

    engine.session().set_session_id("sess_bench_001");
    engine.session().add_message("user", "Hello assistant");
    engine.session().add_message("assistant", "Hello! How can I help you?");
    engine.session().add_message("user", "Rule: max OpenMP threads is 2");
    engine.session().add_decision("Decision: Use SQLite for canonical memory");

    auto result = engine.end_session_and_archive();
    assert(result.session_id == "sess_bench_001");
    assert(!result.archive_id.empty());
    assert(result.extracted_memories.size() >= 2);

    // Verify session turns were cleared from RAM
    assert(engine.session().turn_count() == 0);

    // Verify archive is stored in SQLite
    std::string summary;
    assert(engine.store().get_archive("sess_bench_001", summary));
    assert(!summary.empty());

    std::cout << "  -> PASSED" << std::endl;
}

void test_memory_recall_and_context_assembly() {
    std::cout << "[Test 6] Memory Recall & Context Assembly..." << std::endl;
    std::string test_db = "/tmp/test_memory_p4a.db";

    MemoryEngine engine;
    assert(engine.init(test_db));

    engine.working().set_objective("Port engine to ARM");
    engine.working().set_current_task("Compile AVX2 fallback");
    engine.working().add_constraint("Do not use external dependencies");

    MemoryEntry r;
    r.key = "threads";
    r.value = "Strict 2-thread cap";
    r.category = MemoryCategory::RULE;
    engine.persistent().set_entry(r);

    std::string context = engine.assemble_memory_context("What are the thread rules?");
    assert(context.find("Port engine to ARM") != std::string::npos);
    assert(context.find("Do not use external dependencies") != std::string::npos);
    assert(context.find("threads: Strict 2-thread cap") != std::string::npos);

    std::cout << "  -> PASSED" << std::endl;
}

void test_persistence_across_restarts() {
    std::cout << "[Test 7] Persistence Across Restarts (Reopening DB)..." << std::endl;
    std::string test_db = "/tmp/test_memory_p4a.db";

    {
        MemoryEngine engine1;
        assert(engine1.init(test_db));

        MemoryEntry fact;
        fact.key = "vulkan_vram_limit";
        fact.value = "1740 MiB";
        fact.category = MemoryCategory::DISCOVERY;
        assert(engine1.store().put_memory(fact));
    }

    // Simulate complete process restart: instantiate new engine pointing to same SQLite DB
    {
        MemoryEngine engine2;
        assert(engine2.init(test_db));

        MemoryEntry recovered;
        assert(engine2.persistent().get_entry("vulkan_vram_limit", recovered));
        assert(recovered.value == "1740 MiB");
    }

    std::filesystem::remove(test_db);
    std::cout << "  -> PASSED" << std::endl;
}

void test_vector_memory_search() {
    std::cout << "[Test 8] Semantic Vector Memory Search..." << std::endl;
    std::string test_db = "/tmp/test_memory_vector.db";
    std::filesystem::remove(test_db);

    MemoryEngine engine;
    assert(engine.init(test_db));

    MemoryEntry e1;
    e1.id = "mem_auth";
    e1.key = "auth_rule";
    e1.value = "User authentication requires JWT bearer tokens and password hashing.";
    e1.category = MemoryCategory::RULE;
    e1.confidence = 0.95f;
    e1.updated_at = 1700000000;
    assert(engine.store().put_memory(e1));

    MemoryEntry e2;
    e2.id = "mem_db";
    e2.key = "db_rule";
    e2.value = "SQLite database operates in WAL mode with normal synchronous writes.";
    e2.category = MemoryCategory::RULE;
    e2.confidence = 0.90f;
    e2.updated_at = 1700000001;
    assert(engine.store().put_memory(e2));

    auto q_vec = VectorSearch::embed_text("how to authenticate and login");
    auto hits = engine.query_memories_vector(q_vec, 5, 0.15f);
    assert(!hits.empty());
    assert(hits[0].first.key == "auth_rule");
    assert(hits[0].second > 0.15f);

    // Verify deletion removes from vector index as well
    assert(engine.store().delete_memory("auth_rule"));
    auto hits_after = engine.query_memories_vector(q_vec, 5, 0.15f);
    for (const auto& h : hits_after) {
        (void)h;
        assert(h.first.key != "auth_rule");
    }

    std::filesystem::remove(test_db);
    std::cout << "  -> PASSED" << std::endl;
}

void test_scoped_living_memory_and_provenance() {
    std::cout << "[Test 9] Scoped Living Memory & Provenance Isolation..." << std::endl;
    std::string test_db = "scoped_mem_test.db";
    std::filesystem::remove(test_db);

    MemoryEngine engine;
    assert(engine.init(test_db));

    // 1. Create memories in different workspaces with rich provenance
    MemoryEntry e_alpha;
    e_alpha.id = "mem_alpha_1";
    e_alpha.key = "alpha_config";
    e_alpha.value = "Workspace Alpha uses PostgreSQL port 5432.";
    e_alpha.category = MemoryCategory::ARCHITECTURAL_DECISION;
    e_alpha.workspace_id = "ws_alpha";
    e_alpha.session_id = "sess_001";
    e_alpha.source_path = "config/database.yml";
    e_alpha.source_type = "config";
    e_alpha.commit_hash = "git_hash_alpha123";
    e_alpha.importance = 0.9f;
    e_alpha.status = "active";
    e_alpha.updated_at = 1700000010;
    assert(engine.store().put_memory(e_alpha));

    MemoryEntry e_beta;
    e_beta.id = "mem_beta_1";
    e_beta.key = "beta_config";
    e_beta.value = "Workspace Beta uses MySQL port 3306.";
    e_beta.category = MemoryCategory::ARCHITECTURAL_DECISION;
    e_beta.workspace_id = "ws_beta";
    e_beta.session_id = "sess_002";
    e_beta.source_path = "env.local";
    e_beta.source_type = "config";
    e_beta.commit_hash = "git_hash_beta456";
    e_beta.importance = 0.8f;
    e_beta.status = "active";
    e_beta.updated_at = 1700000020;
    assert(engine.store().put_memory(e_beta));

    // 2. Verify workspace isolation in vector search
    auto q_vec = VectorSearch::embed_text("database port configuration");
    
    MemoryScopeFilter filter_alpha;
    filter_alpha.workspace_id = "ws_alpha";
    filter_alpha.active_only = true;
    auto hits_alpha = engine.query_memories_vector(q_vec, 10, 0.1f, filter_alpha);
    assert(!hits_alpha.empty());
    for (const auto& h : hits_alpha) {
        assert(h.first.workspace_id == "ws_alpha");
        assert(h.first.key != "beta_config");
    }

    MemoryScopeFilter filter_beta;
    filter_beta.workspace_id = "ws_beta";
    filter_beta.active_only = true;
    auto hits_beta = engine.query_memories_vector(q_vec, 10, 0.1f, filter_beta);
    assert(!hits_beta.empty());
    for (const auto& h : hits_beta) {
        assert(h.first.workspace_id == "ws_beta");
        assert(h.first.key != "alpha_config");
    }

    // 3. Verify provenance retrieval
    MemoryEntry alpha_retrieved;
    assert(engine.store().get_memory("alpha_config", alpha_retrieved));
    assert(alpha_retrieved.workspace_id == "ws_alpha");
    assert(alpha_retrieved.source_path == "config/database.yml");
    assert(alpha_retrieved.commit_hash == "git_hash_alpha123");
    assert(alpha_retrieved.access_count == 0);

    // 4. Test touch_memory
    assert(engine.touch_memory("alpha_config"));
    MemoryEntry alpha_touched;
    assert(engine.store().get_memory("alpha_config", alpha_touched));
    assert(alpha_touched.access_count == 1);
    assert(alpha_touched.last_accessed_at > 0);

    // 5. Test superseding a memory
    MemoryEntry e_alpha_v2;
    e_alpha_v2.id = "mem_alpha_2";
    e_alpha_v2.key = "alpha_config_v2";
    e_alpha_v2.value = "Workspace Alpha migrated to PostgreSQL port 5433 with SSL.";
    e_alpha_v2.category = MemoryCategory::ARCHITECTURAL_DECISION;
    e_alpha_v2.workspace_id = "ws_alpha";
    e_alpha_v2.status = "active";
    e_alpha_v2.updated_at = 1700000030;
    assert(engine.store().put_memory(e_alpha_v2));

    assert(engine.supersede_memory("alpha_config", "alpha_config_v2"));

    MemoryEntry old_alpha;
    assert(engine.store().get_memory("alpha_config", old_alpha));
    assert(old_alpha.status == "superseded");
    assert(old_alpha.supersedes_id == "alpha_config_v2");

    // Active query must return v2 and NOT the superseded v1
    auto hits_active = engine.query_memories_vector(q_vec, 10, 0.1f, filter_alpha);
    bool found_v2 = false;
    for (const auto& h : hits_active) {
        assert(h.first.status == "active");
        if (h.first.key == "alpha_config_v2") found_v2 = true;
        assert(h.first.key != "alpha_config"); // Old one excluded
    }
    assert(found_v2);

    std::filesystem::remove(test_db);
    std::cout << "  -> PASSED" << std::endl;
}

void test_benchmark_memory_latency() {
    std::cout << "[Benchmark 1] Memory Store Insertion & Retrieval Latency..." << std::endl;
    std::string test_db = "benchmark_mem.db";
    std::filesystem::remove(test_db);

    MemoryEngine engine;
    assert(engine.init(test_db));

    auto start_write = std::chrono::high_resolution_clock::now();
    int iterations = 1000;
    for (int i = 0; i < iterations; ++i) {
        MemoryEntry e;
        e.id = "mem_bench_" + std::to_string(i);
        e.key = "key_" + std::to_string(i);
        e.value = "Benchmark value for memory latency testing.";
        e.category = MemoryCategory::RULE;
        e.confidence = 1.0f;
        e.updated_at = 1700000000;
        engine.store().put_memory(e);
    }
    auto end_write = std::chrono::high_resolution_clock::now();
    double write_ms = std::chrono::duration<double, std::milli>(end_write - start_write).count() / iterations;

    auto start_read = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        MemoryEntry out;
        engine.store().get_memory("key_" + std::to_string(i), out);
    }
    auto end_read = std::chrono::high_resolution_clock::now();
    double read_ms = std::chrono::duration<double, std::milli>(end_read - start_read).count() / iterations;

    std::cout << "  -> PASSED: Write Latency: " << (write_ms * 1000.0) << " µs/op | Read Latency: " << (read_ms * 1000.0) << " µs/op" << std::endl;

    std::filesystem::remove(test_db);
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << " DenseLite Phase 4A Memory Engine Test Suite     " << std::endl;
    std::cout << "=================================================" << std::endl;

    test_memory_add_and_retrieve();
    test_memory_update_no_conflict();
    test_memory_delete();
    test_memory_ignore_noise();
    test_session_compaction_and_archive();
    test_memory_recall_and_context_assembly();
    test_persistence_across_restarts();
    test_vector_memory_search();
    test_scoped_living_memory_and_provenance();

    test_benchmark_memory_latency();

    std::cout << "=================================================" << std::endl;
    std::cout << " All Phase 4A Memory Tests PASSED Successfully!  " << std::endl;
    std::cout << "=================================================" << std::endl;
    return 0;
}
