#include "memory_engine.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>

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

    std::cout << "=================================================" << std::endl;
    std::cout << " All Phase 4A Memory Tests PASSED Successfully!  " << std::endl;
    std::cout << "=================================================" << std::endl;
    return 0;
}
