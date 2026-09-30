#include "settings_types.hpp"
#include "settings_manager.hpp"
#include "path_service.hpp"
#include "model_registry_db.hpp"
#include "model_inspector.hpp"
#include "database_migrator.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cstdlib>

#define REQUIRE(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
            std::exit(1); \
        } \
    } while (0)

void test_storage_config_and_tilde() {
    std::cout << "[Test 1] Verifying StorageConfig and tilde expansion..." << std::endl;
    std::string expanded = PathService::expand_user("~/my_models/qwen.gguf");
    REQUIRE(expanded.find('~') == std::string::npos, "Tilde must be expanded");
    REQUIRE(expanded.find("/my_models/qwen.gguf") != std::string::npos, "Path suffix preserved");

    std::string sandbox = "/tmp/denselite_storage_test";
    std::filesystem::remove_all(sandbox);
    DatabaseMigrator::ensure_all_databases_ready(sandbox, false);

    SettingsManager mgr;
    REQUIRE(mgr.init(sandbox + "/src/databases/denselite_settings.db"), "Init DB failed");

    StorageConfig sc = mgr.get_storage_config();
    REQUIRE(sc.models_dir == "~/.denselite/models", "Default models dir must match");
    REQUIRE(sc.data_dir == "~/.denselite/data", "Default data dir must match");

    sc.models_dir = "/custom/models";
    sc.data_dir = "/custom/data";
    mgr.set_storage_config(sc);

    StorageConfig sc2 = mgr.get_storage_config();
    REQUIRE(sc2.models_dir == "/custom/models", "Models dir updated");
    REQUIRE(sc2.data_dir == "/custom/data", "Data dir updated");
    REQUIRE(PathService::instance().get_models_dir() == "/custom/models", "PathService synced");
    REQUIRE(PathService::instance().get_data_dir() == "/custom/data", "PathService data dir synced");

    InferenceConfig ic = mgr.get_inference_config();
    REQUIRE(ic.enable_context_injection == true, "Context injection default true");
    ic.enable_context_injection = false;
    mgr.set_inference_config(ic);
    REQUIRE(mgr.get_inference_config().enable_context_injection == false, "Context injection toggled to false");

    std::filesystem::remove_all(sandbox);
    std::cout << "  Passed!" << std::endl;
}

void test_model_registry_crud_and_multi_role() {
    std::cout << "[Test 2] Verifying ModelRegistryDB CRUD and multi-role assignment..." << std::endl;
    std::string sandbox = "/tmp/denselite_registry_test";
    std::filesystem::remove_all(sandbox);
    DatabaseMigrator::ensure_all_databases_ready(sandbox, false);
    std::string db_path = sandbox + "/src/databases/denselite_settings.db";
    sqlite3* test_db = nullptr;
    if (sqlite3_open(db_path.c_str(), &test_db) == SQLITE_OK) {
        sqlite3_exec(test_db, "DELETE FROM model_roles;", nullptr, nullptr, nullptr);
        sqlite3_close(test_db);
    }

    LocalModelRecord m1;
    m1.model_id = "qwen_shared";
    m1.file_path = "~/models/qwen2.5-1.5b.gguf";
    m1.architecture = "qwen2";
    m1.param_count = 1540000000ULL;
    m1.param_size_str = "1.54B";
    m1.quant_type = "Q4_0";
    m1.context_length = 32768;
    m1.is_verified = true;

    REQUIRE(ModelRegistryDB::register_model(db_path, m1), "Register model failed");

    LocalModelRecord fetched;
    REQUIRE(ModelRegistryDB::get_model(db_path, "qwen_shared", fetched), "Get model failed");
    REQUIRE(fetched.model_id == "qwen_shared", "Model ID matches");
    REQUIRE(fetched.architecture == "qwen2", "Model architecture matches");
    REQUIRE(fetched.param_count == 1540000000ULL, "Parameter count matches");

    // Multi-role assignment: map the SAME model to general, coder, and compressor
    REQUIRE(ModelRegistryDB::bind_role(db_path, "general", "qwen_shared", true), "Bind general failed");
    REQUIRE(ModelRegistryDB::bind_role(db_path, "coder", "qwen_shared", true), "Bind coder failed");
    REQUIRE(ModelRegistryDB::bind_role(db_path, "compressor", "qwen_shared", true), "Bind compressor failed");

    REQUIRE(ModelRegistryDB::get_model_for_role(db_path, "general") == "qwen_shared", "Role general matches");
    REQUIRE(ModelRegistryDB::get_model_for_role(db_path, "coder") == "qwen_shared", "Role coder matches");
    REQUIRE(ModelRegistryDB::get_model_for_role(db_path, "compressor") == "qwen_shared", "Role compressor matches");

    auto bindings = ModelRegistryDB::get_all_role_bindings(db_path);
    REQUIRE(bindings.size() == 3, "Must have 3 active role bindings");

    // Deactivate role coder
    REQUIRE(ModelRegistryDB::set_role_active(db_path, "coder", false), "Deactivate coder failed");
    REQUIRE(ModelRegistryDB::get_model_for_role(db_path, "coder").empty(), "Inactive coder must return empty");
    REQUIRE(ModelRegistryDB::get_all_role_bindings(db_path, true).size() == 2, "2 active bindings");
    REQUIRE(ModelRegistryDB::get_all_role_bindings(db_path, false).size() == 3, "3 total bindings");
    REQUIRE(ModelRegistryDB::set_role_active(db_path, "coder", true), "Reactivate coder failed");
    REQUIRE(ModelRegistryDB::get_model_for_role(db_path, "coder") == "qwen_shared", "Coder reactivated");

    // Unbind one role
    REQUIRE(ModelRegistryDB::unbind_role(db_path, "compressor"), "Unbind compressor failed");
    REQUIRE(ModelRegistryDB::get_model_for_role(db_path, "compressor").empty(), "Compressor role unmapped");
    REQUIRE(ModelRegistryDB::get_model_for_role(db_path, "general") == "qwen_shared", "General still mapped");

    std::filesystem::remove_all(sandbox);
    std::cout << "  Passed!" << std::endl;
}

