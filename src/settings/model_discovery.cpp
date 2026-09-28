#include "model_discovery.hpp"
#include "path_service.hpp"
#include "model_registry_db.hpp"
#include "model_inspector.hpp"
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

std::string ModelDiscovery::resolve_model_path(const std::string& raw_path, const std::string& base_dir) {
    if (raw_path.empty()) return "";

    std::string expanded = PathService::expand_user(raw_path);
    if (fs::exists(expanded)) return expanded;

    std::string default_user = PathService::expand_user("~/.denselite/models");
    std::string user_dir = PathService::instance().get_models_dir();
    std::string b_dir = base_dir.empty() ? PathService::instance().get_base_dir() : base_dir;

    std::vector<std::string> search_bases = { default_user, user_dir, b_dir + "/models" };
    for (const auto& d : search_bases) {
        if (d.empty()) continue;
        fs::path p = fs::path(d) / raw_path;
        if (fs::exists(p)) return p.string();
    }

    std::string fname = fs::path(raw_path).filename().string();
    for (const auto& d : search_bases) {
        if (d.empty()) continue;
        fs::path p = fs::path(d) / fname;
        if (fs::exists(p)) return p.string();
    }

    return expanded;
}

static std::string derive_model_id(const std::string& filename) {
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("qwen2.5-coder") != std::string::npos) return "qwen25_coder";
    if (lower.find("qwen2.5") != std::string::npos) return "qwen25_main";
    if (lower.find("smollm2") != std::string::npos || lower.find("smol") != std::string::npos) return "smollm2";
    if (lower.find("nomic-embed") != std::string::npos || lower.find("nomic") != std::string::npos) return "nomic_embed";
    if (lower.find("whisper") != std::string::npos || lower.find("ggml-base") != std::string::npos) return "whisper_base";
    if (lower.find("stable-diffusion") != std::string::npos || lower.find("sd-v1") != std::string::npos ||
        lower.find("sd15") != std::string::npos || lower.find("v1-5") != std::string::npos) return "sd15";

    std::string stem = fs::path(filename).stem().string();
    std::string clean;
    for (char c : stem) {
        if (std::isalnum(c) || c == '_' || c == '-') clean += (char)::tolower(c);
        else clean += '_';
    }
    return clean.empty() ? "custom_model" : clean;
}

static void bind_roles_for_id(const std::string& db_path, const std::string& model_id) {
    auto bindings = ModelRegistryDB::get_all_role_bindings(db_path, false);
    auto has_role = [&](const std::string& r) {
        for (const auto& b : bindings) if (b.role == r && !b.model_id.empty()) return true;
        return false;
    };

    if (model_id == "qwen25_main" && !has_role("general")) {
        ModelRegistryDB::bind_role(db_path, "general", model_id, true);
    } else if (model_id == "qwen25_coder" && !has_role("coder")) {
        ModelRegistryDB::bind_role(db_path, "coder", model_id, true);
    } else if (model_id == "smollm2" && !has_role("compressor")) {
        ModelRegistryDB::bind_role(db_path, "compressor", model_id, true);
    } else if (model_id == "nomic_embed" && !has_role("embedding")) {
        ModelRegistryDB::bind_role(db_path, "embedding", model_id, true);
    } else if (model_id == "whisper_base" && !has_role("audio_stt")) {
        ModelRegistryDB::bind_role(db_path, "audio_stt", model_id, true);
    } else if (model_id == "sd15" && !has_role("image_gen")) {
        ModelRegistryDB::bind_role(db_path, "image_gen", model_id, true);
    }
}

int ModelDiscovery::auto_discover_and_register(const std::string& base_dir, const std::string& db_path) {
    int registered_count = 0;
    std::string default_user = PathService::expand_user("~/.denselite/models");
    std::string user_dir = PathService::instance().get_models_dir();
    std::string b_dir = base_dir.empty() ? PathService::instance().get_base_dir() : base_dir;
    std::string internal_dir = b_dir + "/models";

    std::vector<std::string> search_dirs;
    if (fs::exists(default_user)) search_dirs.push_back(default_user);
    if (fs::exists(user_dir) && user_dir != default_user) search_dirs.push_back(user_dir);
    if (fs::exists(internal_dir) && internal_dir != default_user && internal_dir != user_dir) search_dirs.push_back(internal_dir);

    auto existing_models = ModelRegistryDB::get_all_models(db_path);
    auto check_and_update_existing = [&](const std::string& path, const std::string& id) -> bool {
        for (auto& m : existing_models) {
            if (m.model_id == id || fs::path(m.file_path).filename() == fs::path(path).filename()) {
                if (m.file_path != path && !fs::exists(m.file_path) && fs::exists(path)) {
                    m.file_path = path;
                    ModelRegistryDB::register_model(db_path, m);
                    std::cout << "[Discovery] Relocated: " << m.model_id << " -> " << path << std::endl;
                }
                return true;
            }
        }
        return false;
    };

    for (const auto& dir : search_dirs) {
        try {
            for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                std::string path = entry.path().string();
                std::string filename = entry.path().filename().string();
                std::string ext = entry.path().extension().string();

                if (ext == ".gguf") {
                    std::string id = derive_model_id(filename);
                    if (check_and_update_existing(path, id)) continue;

                    auto insp = ModelInspector::inspect(path);
                    if (insp.is_valid && insp.is_supported_edge_size()) {
                        LocalModelRecord rec{id, path, insp.architecture, insp.param_count,
                                             insp.param_size_str, insp.quant_type, insp.context_length, true, 0};
                        if (ModelRegistryDB::register_model(db_path, rec)) {
                            bind_roles_for_id(db_path, id);
                            registered_count++;
                            existing_models.push_back(rec);
                            std::cout << "[Discovery] Registered GGUF: " << id << " (" << path << ")" << std::endl;
                        }
                    }
                } else if (filename == "ggml-base.en.bin" || filename.find("whisper") != std::string::npos) {
                    std::string id = "whisper_base";
                    if (check_and_update_existing(path, id)) continue;
                    LocalModelRecord rec{id, path, "whisper", 72593920, "72M", "FP32", 448, true, 0};
                    if (ModelRegistryDB::register_model(db_path, rec)) {
                        bind_roles_for_id(db_path, id);
                        registered_count++;
                        existing_models.push_back(rec);
                        std::cout << "[Discovery] Registered Audio Model: " << id << " (" << path << ")" << std::endl;
                    }
                } else if (ext == ".safetensors" || filename.find("v1-5-pruned") != std::string::npos) {
                    std::string id = "sd15";
                    if (check_and_update_existing(path, id)) continue;
                    LocalModelRecord rec{id, path, "diffusion", 860000000, "860M", "FP16", 77, true, 0};
                    if (ModelRegistryDB::register_model(db_path, rec)) {
                        bind_roles_for_id(db_path, id);
                        registered_count++;
                        existing_models.push_back(rec);
                        std::cout << "[Discovery] Registered Diffusion Model: " << id << " (" << path << ")" << std::endl;
                    }
                }
            }
        } catch (...) {}
    }
    return registered_count;
}
