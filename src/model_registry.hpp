#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstddef>

enum class ModelRole {
    ROUTER,           // Needle
    EMBEDDING,        // Nomic
    FORMATTER,        // SmolLM2
    GENERAL_REASONER, // Qwen Main
    CODER,            // Qwen Coder
    SPEECH_TO_TEXT,   // Whisper
    IMAGE_GENERATOR   // SD 1.5
};

enum class DevicePlacement {
    VULKAN_GPU,
    CPU_RAM
};

enum class ModelState {
    COLD,
    LOADING,
    HOT,
    WARM,
    FAILED
};

enum class PlacementPolicy {
    GPU_PREFERRED,
    CPU_RAM_ONLY
};

struct ModelDescriptor {
    std::string id;
    ModelRole role;
    std::string file_path;
    bool is_gguf = true;
    PlacementPolicy placement_policy = PlacementPolicy::GPU_PREFERRED;
    size_t estimated_weight_bytes = 0;
    size_t estimated_scratch_bytes = 0;
};

class ModelRegistry {
public:
    ModelRegistry() = default;

    void register_model(const ModelDescriptor& desc);
    const ModelDescriptor* get(const std::string& id) const;
    const ModelDescriptor* get_by_role(ModelRole role) const;
    std::vector<std::string> get_all_ids() const;

    static std::string role_to_string(ModelRole role);
    static ModelRole string_to_role(const std::string& str);
    static std::string state_to_string(ModelState state);
    static std::string placement_to_string(DevicePlacement placement);

private:
    std::map<std::string, ModelDescriptor> models_by_id_;
    std::map<ModelRole, std::string> primary_model_for_role_;
};
