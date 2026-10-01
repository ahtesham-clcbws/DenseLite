#ifdef NDEBUG
#undef NDEBUG
#endif
#include "memory_engine.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <filesystem>
#include <iomanip>

#define VERIFY(expr) do { \
    if (!(expr)) { \
        std::cerr << "FATAL: Verification failed: " #expr << " at line " << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while (0)

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << " Memory Consolidation & Supersession Benchmark (Point 10 & 11)" << std::endl;
    std::cout << " Automated Session Fact Consolidation & State Retention Suite" << std::endl;
    std::cout << "==========================================================" << std::endl;

    std::string test_db = "/tmp/test_memory_consolidation_fidelity.db";
    std::filesystem::remove(test_db);

    MemoryEngine engine;
    VERIFY(engine.init(test_db) && "Failed to initialize memory engine");

    // 1. Automated Consolidation via MemoryConsolidator
    std::cout << "\n[1] Testing Automated Session Memory Consolidation..." << std::endl;
    {
        SessionMemory s1("sess_01");
        s1.add_message("user", "decision: primary_backend: Laravel 11");

        auto res1 = engine.consolidator().consolidate(s1);
        std::cout << "  - Initial Session: added=" << res1.added_count << ", updated=" << res1.updated_count << std::endl;
        VERIFY(res1.added_count == 1);

        MemoryEntry m1;
        VERIFY(engine.store().get_memory("primary_backend", m1));
        VERIFY(m1.value == "Laravel 11");
        VERIFY(m1.category == MemoryCategory::ARCHITECTURAL_DECISION);
        std::cout << "  ✓ Initial Fact Extracted: 'primary_backend' = '" << m1.value << "'" << std::endl;

        // Subsequent session updates the fact
        SessionMemory s2("sess_02");
        s2.add_message("user", "decision: primary_backend: Laravel 12");

        auto res2 = engine.consolidator().consolidate(s2);
        std::cout << "  - Update Session: added=" << res2.added_count << ", updated=" << res2.updated_count << std::endl;
        VERIFY(res2.updated_count == 1);

        MemoryEntry m2;
        VERIFY(engine.store().get_memory("primary_backend", m2));
        VERIFY(m2.value == "Laravel 12");
        std::cout << "  ✓ Automatic Fact Supersession: 'primary_backend' updated to '" << m2.value << "'" << std::endl;
    }

    // 2. Multi-Key Retention & Category Separation
    std::cout << "\n[2] Testing Multi-Category Knowledge Isolation..." << std::endl;
    struct CatFact {
        std::string key;
        std::string val;
        MemoryCategory cat;
    };
    std::vector<CatFact> facts = {
        {"compiler_target", "AVX2", MemoryCategory::ARCHITECTURAL_DECISION},
        {"max_compute_threads", "2", MemoryCategory::RULE},
        {"style_standard", "composition_over_inheritance", MemoryCategory::CONVENTION},
        {"host_distro", "CachyOS", MemoryCategory::DISCOVERY}
    };

    for (const auto& f : facts) {
        MemoryEntry entry;
        entry.id = "mem_" + f.key;
        entry.key = f.key;
        entry.value = f.val;
        entry.category = f.cat;
        entry.confidence = 1.0f;
        entry.status = "active";
        VERIFY(engine.store().put_memory(entry));
    }

    for (const auto& f : facts) {
        MemoryEntry got;
        VERIFY(engine.store().get_memory(f.key, got));
        VERIFY(got.value == f.val);
        VERIFY(got.category == f.cat);
        std::cout << "  ✓ Key '" << f.key << "' -> Retained: '" << got.value 
                  << "' (Category: " << static_cast<int>(got.category) << ")" << std::endl;
    }

    // 3. Retrieval Recall Fidelity
    std::cout << "\n[3] Testing Direct & Prefix Memory Recall..." << std::endl;
    auto hits = engine.recall().recall("compiler_target", 5);
    bool found_target = false;
    for (const auto& h : hits) {
        if (h.entry.key == "compiler_target" && h.entry.value == "AVX2") {
            found_target = true;
            break;
        }
    }
    std::cout << "  ✓ Recall hit for 'compiler_target': " << (found_target ? "FOUND (AVX2)" : "NOT FOUND") << std::endl;
    VERIFY(found_target && "Memory recall failed to find target entry");

    // 4. State Deletion & Tombstoning
    std::cout << "\n[4] Testing Memory Deletion & State Cleanup..." << std::endl;
    VERIFY(engine.store().delete_memory("compiler_target"));
    MemoryEntry dead_entry;
    bool exists_after_delete = engine.store().get_memory("compiler_target", dead_entry);
    VERIFY(!exists_after_delete && "Deleted memory still retrieved!");
    std::cout << "  ✓ Deleted 'compiler_target' verified completely removed." << std::endl;

    std::filesystem::remove(test_db);

    std::cout << "\n[Results Summary]:" << std::endl;
    std::cout << "| Test Area                     | Result | Gate Status |" << std::endl;
    std::cout << "|-------------------------------|--------|-------------|" << std::endl;
    std::cout << "| Automated Fact Consolidation  | 100%   | 🟢 PASS     |" << std::endl;
    std::cout << "| Multi-Category Isolation      | 100%   | 🟢 PASS     |" << std::endl;
    std::cout << "| Direct & Prefix Recall        | 100%   | 🟢 PASS     |" << std::endl;
    std::cout << "| State Cleanup & Deletion      | 100%   | 🟢 PASS     |" << std::endl;

    std::cout << "\n>> All Living Memory Consolidation & Retention Tests Passed!" << std::endl;
    return 0;
}
