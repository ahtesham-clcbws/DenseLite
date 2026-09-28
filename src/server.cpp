#include "httplib.h"
#include "gguf_parser.hpp"
#include "DenseLiteEngine.hpp"
#include "HardwareManager.hpp"
#include "ModelLoader.hpp"
#include "database_migrator.hpp"
#include "database_paths.hpp"
#include "settings_manager.hpp"
#include <mutex>
#include <csignal>
#include <iostream>
#include <map>
#include <vector>
#include <filesystem>

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
    
    SQLiteRouter router(DatabasePaths::state_db(base_dir));
    router.load_env(base_dir + "/.env");

    DenseLiteEngine engine(resident_models, router, base_dir);

    svr.Post("/v1/chat/completions", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[Gateway] Received request (" << req.body.size() << " bytes)" << std::endl;
        engine.process(req.body, res);
    });
    
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });
    
    std::cout << "[API] Server listening on http://localhost:9501" << std::endl;
    if (!svr.listen("0.0.0.0", 9501)) {
        std::cerr << "\n[DenseLite Error] Failed to bind to http://localhost:9501!\n"
                  << "Port 9501 is already occupied by an existing process.\n"
                  << "To stop the running instance, run: pkill -9 -f DenseLite\n"
                  << "Or check the active process with: lsof -i :9501\n" << std::endl;
    }
    
    std::cout << "Shutting down..." << std::endl;
    for (auto& pair : resident_models) {
        free_gguf_model(pair.second);
    }
    return 0;
}
