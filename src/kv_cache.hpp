#pragma once
#include "model.hpp"
#include "model_registry.hpp"
#include "vulkan_device.hpp"
#include <cstddef>
#include <vector>
#include <cstdint>

class KVCache {
public:
    KVCache();
    ~KVCache();

    KVCache(const KVCache&) = delete;
    KVCache& operator=(const KVCache&) = delete;
    KVCache(KVCache&& other) noexcept;
    KVCache& operator=(KVCache&& other) noexcept;

    // Calculate KV bytes per token for a given model config
    static size_t calculate_bytes_per_token(const ModelConfig& config);

    // Allocates bounded KV cache. Checks GPU 85% ceiling; falls back to Host RAM if over budget.
    bool allocate(const ModelConfig& config, size_t max_context_budget, VulkanDevice* gpu_device);
    void release();

    DevicePlacement placement() const { return placement_; }
    size_t capacity_tokens() const { return capacity_tokens_; }
    size_t current_tokens() const { return current_tokens_; }
    size_t allocated_bytes() const { return allocated_bytes_; }
    size_t bytes_per_token() const { return bytes_per_token_; }

    bool can_append(size_t num_tokens) const;
    void advance(size_t num_tokens);
    void reset();

    uint8_t* host_buffer() { return host_buffer_.data(); }
    const uint8_t* host_buffer() const { return host_buffer_.data(); }

private:
    DevicePlacement placement_ = DevicePlacement::CPU_RAM;
    VulkanDevice* gpu_device_ = nullptr;
    size_t capacity_tokens_ = 0;
    size_t current_tokens_ = 0;
    size_t bytes_per_token_ = 0;
    size_t allocated_bytes_ = 0;

    std::vector<uint8_t> host_buffer_;
};
