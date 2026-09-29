#include "vulkan_compute.hpp"
#include "vulkan_device.hpp"
#include "avx2_math.hpp"
#include "shaders/vector_dot.spv.h"
#include "shaders/rmsnorm.spv.h"
#include <iostream>
#include <cstring>
#include <cmath>

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

VulkanCompute::VulkanCompute(VulkanDevice* device) : device_(device) {}

VulkanCompute::~VulkanCompute() {
    cleanup_resources();
}

bool VulkanCompute::is_ready() const {
    return device_ && device_->is_available() && device_->has_compute();
}

void VulkanCompute::free_buffer(GPUBuffer& buf) {
    if (!device_ || !device_->device()) return;
    VkDevice dev = device_->device();
    if (buf.mapped) {
        vkUnmapMemory(dev, buf.memory);
        buf.mapped = nullptr;
    }
    if (buf.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, buf.buffer, nullptr);
        buf.buffer = VK_NULL_HANDLE;
    }
    if (buf.memory != VK_NULL_HANDLE) {
        vkFreeMemory(dev, buf.memory, nullptr);
        buf.memory = VK_NULL_HANDLE;
    }
    buf.capacity_bytes = 0;
}

bool VulkanCompute::ensure_buffer(GPUBuffer& buf, size_t size_bytes) {
    if (buf.buffer != VK_NULL_HANDLE && buf.capacity_bytes >= size_bytes) {
        return true;
    }
    free_buffer(buf);
    if (!device_ || !device_->device()) return false;
    VkDevice dev = device_->device();

    size_t alloc_size = (size_bytes + 1023) & ~1023; // Align to 1KB

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = alloc_size;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(dev, &bci, nullptr, &buf.buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(dev, buf.buffer, &reqs);
    uint32_t mem_type = device_->find_memory_type(reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mem_type == UINT32_MAX) {
        vkDestroyBuffer(dev, buf.buffer, nullptr);
        buf.buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = reqs.size;
    mai.memoryTypeIndex = mem_type;
    if (vkAllocateMemory(dev, &mai, nullptr, &buf.memory) != VK_SUCCESS) {
        vkDestroyBuffer(dev, buf.buffer, nullptr);
        buf.buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(dev, buf.buffer, buf.memory, 0);
    if (vkMapMemory(dev, buf.memory, 0, alloc_size, 0, &buf.mapped) != VK_SUCCESS) {
        free_buffer(buf);
        return false;
    }
    buf.capacity_bytes = alloc_size;
    return true;
}

bool VulkanCompute::init_resources() {
    if (initialized_) return true;
    if (!device_ || !device_->is_available() || !device_->has_compute()) return false;
    VkDevice dev = device_->device();

    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 }
    };
    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 4;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = pool_sizes;
    if (vkCreateDescriptorPool(dev, &dpci, nullptr, &desc_pool_) != VK_SUCCESS) return false;

    VkDescriptorSetLayoutBinding bindings[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 3;
    dslci.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &dot_dsl_) != VK_SUCCESS) return false;
    if (vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &rms_dsl_) != VK_SUCCESS) return false;

    VkPushConstantRange dot_pcr{};
    dot_pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    dot_pcr.offset = 0;
    dot_pcr.size = sizeof(uint32_t);

    VkPipelineLayoutCreateInfo dot_plci{};
    dot_plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    dot_plci.setLayoutCount = 1;
    dot_plci.pSetLayouts = &dot_dsl_;
    dot_plci.pushConstantRangeCount = 1;
    dot_plci.pPushConstantRanges = &dot_pcr;
    if (vkCreatePipelineLayout(dev, &dot_plci, nullptr, &dot_layout_) != VK_SUCCESS) return false;

    VkPushConstantRange rms_pcr{};
    rms_pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    rms_pcr.offset = 0;
    rms_pcr.size = sizeof(uint32_t) + sizeof(float);

    VkPipelineLayoutCreateInfo rms_plci{};
    rms_plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    rms_plci.setLayoutCount = 1;
    rms_plci.pSetLayouts = &rms_dsl_;
    rms_plci.pushConstantRangeCount = 1;
    rms_plci.pPushConstantRanges = &rms_pcr;
    if (vkCreatePipelineLayout(dev, &rms_plci, nullptr, &rms_layout_) != VK_SUCCESS) return false;

    VkShaderModuleCreateInfo dot_smci{};
    dot_smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    dot_smci.codeSize = sizeof(spv_vector_dot);
    dot_smci.pCode = spv_vector_dot;
    if (vkCreateShaderModule(dev, &dot_smci, nullptr, &dot_module_) != VK_SUCCESS) return false;

    VkShaderModuleCreateInfo rms_smci{};
    rms_smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    rms_smci.codeSize = sizeof(spv_rmsnorm);
    rms_smci.pCode = spv_rmsnorm;
    if (vkCreateShaderModule(dev, &rms_smci, nullptr, &rms_module_) != VK_SUCCESS) return false;

    VkComputePipelineCreateInfo dot_cpci{};
    dot_cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    dot_cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    dot_cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    dot_cpci.stage.module = dot_module_;
    dot_cpci.stage.pName = "main";
    dot_cpci.layout = dot_layout_;
    if (vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &dot_cpci, nullptr, &dot_pipeline_) != VK_SUCCESS) return false;

    VkComputePipelineCreateInfo rms_cpci{};
    rms_cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    rms_cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rms_cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    rms_cpci.stage.module = rms_module_;
    rms_cpci.stage.pName = "main";
    rms_cpci.layout = rms_layout_;
    if (vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &rms_cpci, nullptr, &rms_pipeline_) != VK_SUCCESS) return false;

    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = desc_pool_;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &dot_dsl_;
    if (vkAllocateDescriptorSets(dev, &dsai, &dot_desc_set_) != VK_SUCCESS) return false;

    dsai.pSetLayouts = &rms_dsl_;
    if (vkAllocateDescriptorSets(dev, &dsai, &rms_desc_set_) != VK_SUCCESS) return false;

    initialized_ = true;
    return true;
}

