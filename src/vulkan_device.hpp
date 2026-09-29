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

    // Real GPU Compute Acceleration
    bool has_compute() const { return compute_queue_ != VK_NULL_HANDLE; }
    bool dispatch_vector_dot(const float* a, const float* b, size_t n, float& result);
    bool dispatch_rmsnorm(const float* x, const float* w, float* y, size_t n, float eps);

    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }
    VkDevice device() const { return device_; }
    VkQueue compute_queue() const { return compute_queue_; }
    VkCommandPool command_pool() const { return command_pool_; }
    const VkPhysicalDeviceMemoryProperties& memory_properties() const { return mem_properties_; }
    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;

private:
    bool init_vulkan();
    void cleanup();

    bool available_ = false;
    std::string device_name_;
    uint32_t device_type_ = 0;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    int compute_queue_family_ = -1;
    VkQueue compute_queue_ = VK_NULL_HANDLE;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;

    VkPhysicalDeviceLimits limits_{};
    VkPhysicalDeviceMemoryProperties mem_properties_{};

    size_t total_vram_bytes_ = 0;
    size_t safe_ceiling_bytes_ = 0;
    size_t allocated_bytes_ = 0;
    mutable std::mutex alloc_mutex_;
};
