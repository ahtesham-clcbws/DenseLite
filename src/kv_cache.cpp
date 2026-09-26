#include "kv_cache.hpp"
#include <algorithm>
#include <utility>

KVCache::KVCache() = default;

KVCache::~KVCache() {
    release();
}

KVCache::KVCache(KVCache&& other) noexcept
    : placement_(other.placement_),
      gpu_device_(other.gpu_device_),
      capacity_tokens_(other.capacity_tokens_),
      current_tokens_(other.current_tokens_),
      bytes_per_token_(other.bytes_per_token_),
      allocated_bytes_(other.allocated_bytes_),
      host_buffer_(std::move(other.host_buffer_)) {
    other.gpu_device_ = nullptr;
    other.allocated_bytes_ = 0;
    other.capacity_tokens_ = 0;
    other.current_tokens_ = 0;
}

KVCache& KVCache::operator=(KVCache&& other) noexcept {
    if (this != &other) {
        release();
        placement_ = other.placement_;
        gpu_device_ = other.gpu_device_;
        capacity_tokens_ = other.capacity_tokens_;
        current_tokens_ = other.current_tokens_;
        bytes_per_token_ = other.bytes_per_token_;
        allocated_bytes_ = other.allocated_bytes_;
        host_buffer_ = std::move(other.host_buffer_);

        other.gpu_device_ = nullptr;
        other.allocated_bytes_ = 0;
        other.capacity_tokens_ = 0;
        other.current_tokens_ = 0;
    }
    return *this;
}

size_t KVCache::calculate_bytes_per_token(const ModelConfig& config) {
    size_t head_dim = config.head_dim;
    if (head_dim == 0 && config.num_heads > 0) {
        head_dim = config.embedding_length / config.num_heads;
    }
    if (head_dim == 0) head_dim = 128; // standard fallback

    // 2 tensors (K + V) * num_kv_heads * head_dim * 2 bytes (fp16) * num_layers
    return static_cast<size_t>(config.num_layers) * 2 * config.num_kv_heads * head_dim * sizeof(uint16_t);
}

bool KVCache::allocate(const ModelConfig& config, size_t max_context_budget, VulkanDevice* gpu_device) {
    release();

    size_t model_context_limit = (config.context_length > 0) ? static_cast<size_t>(config.context_length) : 2048;
    size_t effective_tokens = (max_context_budget > 0) ? std::min(model_context_limit, max_context_budget) : model_context_limit;

    bytes_per_token_ = calculate_bytes_per_token(config);
    allocated_bytes_ = effective_tokens * bytes_per_token_;
    capacity_tokens_ = effective_tokens;
    current_tokens_ = 0;

    // GPU-Preferred Admission Gate
    if (gpu_device && gpu_device->is_available() && gpu_device->can_admit(allocated_bytes_)) {
        if (gpu_device->allocate(allocated_bytes_)) {
            placement_ = DevicePlacement::VULKAN_GPU;
            gpu_device_ = gpu_device;
            return true;
        }
    }

    // Fallback: 100% Host Memory (bounded)
    placement_ = DevicePlacement::CPU_RAM;
    gpu_device_ = nullptr;
    try {
        host_buffer_.resize(allocated_bytes_, 0);
    } catch (...) {
        release();
        return false;
    }

    return true;
}

void KVCache::release() {
    if (placement_ == DevicePlacement::VULKAN_GPU && gpu_device_) {
        gpu_device_->release(allocated_bytes_);
        gpu_device_ = nullptr;
    }
    host_buffer_.clear();
    host_buffer_.shrink_to_fit();
    allocated_bytes_ = 0;
    capacity_tokens_ = 0;
    current_tokens_ = 0;
    placement_ = DevicePlacement::CPU_RAM;
}

bool KVCache::can_append(size_t num_tokens) const {
    return (current_tokens_ + num_tokens <= capacity_tokens_);
}

void KVCache::advance(size_t num_tokens) {
    current_tokens_ = std::min(capacity_tokens_, current_tokens_ + num_tokens);
}

void KVCache::reset() {
    current_tokens_ = 0;
}
