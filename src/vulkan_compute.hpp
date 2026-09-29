#pragma once
#include <vulkan/vulkan.h>
#include <cstddef>

class VulkanDevice;

class VulkanCompute {
public:
    explicit VulkanCompute(VulkanDevice* device = nullptr);
    ~VulkanCompute();

    bool is_ready() const;

    // Dispatches parallel vector dot product compute shader to GPU
    bool vector_dot(const float* a, const float* b, size_t n, float& result);

    // Dispatches parallel RMSNorm compute shader to GPU
    bool rmsnorm(const float* x, const float* w, float* y, size_t n, float eps);

private:
    VulkanDevice* device_ = nullptr;
    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
};
