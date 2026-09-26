#pragma once
#include "model.hpp"
#include "gguf_parser.hpp"
#include <map>
#include <string>

class ModelLoader {
public:
    static std::map<std::string, std::string> load_env(const std::string& filepath);

    // Safe load: returns true on success, false on error with diagnostic message (no exit(1))
    static bool load_model(const std::string& filepath, DenseModel& model, std::string& error_msg);

    // Safe unload: frees mmapped buffers and clears tensor maps
    static bool unload_model(DenseModel& model);

    // Backward-compatible loader for server startup without exit(1)
    static bool load_resident_models(const std::string& base_dir,
                                     const std::map<std::string, std::string>& env,
                                     std::map<std::string, DenseModel>& resident_models,
                                     std::string& error_msg);

    static bool load_resident_models(const std::string& base_dir,
                                     const std::map<std::string, std::string>& env,
                                     std::map<std::string, DenseModel>& resident_models) {
        std::string err;
        return load_resident_models(base_dir, env, resident_models, err);
    }
};
