#include "ModelLoader.hpp"
#include "path_service.hpp"
#include "model_registry_db.hpp"
#include "database_paths.hpp"
#include "model_discovery.hpp"
#include "settings_manager.hpp"
#include "resource_governor.hpp"
#include "routing/modernbert_router.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>

static int get_role_priority(const std::string& role) {
    if (role == "router") return 1;
    if (role == "embedding") return 2;
    if (role == "general") return 3;
    if (role == "compressor") return 4;
    if (role == "coder") return 5;
    if (role == "audio_stt") return 6;
    if (role == "image_gen" || role == "vision_sd") return 7;
    return 99;
}

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
    (void)env;
    if (!base_dir.empty()) PathService::instance().set_base_dir(base_dir);
    std::string db_path = DatabasePaths::settings_db(base_dir.empty() ? "." : base_dir);
    ModelDiscovery::auto_discover_and_register(base_dir, db_path);

    std::string mbert_dir = PathService::instance().get_models_dir() + "/modernbert";
    ModernBERTRouter::instance().initialize(mbert_dir);

    auto db_roles = ModelRegistryDB::get_all_role_bindings(db_path);
    if (db_roles.empty()) return false;

    std::sort(db_roles.begin(), db_roles.end(), [](const auto& a, const auto& b) {
        return get_role_priority(a.role) < get_role_priority(b.role);
    });

    auto res_cfg = SettingsManager::instance().get_resource_config();
    VulkanDevice gpu_device;
    ResourceGovernor gov(&gpu_device);
    size_t total_ram = ResourceGovernor::get_host_total_ram_bytes();
    size_t ram_budget = static_cast<size_t>(total_ram * res_cfg.ram_budget_percent);
    size_t current_rss = ResourceGovernor::get_process_rss_bytes();
    size_t available_ram = (ram_budget > current_rss) ? (ram_budget - current_rss) : 0;
    float headroom_mult = res_cfg.headroom_safety_multiplier;

    std::cout << "[TieredGovernor] Strict Hierarchy: Router -> Embedding -> Main -> Compressor -> Coding -> Audio -> Vision" << std::endl;
    std::cout << "[TieredGovernor] Host RAM Budget (50%): " << (ram_budget / (1024 * 1024)) << " MB | Headroom Buffer: 10%" << std::endl;

    std::map<std::string, std::string> path_to_loaded_role;

    for (const auto& binding : db_roles) {
        LocalModelRecord rec;
        if (!ModelRegistryDB::get_model(db_path, binding.model_id, rec) || !rec.is_verified) continue;
        if (binding.role == "router") continue;

        int priority = get_role_priority(binding.role);
        std::string path = ModelDiscovery::resolve_model_path(rec.file_path, base_dir);
        size_t file_bytes = std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0;
        size_t required_bytes = static_cast<size_t>(file_bytes * headroom_mult);

        if (priority >= 5 && (binding.role == "audio_stt" || binding.role == "image_gen" || rec.architecture == "whisper" || rec.architecture == "diffusion")) {
            std::cout << "[TieredGovernor] Non-GGUF / Multimodal role '" << binding.role << "' mapped as On-Demand." << std::endl;
            continue;
        }

        if (priority >= 5 && available_ram < required_bytes) {
            std::cout << "[TieredGovernor] Memory constrained: Role '" << binding.role << "' held as On-Demand (needs "
                      << (required_bytes / (1024 * 1024)) << " MB, available: " << (available_ram / (1024 * 1024)) << " MB)." << std::endl;
            continue;
        }

        if (path_to_loaded_role.count(path)) {
            std::string src_role = path_to_loaded_role[path];
            resident_models[binding.role] = resident_models[src_role].create_shared_reference();
            std::cout << "[Loader] Multi-role deduplication: '" << binding.role << "' shares memory with '" << src_role << "'" << std::endl;
        } else {
            std::cout << "[Loader] Loading model for role '" << binding.role << "' from " << path << "..." << std::endl;
            DenseModel m;
            if (!load_model(path, m, error_msg)) {
                std::cerr << "[Loader Error] " << error_msg << std::endl;
                continue;
            }
            m.config.model_id = rec.model_id;
            
            // Hardware Governance & Strict Memory Rules (DenseLite v4.0+)
            size_t overhead = (file_bytes < 3ULL * 1024 * 1024 * 1024) ? (200ULL * 1024 * 1024) : (300ULL * 1024 * 1024);
            if (gov.can_admit_gpu_vram(file_bytes, overhead)) {
                m.execution_context = DeviceContext::GPU;
                std::cout << "[ResourceGovernor] Free VRAM >= " << (file_bytes + overhead) / (1024 * 1024) 
                          << "MB. Assigned to GPU." << std::endl;
            } else {
                m.execution_context = DeviceContext::CPU;
                std::cout << "[ResourceGovernor] Insufficient VRAM. Silently falling back to System RAM (CPU)." << std::endl;
            }

            resident_models[binding.role] = std::move(m);
            path_to_loaded_role[path] = binding.role;
            if (available_ram > file_bytes) available_ram -= file_bytes; else available_ram = 0;
        }

        if (resident_models.count(binding.role)) {
            resident_models[binding.model_id] = resident_models[binding.role].create_shared_reference();
        }
        if (binding.role == "general") {
            resident_models["llama_main"] = resident_models["general"].create_shared_reference();
            resident_models["qwen_main"] = resident_models["general"].create_shared_reference();
            resident_models["qwen25_main"] = resident_models["general"].create_shared_reference();
        } else if (binding.role == "coder") {
            resident_models["deepseek_coder"] = resident_models["coder"].create_shared_reference();
            resident_models["qwen_coder"] = resident_models["coder"].create_shared_reference();
            resident_models["qwen25_coder"] = resident_models["coder"].create_shared_reference();
        } else if (binding.role == "compressor") {
            resident_models["smollm2"] = resident_models["compressor"].create_shared_reference();
        } else if (binding.role == "embedding") {
            resident_models["nomic"] = resident_models["embedding"].create_shared_reference();
            resident_models["nomic_embed"] = resident_models["embedding"].create_shared_reference();
        }
    }
    return !resident_models.empty();
}
