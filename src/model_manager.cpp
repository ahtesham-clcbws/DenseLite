#include "model_manager.hpp"
#include <filesystem>
#include <iostream>

ModelManager::ModelManager()
    : vulkan_device_(std::make_unique<VulkanDevice>()),
      governor_(vulkan_device_.get()) {
    governor_.enforce_thread_limits();
}

ModelManager::ModelManager(std::unique_ptr<VulkanDevice> vk_dev)
    : vulkan_device_(std::move(vk_dev)),
      governor_(vulkan_device_ ? vulkan_device_.get() : nullptr) {
    governor_.enforce_thread_limits();
}

void ModelManager::initialize_from_env(const std::string& base_dir, const std::map<std::string, std::string>& env) {
    auto register_def = [&](const std::string& id, ModelRole role, const std::vector<std::string>& env_keys, PlacementPolicy policy, bool is_gguf) {
        std::string filename;
        for (const auto& k : env_keys) {
            if (env.count(k)) {
                filename = env.at(k);
                break;
            }
        }
        if (filename.empty()) return;
        std::string full_path = base_dir + "/models/" + filename;

        ModelDescriptor desc;
        desc.id = id;
        desc.role = role;
        desc.file_path = full_path;
        desc.is_gguf = is_gguf;
        desc.placement_policy = policy;

        if (std::filesystem::exists(full_path)) {
            desc.estimated_weight_bytes = std::filesystem::file_size(full_path);
            desc.estimated_scratch_bytes = is_gguf ? std::max<size_t>(64ULL * 1024 * 1024, desc.estimated_weight_bytes / 8) : 0;
        } else {
            desc.estimated_weight_bytes = 0;
            desc.estimated_scratch_bytes = 0;
        }
        registry_.register_model(desc);
    };

    // Primary Zero-Shot Intent Router (ModernBERT)
    ModelDescriptor router_desc;
    router_desc.id = "modernbert";
    router_desc.role = ModelRole::ROUTER;
    router_desc.file_path = base_dir + "/models/modernbert";
    router_desc.is_gguf = false;
    router_desc.placement_policy = PlacementPolicy::CPU_RAM_ONLY;
    std::string onnx_file = router_desc.file_path + "/model.onnx";
    if (std::filesystem::exists(onnx_file)) {
        router_desc.estimated_weight_bytes = std::filesystem::file_size(onnx_file);
    } else {
        router_desc.estimated_weight_bytes = 398ULL * 1024 * 1024;
    }
    router_desc.estimated_scratch_bytes = 64ULL * 1024 * 1024;
    registry_.register_model(router_desc);

    register_def("nomic", ModelRole::EMBEDDING, {"MODEL_NOMIC_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("smollm2", ModelRole::FORMATTER, {"MODEL_SMOLLM2_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("general", ModelRole::GENERAL_REASONER, {"MODEL_MAIN_FILE", "MODEL_LLAMA_FILE", "MODEL_QWEN_MAIN_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("llama_main", ModelRole::GENERAL_REASONER, {"MODEL_MAIN_FILE", "MODEL_LLAMA_FILE", "MODEL_QWEN_MAIN_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("qwen_main", ModelRole::GENERAL_REASONER, {"MODEL_MAIN_FILE", "MODEL_LLAMA_FILE", "MODEL_QWEN_MAIN_FILE"}, PlacementPolicy::GPU_PREFERRED, true);

    register_def("coder", ModelRole::CODER, {"MODEL_CODER_FILE", "MODEL_DEEPSEEK_FILE", "MODEL_QWEN_CODER_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("deepseek_coder", ModelRole::CODER, {"MODEL_CODER_FILE", "MODEL_DEEPSEEK_FILE", "MODEL_QWEN_CODER_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("qwen_coder", ModelRole::CODER, {"MODEL_CODER_FILE", "MODEL_DEEPSEEK_FILE", "MODEL_QWEN_CODER_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("whisper", ModelRole::SPEECH_TO_TEXT, {"MODEL_WHISPER_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("whisper_turbo", ModelRole::SPEECH_TO_TEXT, {"MODEL_WHISPER_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("audio_stt", ModelRole::SPEECH_TO_TEXT, {"MODEL_WHISPER_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("vision", ModelRole::IMAGE_GENERATOR, {"MODEL_VISION_FILE", "MODEL_SD_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("sdxl_lightning", ModelRole::IMAGE_GENERATOR, {"MODEL_VISION_FILE", "MODEL_SD_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
    register_def("stable_diffusion", ModelRole::IMAGE_GENERATOR, {"MODEL_VISION_FILE", "MODEL_SD_FILE"}, PlacementPolicy::GPU_PREFERRED, true);
}

ModelLease ModelManager::acquire(ModelRole role) {
    const ModelDescriptor* desc = registry_.get_by_role(role);
    if (!desc) {
        return ModelLease();
    }
    return acquire(desc->id);
}

ModelLease ModelManager::acquire(const std::string& model_id) {
    ModelLease lease;
    if (pool_.acquire_lease(model_id, lease)) {
        return lease;
    }

    // Model not yet loaded; load on-demand
    std::string err;
    if (!load(model_id, err)) {
        std::cerr << "[ModelManager] Failed to load on-demand model " << model_id << ": " << err << std::endl;
        return ModelLease();
    }

    if (pool_.acquire_lease(model_id, lease)) {
        return lease;
    }
    return ModelLease();
}

ModelState ModelManager::state(const std::string& id) const {
    return pool_.get_state(id);
}

DevicePlacement ModelManager::placement(const std::string& id) const {
    return pool_.get_placement(id);
}

bool ModelManager::load(const std::string& id, std::string& error_msg) {
    std::lock_guard<std::mutex> lock(manager_mutex_);

    // Already resident
    if (pool_.get_state(id) == ModelState::HOT || pool_.get_state(id) == ModelState::WARM) {
        return true;
    }

    const ModelDescriptor* desc = registry_.get(id);
    if (!desc) {
        error_msg = "Model ID not registered in ModelRegistry: " + id;
        return false;
    }

    if (!desc->is_gguf) {
        // Non-GGUF router stub
        DenseModel dummy_model;
        pool_.add_model(id, std::move(dummy_model), DevicePlacement::CPU_RAM, 0);
        return true;
    }

    // GPU-Preferred Admission Gate
    DevicePlacement admitted_placement = DevicePlacement::CPU_RAM;
    const size_t total_gpu_required = desc->estimated_weight_bytes + desc->estimated_scratch_bytes;
    if (vulkan_device_ && vulkan_device_->is_available() && desc->placement_policy == PlacementPolicy::GPU_PREFERRED) {
        if (vulkan_device_->can_admit(total_gpu_required)) {
            admitted_placement = DevicePlacement::VULKAN_GPU;
        } else {
            // Falls back to CPU_RAM when GPU budget is saturated
            admitted_placement = DevicePlacement::CPU_RAM;
        }
    }

    DenseModel model;
    if (!ModelLoader::load_model(desc->file_path, model, error_msg)) {
        return false;
    }

    if (admitted_placement == DevicePlacement::VULKAN_GPU) {
        vulkan_device_->allocate(total_gpu_required);
    }

    pool_.add_model(id, std::move(model), admitted_placement, total_gpu_required);
    return true;
}

bool ModelManager::unload(const std::string& id, std::string& error_msg) {
    std::lock_guard<std::mutex> lock(manager_mutex_);

    if (pool_.is_in_use(id)) {
        error_msg = "Model " + id + " has active leases; unload blocked by eviction guard.";
        return false;
    }

    DevicePlacement plc = pool_.get_placement(id);
    size_t mem_bytes = pool_.get_memory_bytes(id);

    DenseModel model_to_free;
    if (pool_.remove_model(id, model_to_free)) {
        if (plc == DevicePlacement::VULKAN_GPU && vulkan_device_) {
            vulkan_device_->release(mem_bytes);
        }
        ModelLoader::unload_model(model_to_free);
        return true;
    }

    error_msg = "Model " + id + " is not currently loaded.";
    return false;
}

void ModelManager::enforce_budget(std::chrono::seconds idle_timeout) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    auto evictable = pool_.get_evictable_model_ids(idle_timeout);
    for (const auto& id : evictable) {
        // Evict idle leased models under memory pressure
        const ModelDescriptor* desc = registry_.get(id);
        if (desc && (desc->role == ModelRole::CODER || 
                     desc->role == ModelRole::SPEECH_TO_TEXT || 
                     desc->role == ModelRole::IMAGE_GENERATOR)) {
            std::string err;
            unload(id, err);
        }
    }
}
