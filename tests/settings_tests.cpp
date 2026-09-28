#include "settings_types.hpp"
#include "settings_db.hpp"
#include "settings_manager.hpp"
#include "path_service.hpp"
#include <iostream>
#include <filesystem>
#include <cstdlib>

#define REQUIRE(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_db_wal_mode() {
    std::cout << "[Test 1] Verifying SQLite WAL mode and table creation..." << std::endl;
    std::string test_db = "test_settings_wal.db";
    std::filesystem::remove(test_db);
    std::filesystem::remove(test_db + "-wal");
    std::filesystem::remove(test_db + "-shm");

    SettingsDB db;
    bool ok = db.init(test_db);
    REQUIRE(ok, "Database initialization failed");
    REQUIRE(db.is_open(), "Database should be open");

    std::string mode = db.get_journal_mode();
    std::cout << "  Database journal mode: " << mode << std::endl;
    REQUIRE((mode == "wal" || mode == "WAL"), "Database must operate in WAL mode");

    SettingRecord rec{"core", "max_threads", "8", "int", 123456789};
    REQUIRE(db.upsert(rec), "Upsert should succeed");
    REQUIRE(db.count() == 1, "Count should be 1");

    SettingRecord out;
    REQUIRE(db.get("core", "max_threads", out), "Get should find record");
    REQUIRE(out.value == "8", "Value should match");
    REQUIRE(out.val_type == "int", "Type should match");

    // Upsert update
    rec.value = "16";
    REQUIRE(db.upsert(rec), "Second upsert should succeed");
    REQUIRE(db.count() == 1, "Count should still be 1");
    REQUIRE(db.get("core", "max_threads", out), "Get should succeed");
    REQUIRE(out.value == "16", "Value should be updated to 16");

    db.close();
    std::filesystem::remove(test_db);
    std::filesystem::remove(test_db + "-wal");
    std::filesystem::remove(test_db + "-shm");
    std::cout << "  Passed!" << std::endl;
}

void test_manager_seeding_and_types() {
    std::cout << "[Test 2] Verifying SettingsManager auto-seeding and DTO contracts..." << std::endl;
    std::string test_db = "test_settings_mgr.db";
    std::filesystem::remove(test_db);
    std::filesystem::remove(test_db + "-wal");
    std::filesystem::remove(test_db + "-shm");

    SettingsManager mgr;
    REQUIRE(mgr.init(test_db), "Manager init failed");
    std::string mode = mgr.get_journal_mode();
    REQUIRE((mode == "wal" || mode == "WAL"), "Manager DB must be in WAL mode");

    // Check seeded defaults
    REQUIRE(mgr.get_version() == "3.3.0", "Version must be 3.3.0");
    ServerConfig sc = mgr.get_server_config();
    REQUIRE(sc.version == "3.3.0", "ServerConfig version must be 3.3.0");
    REQUIRE(sc.port == 9501, "Default port must be 9501");
    REQUIRE(sc.host == "0.0.0.0", "Default host must be 0.0.0.0");

    ResourceConfig rc = mgr.get_resource_config();
    REQUIRE(rc.ram_budget_percent >= 0.44f && rc.ram_budget_percent <= 0.46f, "RAM budget must be ~0.45");
    REQUIRE(rc.max_kv_tokens == 65536, "Max KV tokens must be 64K");

    InferenceConfig ic = mgr.get_inference_config();
    REQUIRE(ic.needle3_mode == "hybrid", "Needle3 mode default must be hybrid");
    REQUIRE(ic.enable_tool_dedup == true, "Tool dedup must be enabled");

    LoggingConfig lc = mgr.get_logging_config();
    REQUIRE(lc.level == "INFO", "Log level default must be INFO");

    // Mutate and check cache + DB
    sc.port = 9800;
    sc.threads = 12;
    mgr.set_server_config(sc);

    ServerConfig sc2 = mgr.get_server_config();
    REQUIRE(sc2.port == 9800, "Port must be updated to 9800");
    REQUIRE(sc2.threads == 12, "Threads must be updated to 12");

    // Primitives
    mgr.set_string("custom", "agent_name", "DenseLite-Quantum");
    mgr.set_int("custom", "timeout_ms", 3500);
    mgr.set_float("custom", "temp_boost", 1.25f);
    mgr.set_bool("custom", "debug_flag", true);

    REQUIRE(mgr.get_string("custom", "agent_name") == "DenseLite-Quantum", "Agent name matches");
    REQUIRE(mgr.get_int("custom", "timeout_ms") == 3500, "Timeout matches");
    REQUIRE(mgr.get_float("custom", "temp_boost") >= 1.24f, "Float matches");
    REQUIRE(mgr.get_bool("custom", "debug_flag") == true, "Bool matches");

    // Re-open in a fresh manager instance to ensure disk persistence
    SettingsManager mgr2;
    REQUIRE(mgr2.init(test_db), "Fresh manager init failed");
    REQUIRE(mgr2.get_server_config().port == 9800, "Persisted port must be 9800");
    REQUIRE(mgr2.get_server_config().threads == 12, "Persisted threads must be 12");
    REQUIRE(mgr2.get_string("custom", "agent_name") == "DenseLite-Quantum", "Persisted agent name matches");
    REQUIRE(mgr2.get_bool("custom", "debug_flag") == true, "Persisted bool matches");

    std::filesystem::remove(test_db);
    std::filesystem::remove(test_db + "-wal");
    std::filesystem::remove(test_db + "-shm");
    std::cout << "  Passed!" << std::endl;
}

void test_path_service() {
    std::cout << "[Test 3] Verifying PathService centralized path resolution..." << std::endl;
    auto& ps = PathService::instance();
    std::string orig_base = ps.get_base_dir();

    ps.set_base_dir("/tmp/test_denselite_root");
    REQUIRE(ps.get_base_dir() == "/tmp/test_denselite_root", "Base dir matches");
    REQUIRE(ps.settings_db() == "/tmp/test_denselite_root/src/databases/denselite_settings.db", "Settings DB path matches");
    REQUIRE(ps.state_db() == "/tmp/test_denselite_root/src/databases/denselite_state.db", "State DB path matches");
    REQUIRE(ps.memory_db() == "/tmp/test_denselite_root/src/databases/denselite_memory.db", "Memory DB path matches");
    REQUIRE(ps.symbols_db() == "/tmp/test_denselite_root/src/databases/denselite_symbols.db", "Symbols DB path matches");
    REQUIRE(ps.env_file() == "/tmp/test_denselite_root/.env", "Env file path matches");

    // Dynamic runtime override of database directory
    ps.set_database_dir("/tmp/custom_db_storage");
    REQUIRE(ps.settings_db() == "/tmp/custom_db_storage/denselite_settings.db", "Custom settings DB path matches");
    REQUIRE(ps.memory_db() == "/tmp/custom_db_storage/denselite_memory.db", "Custom memory DB path matches");

    // Restore original base dir
    ps.set_base_dir(orig_base);
    std::cout << "  Passed!" << std::endl;
}

int main() {
    std::cout << "=== Running DenseLite Settings Engine Test Suite ===" << std::endl;
    test_db_wal_mode();
    test_manager_seeding_and_types();
    test_path_service();
    std::cout << "All Settings Engine tests passed successfully (100%)!" << std::endl;
    return 0;
}
