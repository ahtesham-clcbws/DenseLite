#pragma once
#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>
#include <mutex>

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
    struct GPUBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        size_t capacity_bytes = 0;
    };

    bool init_resources();
    void cleanup_resources();
    bool ensure_buffer(GPUBuffer& buf, size_t size_bytes);
    void free_buffer(GPUBuffer& buf);

    VulkanDevice* device_ = nullptr;
    mutable std::mutex compute_mutex_;

    bool initialized_ = false;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;

    // Dot product pipeline
    VkShaderModule dot_module_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout dot_dsl_ = VK_NULL_HANDLE;
    VkPipelineLayout dot_layout_ = VK_NULL_HANDLE;
    VkPipeline dot_pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSet dot_desc_set_ = VK_NULL_HANDLE;

    // RMSNorm pipeline
    VkShaderModule rms_module_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout rms_dsl_ = VK_NULL_HANDLE;
    VkPipelineLayout rms_layout_ = VK_NULL_HANDLE;
    VkPipeline rms_pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSet rms_desc_set_ = VK_NULL_HANDLE;

    // Reusable buffers
    GPUBuffer dot_buf_a_;
    GPUBuffer dot_buf_b_;
    GPUBuffer dot_buf_out_;

    GPUBuffer rms_buf_x_;
    GPUBuffer rms_buf_w_;
    GPUBuffer rms_buf_y_;
};