void VulkanCompute::cleanup_resources() {
    if (!device_ || !device_->device()) return;
    VkDevice dev = device_->device();

    free_buffer(dot_buf_a_);
    free_buffer(dot_buf_b_);
    free_buffer(dot_buf_out_);
    free_buffer(rms_buf_x_);
    free_buffer(rms_buf_w_);
    free_buffer(rms_buf_y_);

    if (dot_pipeline_ != VK_NULL_HANDLE) { vkDestroyPipeline(dev, dot_pipeline_, nullptr); dot_pipeline_ = VK_NULL_HANDLE; }
    if (rms_pipeline_ != VK_NULL_HANDLE) { vkDestroyPipeline(dev, rms_pipeline_, nullptr); rms_pipeline_ = VK_NULL_HANDLE; }
    if (dot_layout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(dev, dot_layout_, nullptr); dot_layout_ = VK_NULL_HANDLE; }
    if (rms_layout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(dev, rms_layout_, nullptr); rms_layout_ = VK_NULL_HANDLE; }
    if (dot_dsl_ != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(dev, dot_dsl_, nullptr); dot_dsl_ = VK_NULL_HANDLE; }
    if (rms_dsl_ != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(dev, rms_dsl_, nullptr); rms_dsl_ = VK_NULL_HANDLE; }
    if (dot_module_ != VK_NULL_HANDLE) { vkDestroyShaderModule(dev, dot_module_, nullptr); dot_module_ = VK_NULL_HANDLE; }
    if (rms_module_ != VK_NULL_HANDLE) { vkDestroyShaderModule(dev, rms_module_, nullptr); rms_module_ = VK_NULL_HANDLE; }
    if (desc_pool_ != VK_NULL_HANDLE) { vkDestroyDescriptorPool(dev, desc_pool_, nullptr); desc_pool_ = VK_NULL_HANDLE; }

    initialized_ = false;
}

