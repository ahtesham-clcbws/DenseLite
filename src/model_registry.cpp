#include "model_registry.hpp"

void ModelRegistry::register_model(const ModelDescriptor& desc) {
    models_by_id_[desc.id] = desc;
    if (primary_model_for_role_.find(desc.role) == primary_model_for_role_.end()) {
        primary_model_for_role_[desc.role] = desc.id;
    }
}

const ModelDescriptor* ModelRegistry::get(const std::string& id) const {
    auto it = models_by_id_.find(id);
    if (it != models_by_id_.end()) {
        return &it->second;
    }
    return nullptr;
}

const ModelDescriptor* ModelRegistry::get_by_role(ModelRole role) const {
    auto it = primary_model_for_role_.find(role);
    if (it != primary_model_for_role_.end()) {
        return get(it->second);
    }
    return nullptr;
}

std::vector<std::string> ModelRegistry::get_all_ids() const {
    std::vector<std::string> ids;
    ids.reserve(models_by_id_.size());
    for (const auto& pair : models_by_id_) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::string ModelRegistry::role_to_string(ModelRole role) {
    switch (role) {
        case ModelRole::ROUTER: return "ROUTER";
        case ModelRole::EMBEDDING: return "EMBEDDING";
        case ModelRole::FORMATTER: return "FORMATTER";
        case ModelRole::GENERAL_REASONER: return "GENERAL_REASONER";
        case ModelRole::CODER: return "CODER";
        case ModelRole::SPEECH_TO_TEXT: return "SPEECH_TO_TEXT";
        case ModelRole::IMAGE_GENERATOR: return "IMAGE_GENERATOR";
    }
    return "UNKNOWN";
}

ModelRole ModelRegistry::string_to_role(const std::string& str) {
    if (str == "ROUTER") return ModelRole::ROUTER;
    if (str == "EMBEDDING") return ModelRole::EMBEDDING;
    if (str == "FORMATTER") return ModelRole::FORMATTER;
    if (str == "GENERAL_REASONER") return ModelRole::GENERAL_REASONER;
    if (str == "CODER") return ModelRole::CODER;
    if (str == "SPEECH_TO_TEXT") return ModelRole::SPEECH_TO_TEXT;
    if (str == "IMAGE_GENERATOR") return ModelRole::IMAGE_GENERATOR;
    return ModelRole::GENERAL_REASONER;
}

std::string ModelRegistry::state_to_string(ModelState state) {
    switch (state) {
        case ModelState::COLD: return "COLD";
        case ModelState::LOADING: return "LOADING";
        case ModelState::HOT: return "HOT";
        case ModelState::WARM: return "WARM";
        case ModelState::FAILED: return "FAILED";
    }
    return "UNKNOWN";
}

std::string ModelRegistry::placement_to_string(DevicePlacement placement) {
    switch (placement) {
        case DevicePlacement::VULKAN_GPU: return "VULKAN_GPU";
        case DevicePlacement::CPU_RAM: return "CPU_RAM";
    }
    return "UNKNOWN";
}
