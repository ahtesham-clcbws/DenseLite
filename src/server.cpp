#include "httplib.h"
#include "gguf_parser.hpp"
#include "DenseLiteEngine.hpp"
#include "HardwareManager.hpp"
#include "ModelLoader.hpp"
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

    auto env = ModelLoader::load_env(base_dir + "/.env");
    std::string version = env.count("DENSELITE_VERSION") ? env["DENSELITE_VERSION"] : "Unknown";
    
    std::cout << "Starting DenseLite Gateway (V" << version << ")..." << std::endl;
    HardwareManager::enforce_limits();
    
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
    
    SQLiteRouter router(base_dir + "/denselite_state.db");
    router.load_env(base_dir + "/.env");

    DenseLiteEngine engine(resident_models, router);

    svr.Post("/v1/chat/completions", [&](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[Gateway] Received request (" << req.body.size() << " bytes)" << std::endl;
        engine.process(req.body, res);
    });
    
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });
    
    std::cout << "[API] Server listening on http://localhost:9501" << std::endl;
    svr.listen("0.0.0.0", 9501);
    
    std::cout << "Shutting down..." << std::endl;
    for (auto& pair : resident_models) {
        free_gguf_model(pair.second);
    }
    return 0;
}