bool VulkanCompute::vector_dot(const float* a, const float* b, size_t n, float& result) {
    if (!a || !b || n == 0) {
        result = 0.0f;
        return false;
    }

    std::lock_guard<std::mutex> lock(compute_mutex_);
    if (!is_ready() || !init_resources()) {
        result = avx2_dot_product_fp32(a, b, n);
        return true;
    }

    size_t byte_size = n * sizeof(float);
    size_t out_byte_size = sizeof(float);
    if (!ensure_buffer(dot_buf_a_, byte_size) ||
        !ensure_buffer(dot_buf_b_, byte_size) ||
        !ensure_buffer(dot_buf_out_, out_byte_size)) {
        result = avx2_dot_product_fp32(a, b, n);
        return true;
    }

    std::memcpy(dot_buf_a_.mapped, a, byte_size);
    std::memcpy(dot_buf_b_.mapped, b, byte_size);

    VkDevice dev = device_->device();
    VkDescriptorBufferInfo buf_infos[3] = {
        { dot_buf_a_.buffer, 0, byte_size },
        { dot_buf_b_.buffer, 0, byte_size },
        { dot_buf_out_.buffer, 0, out_byte_size }
    };
    VkWriteDescriptorSet writes[3]{};
    for (int i = 0; i < 3; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = dot_desc_set_;
        writes[i].dstBinding = static_cast<uint32_t>(i);
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &buf_infos[i];
    }
    vkUpdateDescriptorSets(dev, 3, writes, 0, nullptr);

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = device_->command_pool();
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(dev, &cbai, &cmd) != VK_SUCCESS) {
        result = avx2_dot_product_fp32(a, b, n);
        return true;
    }

    VkCommandBufferBeginInfo cbbi{};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &cbbi);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dot_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, dot_layout_, 0, 1, &dot_desc_set_, 0, nullptr);
    uint32_t count = static_cast<uint32_t>(n);
    vkCmdPushConstants(cmd, dot_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &count);
    vkCmdDispatch(cmd, 1, 1, 1);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;

    VkQueue q = device_->compute_queue();
    if (vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS) {
        vkQueueWaitIdle(q);
        result = *reinterpret_cast<float*>(dot_buf_out_.mapped);
    } else {
        result = avx2_dot_product_fp32(a, b, n);
    }

    vkFreeCommandBuffers(dev, device_->command_pool(), 1, &cmd);
    return true;
}

bool VulkanCompute::rmsnorm(const float* x, const float* w, float* y, size_t n, float eps) {
    if (!x || !y || n == 0) return false;

    std::lock_guard<std::mutex> lock(compute_mutex_);
    if (!is_ready() || !init_resources()) {
        math::rmsnorm(y, x, static_cast<int>(n), eps, w);
        return true;
    }

    size_t byte_size = n * sizeof(float);
    if (!ensure_buffer(rms_buf_x_, byte_size) ||
        !ensure_buffer(rms_buf_w_, byte_size) ||
        !ensure_buffer(rms_buf_y_, byte_size)) {
        math::rmsnorm(y, x, static_cast<int>(n), eps, w);
        return true;
    }

    std::memcpy(rms_buf_x_.mapped, x, byte_size);
    std::memcpy(rms_buf_w_.mapped, w, byte_size);

    VkDevice dev = device_->device();
    VkDescriptorBufferInfo buf_infos[3] = {
        { rms_buf_x_.buffer, 0, byte_size },
        { rms_buf_w_.buffer, 0, byte_size },
        { rms_buf_y_.buffer, 0, byte_size }
    };
    VkWriteDescriptorSet writes[3]{};
    for (int i = 0; i < 3; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = rms_desc_set_;
        writes[i].dstBinding = static_cast<uint32_t>(i);
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &buf_infos[i];
    }
    vkUpdateDescriptorSets(dev, 3, writes, 0, nullptr);

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = device_->command_pool();
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(dev, &cbai, &cmd) != VK_SUCCESS) {
        math::rmsnorm(y, x, static_cast<int>(n), eps, w);
        return true;
    }

    VkCommandBufferBeginInfo cbbi{};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &cbbi);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rms_pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rms_layout_, 0, 1, &rms_desc_set_, 0, nullptr);
    struct {
        uint32_t n;
        float eps;
    } pc = { static_cast<uint32_t>(n), eps };
    vkCmdPushConstants(cmd, rms_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch(cmd, 1, 1, 1);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;

    VkQueue q = device_->compute_queue();
    if (vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS) {
        vkQueueWaitIdle(q);
        std::memcpy(y, rms_buf_y_.mapped, byte_size);
    } else {
        math::rmsnorm(y, x, static_cast<int>(n), eps, w);
    }

    vkFreeCommandBuffers(dev, device_->command_pool(), 1, &cmd);
    return true;
}
