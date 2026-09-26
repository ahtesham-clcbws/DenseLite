#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <cstddef>
#include <mutex>

struct VulkanMemoryInfo {
    size_t total_vram_bytes = 0;
    size_t safe_ceiling_bytes = 0; // 85% of dedicated VRAM
    size_t allocated_bytes = 0;
    size_t available_budget_bytes = 0;
};

class VulkanDevice {
public:
    VulkanDevice();
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    bool is_available() const { return available_; }
    const std::string& device_name() const { return device_name_; }
    uint32_t device_type() const { return device_type_; }
    const VkPhysicalDeviceLimits& limits() const { return limits_; }

    VulkanMemoryInfo memory_info() const;
    size_t safe_ceiling_bytes() const;
    size_t allocated_bytes() const;
    size_t available_safe_bytes() const;

    // 85% safety gate admission check
    bool can_admit(size_t bytes) const;
    bool allocate(size_t bytes);
    void release(size_t bytes);

    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }
    VkDevice device() const { return device_; }

private:
    bool init_vulkan();
    void cleanup();

    bool available_ = false;
    std::string device_name_;
    uint32_t device_type_ = 0;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDeviceLimits limits_{};
    VkPhysicalDeviceMemoryProperties mem_properties_{};

    size_t total_vram_bytes_ = 0;
    size_t safe_ceiling_bytes_ = 0;
    size_t allocated_bytes_ = 0;
    mutable std::mutex alloc_mutex_;
};
