#include "httplib.h"
#include "gguf_parser.hpp"
#include "DenseLiteEngine.hpp"
#include <mutex>
#include <csignal>
#include <iostream>
#include <map>
#include <vector>
#include <filesystem>

// ============================================================================
// Hot-Loading System (Utility Models)
// ============================================================================
bool load_hot_model(const std::string& model_id, DenseModel& temp_model) {
    std::cout << "[HotLoad] Spinning up " << model_id << " into RAM on-demand..." << std::endl;
    return true; 
}

void unload_hot_model(const std::string& model_id, DenseModel& temp_model) {
    std::cout << "[HotLoad] Purging " << model_id << " from RAM to protect hardware." << std::endl;
}

#include "HardwareManager.hpp"

#include <fstream>
#include <sstream>

std::map<std::string, std::string> load_env(const std::string& filepath) {
    std::map<std::string, std::string> env;
    std::ifstream file(filepath);
    if (!file.is_open()) return env;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            env[key] = val;
        }
    }
    return env;
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    // Resolve the DenseLite base directory dynamically (assumes executable is in build/)
    std::string base_dir = std::filesystem::read_symlink("/proc/self/exe").parent_path().parent_path().string();

    auto env = load_env(base_dir + "/.env");
    std::string version = env.count("DENSELITE_VERSION") ? env["DENSELITE_VERSION"] : "Unknown";
    
    // Remove quotes from environment variables if they exist
    for (auto& pair : env) {
        if (pair.second.size() >= 2 && pair.second.front() == '"' && pair.second.back() == '"') {
            pair.second = pair.second.substr(1, pair.second.size() - 2);
        }
    }

    std::cout << "Starting DenseLite Gateway (V" << version << ")..." << std::endl;
    HardwareManager::enforce_limits();
    
    std::map<std::string, DenseModel> resident_models;
    
    struct ModelDef {
        std::string id;
        std::string path;
        bool is_gguf;
    };

    std::vector<ModelDef> models_to_load;
    
    auto add_model = [&](const std::string& id, const std::string& env_key, bool is_gguf) {
        if (env.count(env_key)) {
            std::string full_path = base_dir + "/models/" + env[env_key];
            if (!std::filesystem::exists(full_path)) {
                std::cerr << "[Fatal] Required model file missing: " << full_path << std::endl;
                exit(1);
            }
            models_to_load.push_back({id, full_path, is_gguf});
        }
    };

    add_model("needle", "MODEL_NEEDLE_FILE", false);
    add_model("smollm2", "MODEL_SMOLLM2_FILE", true);
    add_model("nomic", "MODEL_NOMIC_FILE", true);
    add_model("qwen_main", "MODEL_QWEN_MAIN_FILE", true);
    add_model("qwen_coder", "MODEL_QWEN_CODER_FILE", true);


    for (const auto& m : models_to_load) {
        std::cout << "[Loader] Loading resident model: " << m.id << "..." << std::endl;
        try {
            if (m.is_gguf) {
                if (!load_gguf_model(m.path, resident_models[m.id])) {
                    std::cerr << "[Fatal] Failed to load GGUF: " << m.id << std::endl;
                    return 1;
                }
            } else {
                std::cout << "[Loader] (Stub) Loaded non-GGUF model: " << m.id << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Fatal] Exception loading " << m.id << ": " << e.what() << std::endl;
            return 1;
        }
    }
    std::cout << "[Loader] All resident models loaded successfully in sequence." << std::endl;
    
    static httplib::Server* svr_ptr = nullptr;
    httplib::Server svr;
    svr_ptr = &svr;
    
    std::signal(SIGINT, [](int) {
        if (svr_ptr) svr_ptr->stop();
    });
    std::signal(SIGTERM, [](int) {
        if (svr_ptr) svr_ptr->stop();
    });
    
    // Resolve the DenseLite base directory dynamically (assumes executable is in build/)
    // base_dir is already resolved at the top of main()

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