void test_model_inspector_logic() {
    std::cout << "[Test 3] Verifying ModelInspector format and edge suitability logic..." << std::endl;
    // Test on non-existent file
    auto res_err = ModelInspector::inspect("/tmp/non_existent_model.gguf");
    REQUIRE(!res_err.is_valid, "Non-existent file should be invalid");

    // Synthesize a minimal valid GGUF header for inspector testing
    std::string dummy_gguf = "/tmp/test_dummy_model.gguf";
    std::ofstream out(dummy_gguf, std::ios::binary);
    uint32_t magic = 0x46554747; // "GGUF"
    uint32_t version = 3;
    uint64_t tensor_count = 0;
    uint64_t kv_count = 1;
    out.write(reinterpret_cast<char*>(&magic), 4);
    out.write(reinterpret_cast<char*>(&version), 4);
    out.write(reinterpret_cast<char*>(&tensor_count), 8);
    out.write(reinterpret_cast<char*>(&kv_count), 8);

    // Write KV: general.architecture = "qwen2"
    std::string k = "general.architecture";
    uint64_t klen = k.size();
    out.write(reinterpret_cast<char*>(&klen), 8);
    out.write(k.data(), klen);
    uint32_t type_str = 8; // STRING
    out.write(reinterpret_cast<char*>(&type_str), 4);
    std::string val = "qwen2";
    uint64_t vlen = val.size();
    out.write(reinterpret_cast<char*>(&vlen), 8);
    out.write(val.data(), vlen);
    out.close();

    auto insp = ModelInspector::inspect(dummy_gguf);
    REQUIRE(insp.is_valid, "Dummy GGUF must validate");
    REQUIRE(insp.architecture == "qwen2", "Architecture must be qwen2");
    REQUIRE(insp.is_supported_edge_size(), "0 parameters is <= 1.85B");
    REQUIRE(insp.is_role_compatible("general"), "Qwen2 compatible with general");
    REQUIRE(insp.is_role_compatible("coder"), "Qwen2 compatible with coder");
    REQUIRE(insp.is_role_compatible("compressor"), "Qwen2 compatible with compressor");
    REQUIRE(insp.is_role_compatible("router"), "Qwen2 compatible with router");
    REQUIRE(!insp.is_role_compatible("embedding"), "Qwen2 NOT compatible with embedding");
    REQUIRE(!insp.is_role_compatible("audio_stt"), "Qwen2 NOT compatible with audio_stt");

    std::filesystem::remove(dummy_gguf);
    std::cout << "  Passed!" << std::endl;
}

int main() {
    std::cout << "=== Running DenseLite Model Registry & Inspector Test Suite ===" << std::endl;
    test_storage_config_and_tilde();
    test_model_registry_crud_and_multi_role();
    test_model_inspector_logic();
    std::cout << "All Model Registry tests passed successfully (100%)!" << std::endl;
    return 0;
}
