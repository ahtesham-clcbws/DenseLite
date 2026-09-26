#include "ModelLoader.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>

std::map<std::string, std::string> ModelLoader::load_env(const std::string& filepath) {
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
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                val = val.substr(1, val.size() - 2);
            }
            env[key] = val;
        }
    }
    return env;
}

bool ModelLoader::load_model(const std::string& filepath, DenseModel& model, std::string& error_msg) {
    if (!std::filesystem::exists(filepath)) {
        error_msg = "Model file does not exist: " + filepath;
        return false;
    }

    try {
        if (!load_gguf_model(filepath, model)) {
            error_msg = "GGUF parser failed to parse model at: " + filepath;
            return false;
        }
    } catch (const std::exception& e) {
        error_msg = std::string("Exception during GGUF loading: ") + e.what();
        return false;
    }

    return true;
}

bool ModelLoader::unload_model(DenseModel& model) {
    free_gguf_model(model);
    return true;
}

bool ModelLoader::load_resident_models(const std::string& base_dir,
                                      const std::map<std::string, std::string>& env,
                                      std::map<std::string, DenseModel>& resident_models,
                                      std::string& error_msg) {
    struct ModelDef {
        std::string id;
        std::string path;
        bool is_gguf;
    };

    std::vector<ModelDef> models_to_load;
    auto add_model = [&](const std::string& id, const std::string& env_key, bool is_gguf) {
        if (env.count(env_key)) {
            std::string full_path = base_dir + "/models/" + env.at(env_key);
            models_to_load.push_back({id, full_path, is_gguf});
        }
    };

    add_model("needle", "MODEL_NEEDLE_FILE", false);
    add_model("smollm2", "MODEL_SMOLLM2_FILE", true);
    add_model("nomic", "MODEL_NOMIC_FILE", true);
    add_model("qwen_main", "MODEL_QWEN_MAIN_FILE", true);
    add_model("qwen_coder", "MODEL_QWEN_CODER_FILE", true);

    for (const auto& m : models_to_load) {
        if (m.is_gguf) {
            std::cout << "[Loader] Loading model: " << m.id << " from " << m.path << "..." << std::endl;
            DenseModel loaded_model;
            if (!load_model(m.path, loaded_model, error_msg)) {
                std::cerr << "[Loader Error] " << error_msg << std::endl;
                return false;
            }
            resident_models[m.id] = std::move(loaded_model);
        } else {
            std::cout << "[Loader] Non-GGUF model descriptor registered: " << m.id << std::endl;
        }
    }

    return true;
}
