#pragma once
#include "model_registry.hpp"
#include "model_pool.hpp"
#include "model_lease.hpp"
#include "vulkan_device.hpp"
#include "resource_governor.hpp"
#include "ModelLoader.hpp"
#include <string>
#include <map>
#include <memory>
#include <mutex>

class ModelManager {
public:
    ModelManager();
    explicit ModelManager(std::unique_ptr<VulkanDevice> vk_dev);
    ~ModelManager() = default;

    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

    // Load definitions and configure initial residency from environment
    void initialize_from_env(const std::string& base_dir, const std::map<std::string, std::string>& env);

    // RAII Lease Acquisition (by Role or direct Model ID)
    ModelLease acquire(ModelRole role);
    ModelLease acquire(const std::string& model_id);

    // State & Placement queries
    ModelState state(const std::string& id) const;
    DevicePlacement placement(const std::string& id) const;

    // Explicit load / unload operations
    bool load(const std::string& id, std::string& error_msg);
    bool unload(const std::string& id, std::string& error_msg);

    // Eviction cascade: unloads idle models with active_users == 0 under memory pressure or idle timeout
    void enforce_budget(std::chrono::seconds idle_timeout = std::chrono::seconds(180));

    // Accessors
    ModelRegistry& registry() { return registry_; }
    const ModelRegistry& registry() const { return registry_; }
    ModelPool& pool() { return pool_; }
    const ModelPool& pool() const { return pool_; }
    VulkanDevice& vulkan_device() { return *vulkan_device_; }
    const VulkanDevice& vulkan_device() const { return *vulkan_device_; }
    ResourceGovernor& governor() { return governor_; }

private:
    std::unique_ptr<VulkanDevice> vulkan_device_;
    ResourceGovernor governor_;
    ModelRegistry registry_;
    ModelPool pool_;
    mutable std::mutex manager_mutex_;
};
