#include "httplib.h"
#include "gguf_parser.hpp"
#include "DenseLiteEngine.hpp"
#include "HardwareManager.hpp"
#include "ModelLoader.hpp"
#include "database_migrator.hpp"
#include "database_paths.hpp"
#include "model_cli.hpp"
#include <mutex>
#include <csignal>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <filesystem>
#include "settings/model_registry_db.hpp"

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    // Resolve the DenseLite base directory dynamically (assumes executable is in build/)
    std::string base_dir = std::filesystem::read_symlink("/proc/self/exe").parent_path().parent_path().string();

    // Zero-Touch Self-Healing Database Bootstrapper
    DatabaseMigrator::ensure_all_databases_ready(base_dir);
    if (argc > 1 && (std::string(argv[1]) == "--init-db" || std::string(argv[1]) == "--migrate")) {
        std::cout << "[DenseLite] All databases verified and migrated successfully." << std::endl;
        return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "--tray") {
        std::string tray_bin = base_dir + "/build/DenseLiteTray";
        char* const args[] = { const_cast<char*>(tray_bin.c_str()), nullptr };
        execv(tray_bin.c_str(), args);
        return 0;
    }

    if (ModelCli::handle_cli(argc, argv, base_dir)) {
        return 0;
    }

    SettingsManager::instance().init(DatabasePaths::settings_db(base_dir));
    std::string version = SettingsManager::instance().get_version();
    
    std::cout << "Starting DenseLite Gateway (V" << version << ")..." << std::endl;
    HardwareManager::enforce_limits();
    
    auto env = ModelLoader::load_env(base_dir + "/.env");
    std::map<std::string, DenseModel> resident_models;
    ModelLoader::load_resident_models(base_dir, env, resident_models);
    
    static httplib::Server* svr_ptr = nullptr;
    httplib::Server svr;
    svr_ptr = &svr;
    
    std::signal(SIGINT, [](int) {
        if (svr_ptr) svr_ptr->stop();
    });
    std::signal(SIGTERM, [](int) {
        if (svr_ptr) svr_ptr->stop();
    });
    
    SQLiteRouter router(DatabasePaths::settings_db(base_dir));
    router.load_env(base_dir + "/.env");

    DenseLiteEngine engine(resident_models, router, base_dir);

    auto srv_cfg = SettingsManager::instance().get_server_config();
    std::string cors_origin = srv_cfg.cors_allowed_origins.empty() ? "*" : srv_cfg.cors_allowed_origins;
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", cors_origin.c_str()},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
    });

    svr.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    svr.Post("/v1/chat/completions", [&](const httplib::Request& req, httplib::Response& res) {
        auto cfg = SettingsManager::instance().get_server_config();
        if (cfg.enable_api_auth) {
            std::string auth = req.get_header_value("Authorization");
            std::string expected = "Bearer " + cfg.api_secret_key;
            if (auth.empty() || auth != expected) {
                res.status = 401;
                res.set_content("{\"error\":{\"message\":\"Unauthorized: Invalid API key\",\"type\":\"invalid_request_error\"}}", "application/json");
                return;
            }
        }
        std::cout << "[Gateway] Received request (" << req.body.size() << " bytes)" << std::endl;
        engine.process(req.body, res);
    });
    
    svr.Get("/v1/models", [&](const httplib::Request& req, httplib::Response& res) {
        auto cfg = SettingsManager::instance().get_server_config();
        if (cfg.enable_api_auth) {
            std::string auth = req.get_header_value("Authorization");
            if (auth.empty() || auth != "Bearer " + cfg.api_secret_key) {
                res.status = 401;
                res.set_content("{\"error\":{\"message\":\"Unauthorized\",\"type\":\"invalid_request_error\"}}", "application/json");
                return;
            }
        }
        std::string db = DatabasePaths::settings_db(base_dir);
        auto registered = ModelRegistryDB::get_all_models(db);
        std::set<std::string> model_ids = {"denselite"};
        for (const auto& r : registered) model_ids.insert(r.model_id);
        for (const auto& kv : resident_models) model_ids.insert(kv.first);

        std::string json_str = "{\"object\":\"list\",\"data\":[";
        bool first = true;
        for (const auto& id : model_ids) {
            if (!first) json_str += ",";
            first = false;
            json_str += "{\"id\":\"" + id + "\",\"object\":\"model\",\"created\":1700000000,\"owned_by\":\"denselite\"}";
        }
        json_str += "]}";
        res.set_content(json_str, "application/json");
    });

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });
    
    std::cout << "[API] Server listening on http://" << srv_cfg.host << ":" << srv_cfg.port << std::endl;
    if (!svr.listen(srv_cfg.host.c_str(), srv_cfg.port)) {
        std::cerr << "\n[DenseLite Error] Failed to bind to http://" << srv_cfg.host << ":" << srv_cfg.port << "!\n"
                  << "Port " << srv_cfg.port << " is already occupied by an existing process.\n"
                  << "To stop the running instance, run: pkill -9 -f DenseLite\n"
                  << "Or check the active process with: lsof -i :" << srv_cfg.port << "\n" << std::endl;
    }
    
    std::cout << "Shutting down..." << std::endl;
    for (auto& pair : resident_models) {
        free_gguf_model(pair.second);
    }
    return 0;
}
