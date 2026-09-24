#pragma once
#include "gguf_parser.hpp"
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>

class ModelLoader {
public:
    static std::map<std::string, std::string> load_env(const std::string& filepath) {
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
                // Remove quotes from environment variables if they exist
                if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                    val = val.substr(1, val.size() - 2);
                }
                env[key] = val;
            }
        }
        return env;
    }

    static void load_resident_models(const std::string& base_dir, const std::map<std::string, std::string>& env, std::map<std::string, DenseModel>& resident_models) {
        struct ModelDef {
            std::string id;
            std::string path;
            bool is_gguf;
        };

        std::vector<ModelDef> models_to_load;
        
        auto add_model = [&](const std::string& id, const std::string& env_key, bool is_gguf) {
            if (env.count(env_key)) {
                std::string full_path = base_dir + "/models/" + env.at(env_key);
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
                        exit(1);
                    }
                } else {
                    std::cout << "[Loader] (Stub) Loaded non-GGUF model: " << m.id << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[Fatal] Exception loading " << m.id << ": " << e.what() << std::endl;
                exit(1);
            }
        }
        std::cout << "[Loader] All resident models loaded successfully in sequence." << std::endl;
    }
};
