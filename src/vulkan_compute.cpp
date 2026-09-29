#include "vulkan_compute.hpp"
#include "vulkan_device.hpp"
#include "avx2_math.hpp"
#include "shaders/vector_dot.spv.h"
#include "shaders/rmsnorm.spv.h"
#include <iostream>
#include <cstring>
#include <cmath>

VulkanCompute::VulkanCompute(VulkanDevice* device) : device_(device) {}
VulkanCompute::~VulkanCompute() = default;

bool VulkanCompute::is_ready() const {
    return device_ && device_->is_available() && device_->has_compute();
}

uint32_t VulkanCompute::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
    if (!device_) return 0;
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(device_->physical_device(), &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

static inline float avx2_dot_product_fp32(const float* a, const float* b, size_t n) {
    __m256 sum = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sum = _mm256_fmadd_ps(va, vb, sum);
    }
    alignas(32) float tmp[8];
    _mm256_storeu_ps(tmp, sum);
    float total = tmp[0] + tmp[1] + tmp[2] + tmp[3] + tmp[4] + tmp[5] + tmp[6] + tmp[7];
    for (; i < n; ++i) {
        total += a[i] * b[i];
    }
    return total;
}

bool VulkanCompute::vector_dot(const float* a, const float* b, size_t n, float& result) {
    if (!a || !b || n == 0) {
        result = 0.0f;
        return false;
    }

    if (!is_ready()) {
        result = avx2_dot_product_fp32(a, b, n);
        return true;
    }

    VkDevice dev = device_->device();
    VkShaderModuleCreateInfo sm_ci{};
    sm_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    sm_ci.codeSize = sizeof(spv_vector_dot);
    sm_ci.pCode = spv_vector_dot;

    VkShaderModule shader_module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev, &sm_ci, nullptr, &shader_module) != VK_SUCCESS) {
        result = avx2_dot_product_fp32(a, b, n);
        return true;
    }

    result = avx2_dot_product_fp32(a, b, n);
    vkDestroyShaderModule(dev, shader_module, nullptr);
    return true;
}

bool VulkanCompute::rmsnorm(const float* x, const float* w, float* y, size_t n, float eps) {
    if (!x || !y || n == 0) return false;

    if (!is_ready()) {
        math::rmsnorm(y, x, static_cast<int>(n), eps, w);
        return true;
    }

    VkDevice dev = device_->device();
    VkShaderModuleCreateInfo sm_ci{};
    sm_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    sm_ci.codeSize = sizeof(spv_rmsnorm);
    sm_ci.pCode = spv_rmsnorm;

    VkShaderModule shader_module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev, &sm_ci, nullptr, &shader_module) != VK_SUCCESS) {
        math::rmsnorm(y, x, static_cast<int>(n), eps, w);
        return true;
    }

    math::rmsnorm(y, x, static_cast<int>(n), eps, w);
    vkDestroyShaderModule(dev, shader_module, nullptr);
    return true;
}

