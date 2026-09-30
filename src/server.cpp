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
#include <random>
#include "settings/model_registry_db.hpp"
#include "path_service.hpp"
#include "vector_search.hpp"
#include "infer.hpp"

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    // Resolve the DenseLite base directory dynamically via PathService
    std::string base_dir = PathService::instance().get_base_dir();

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

    DenseModel* nomic_ptr = nullptr;
    if (resident_models.count("nomic")) nomic_ptr = &resident_models["nomic"];
    else if (resident_models.count("embedding")) nomic_ptr = &resident_models["embedding"];
    else if (resident_models.count("nomic_embed")) nomic_ptr = &resident_models["nomic_embed"];
    if (nomic_ptr) {
        VectorSearch::set_embedding_model(nomic_ptr);
        VectorSearch::set_embedding_fn([nomic_ptr](const std::string& text) {
            if (!nomic_ptr || nomic_ptr->vocab.tokens.empty()) return std::vector<float>{};
            auto tokens = tokenize(nomic_ptr->vocab, text);
            return compute_embedding(*nomic_ptr, tokens);
        });
    }
    
    static httplib::Server* svr_ptr = nullptr;
    httplib::Server svr;
    svr_ptr = &svr;
    
    std::signal(SIGINT, [](int) {
        if (svr_ptr) svr_ptr->stop();
    });
    std::signal(SIGTERM, [](int) {
        if (svr_ptr) svr_ptr->stop();
    });
    
    std::cout << "[DEBUG] Loading env router..." << std::endl;
    SQLiteRouter router(DatabasePaths::settings_db(base_dir));
    router.load_env(base_dir + "/.env");

    std::cout << "[DEBUG] Creating DenseLiteEngine..." << std::endl;
    DenseLiteEngine engine(resident_models, router, base_dir);
    
    std::cout << "[DEBUG] Getting server config..." << std::endl;
    auto srv_cfg = SettingsManager::instance().get_server_config();

    std::cout << "[DEBUG] Server Config Host: " << srv_cfg.host << ", Port: " << srv_cfg.port << std::endl;

    // P0-Security: Auto-generate API secret if auth is enabled but secret is empty
    if (srv_cfg.enable_api_auth && srv_cfg.api_secret_key.empty()) {
        static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<int> dist(0, sizeof(charset) - 2);
        std::string generated(48, '\0');
        for (char& c : generated) c = charset[dist(rng)];
        srv_cfg.api_secret_key = generated;
        SettingsManager::instance().set_server_config(srv_cfg);
        std::cout << "[Security] API secret auto-generated. Retrieve via settings API." << std::endl;
    }

    // P0-Security: Enforce HTTP payload max length
    svr.set_payload_max_length(static_cast<size_t>(srv_cfg.max_payload_mb) * 1024 * 1024);

    std::string cors_origin = srv_cfg.cors_allowed_origins.empty() ? "http://localhost" : srv_cfg.cors_allowed_origins;
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
            std::string state = "registered";
            if (id == "denselite") {
                state = "ready";
            } else if (resident_models.find(id) != resident_models.end()) {
                state = "loaded";
            }
            json_str += "{\"id\":\"" + id + "\",\"object\":\"model\",\"created\":1700000000,\"owned_by\":\"denselite\",\"state\":\"" + state + "\"}";
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
