#include "memory_engine.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <filesystem>
#include <iomanip>

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << " Memory Consolidation & Supersession Benchmark (Point 10)" << std::endl;
    std::cout << " Temporal Contradiction Resolution & Retention Test Suite" << std::endl;
    std::cout << "==========================================================" << std::endl;

    std::string test_db = "/tmp/test_memory_consolidation_fidelity.db";
    std::filesystem::remove(test_db);

    MemoryEngine engine;
    assert(engine.init(test_db) && "Failed to initialize memory engine");

    // 1. Temporal Sequence of Supersessions
    std::cout << "\n[1] Testing Sequential Fact Supersession..." << std::endl;
    struct FactUpdate {
        std::string id;
        std::string key;
        std::string value;
        MemoryCategory cat;
        float confidence;
    };

    std::vector<FactUpdate> timeline = {
        {"f_v1", "user_primary_language", "Python", MemoryCategory::DISCOVERY, 0.90f},
        {"f_v2", "user_primary_language", "C++", MemoryCategory::DISCOVERY, 0.95f},
        {"f_v3", "user_primary_language", "Rust", MemoryCategory::DISCOVERY, 0.99f}
    };

    for (size_t t = 0; t < timeline.size(); ++t) {
        const auto& fu = timeline[t];
        MemoryEntry entry;
        entry.id = fu.id;
        entry.key = fu.key;
        entry.value = fu.value;
        entry.category = fu.cat;
        entry.confidence = fu.confidence;
        if (t > 0) {
            entry.supersedes_id = timeline[t - 1].id;
        }

        bool ok = engine.store().put_memory(entry);
        assert(ok && "Failed to put memory entry");

        MemoryEntry retrieved;
        assert(engine.store().get_memory("user_primary_language", retrieved));
        assert(retrieved.value == fu.value);
        assert(retrieved.confidence == fu.confidence);

        std::cout << "  - Timestep " << (t + 1) << ": Superseded to '" << retrieved.value
                  << "' (Confidence: " << retrieved.confidence << ") -> OK" << std::endl;
    }

    // Verify final state matches latest fact
    MemoryEntry final_entry;
    assert(engine.store().get_memory("user_primary_language", final_entry));
    assert(final_entry.value == "Rust");
    std::cout << "  -> Final State Verified: 'Rust' is active state." << std::endl;

    // 2. Category Isolation & Multi-Key Retention
    std::cout << "\n[2] Testing Multi-Key Retention & Category Separation..." << std::endl;
    std::vector<FactUpdate> multi_domain = {
        {"arch_1", "compiler_target", "AVX2", MemoryCategory::ARCHITECTURAL_DECISION, 1.0f},
        {"sys_1", "max_threads", "2", MemoryCategory::RULE, 1.0f},
        {"disc_1", "os_distribution", "CachyOS", MemoryCategory::DISCOVERY, 0.95f}
    };

    for (const auto& md : multi_domain) {
        MemoryEntry entry;
        entry.id = md.id;
        entry.key = md.key;
        entry.value = md.value;
        entry.category = md.cat;
        entry.confidence = md.confidence;
        bool ok = engine.store().put_memory(entry);
        assert(ok && "Failed to put multi-domain entry");
    }

    for (const auto& md : multi_domain) {
        MemoryEntry got;
        assert(engine.store().get_memory(md.key, got));
        assert(got.value == md.value);
        assert(got.category == md.cat);
        std::cout << "  - Key '" << md.key << "' -> Retained: '" << got.value << "' (Category: " 
                  << static_cast<int>(got.category) << ") -> OK" << std::endl;
    }

    // 3. Retrieval Recall Fidelity
    std::cout << "\n[3] Testing Memory Recall Engine..." << std::endl;
    auto recall_hits = engine.recall().recall("compiler_target", 5);
    bool found_arch = false;
    for (const auto& h : recall_hits) {
        if (h.entry.key == "compiler_target" && h.entry.value == "AVX2") {
            found_arch = true;
            break;
        }
    }
    std::cout << "  - Direct Recall hit: " << (found_arch ? "YES (AVX2)" : "NO") << std::endl;
    assert(found_arch && "Memory recall failed to find target key");

    // 4. Memory deletion / tombstoning
    std::cout << "\n[4] Testing Memory Deletion & State Cleanup..." << std::endl;
    assert(engine.store().delete_memory("compiler_target"));
    MemoryEntry dead_entry;
    bool exists_after_delete = engine.store().get_memory("compiler_target", dead_entry);
    assert(!exists_after_delete && "Deleted memory still retrieved!");
    std::cout << "  - Deleted 'compiler_target' verified removed." << std::endl;

    std::filesystem::remove(test_db);

    std::cout << "\n[Results Summary]:" << std::endl;
    std::cout << "| Test Area                     | Result | Gate Status |" << std::endl;
    std::cout << "|-------------------------------|--------|-------------|" << std::endl;
    std::cout << "| Sequential Fact Supersession  | 100%   | 🟢 PASS     |" << std::endl;
    std::cout << "| Multi-Category Isolation      | 100%   | 🟢 PASS     |" << std::endl;
    std::cout << "| Retrieval Fidelity (Recall)   | 100%   | 🟢 PASS     |" << std::endl;
    std::cout << "| State Cleanup & Deletion      | 100%   | 🟢 PASS     |" << std::endl;

    std::cout << "\n>> All Living Memory Consolidation & Supersession Tests Passed!" << std::endl;
    return 0;
}
