#include "ModelLoader.hpp"
#include "path_service.hpp"
#include "model_registry_db.hpp"
#include "database_paths.hpp"
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

bool ModelLoader::load_resident_models(const std::map<std::string, std::string>& env,
                                      std::map<std::string, DenseModel>& resident_models,
                                      std::string& error_msg) {
    return load_resident_models("", env, resident_models, error_msg);
}

bool ModelLoader::load_resident_models(const std::string& base_dir,
                                      const std::map<std::string, std::string>& env,
                                      std::map<std::string, DenseModel>& resident_models,
                                      std::string& error_msg) {
    if (!base_dir.empty()) {
        PathService::instance().set_base_dir(base_dir);
    }
    std::string models_dir = PathService::instance().get_models_dir();
    std::string db_path = DatabasePaths::settings_db(base_dir.empty() ? "." : base_dir);

    // 1. Check if database has active model role bindings
    auto db_roles = ModelRegistryDB::get_all_role_bindings(db_path);
    if (!db_roles.empty()) {
        std::cout << "[Loader] Loading resident models from database registry (" << db_roles.size() << " roles mapped)..." << std::endl;
        std::map<std::string, std::string> path_to_loaded_role;

        for (const auto& binding : db_roles) {
            LocalModelRecord rec;
            if (!ModelRegistryDB::get_model(db_path, binding.model_id, rec) || !rec.is_verified) {
                continue;
            }
            std::string path = PathService::expand_user(rec.file_path);
            if (std::filesystem::path(path).is_relative()) {
                path = models_dir + "/" + path;
            }

            if (path_to_loaded_role.count(path)) {
                std::string src_role = path_to_loaded_role[path];
                resident_models[binding.role] = resident_models[src_role].create_shared_reference();
                std::cout << "[Loader] Multi-role deduplication: role '" << binding.role
                          << "' shares memory with '" << src_role << "' (0 MB duplicate RAM)" << std::endl;
            } else {
                std::cout << "[Loader] Loading model for role '" << binding.role << "' from " << path << "..." << std::endl;
                DenseModel m;
                if (!load_model(path, m, error_msg)) {
                    std::cerr << "[Loader Error] " << error_msg << std::endl;
                    return false;
                }
                resident_models[binding.role] = std::move(m);
                path_to_loaded_role[path] = binding.role;
            }

            // Sync legacy aliases for internal subsystem lookups
            if (binding.role == "general" && !resident_models.count("qwen_main")) {
                resident_models["qwen_main"] = resident_models["general"].create_shared_reference();
            } else if (binding.role == "qwen_main" && !resident_models.count("general")) {
                resident_models["general"] = resident_models["qwen_main"].create_shared_reference();
            }
            if (binding.role == "coder" && !resident_models.count("qwen_coder")) {
                resident_models["qwen_coder"] = resident_models["coder"].create_shared_reference();
            } else if (binding.role == "qwen_coder" && !resident_models.count("coder")) {
                resident_models["coder"] = resident_models["qwen_coder"].create_shared_reference();
            }
            if (binding.role == "compressor" && !resident_models.count("smollm2")) {
                resident_models["smollm2"] = resident_models["compressor"].create_shared_reference();
            } else if (binding.role == "smollm2" && !resident_models.count("compressor")) {
                resident_models["compressor"] = resident_models["smollm2"].create_shared_reference();
            }
            if (binding.role == "embedding" && !resident_models.count("nomic")) {
                resident_models["nomic"] = resident_models["embedding"].create_shared_reference();
            } else if (binding.role == "nomic" && !resident_models.count("embedding")) {
                resident_models["embedding"] = resident_models["nomic"].create_shared_reference();
            }
        }
        return true;
    }

    // 2. Fall back to .env model definitions
    struct ModelDef {
        std::string id;
        std::string path;
        bool is_gguf;
    };

    std::vector<ModelDef> models_to_load;
    auto add_model = [&](const std::string& id, const std::string& env_key, bool is_gguf) {
        if (env.count(env_key)) {
            std::string full_path = models_dir + "/" + env.at(env_key);
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
