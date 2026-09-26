#pragma once
#include "vulkan_device.hpp"
#include <cstddef>
#include <string>
#include <atomic>

enum class EvictionStage {
    NONE = 0,
    DISCARD_SCRATCH = 1,
    SHRINK_CONTEXT = 2,
    EVICT_RETRIEVAL = 3,
    UNLOAD_WARM = 4,
    REJECT_OPTIONAL = 5,
    ROUTE_CLOUD = 6
};

struct SystemResourceSnapshot {
    size_t host_total_ram_bytes = 0;
    size_t host_available_ram_bytes = 0;
    size_t host_process_rss_bytes = 0;
    size_t vram_total_bytes = 0;
    size_t vram_safe_ceiling_bytes = 0;
    size_t vram_allocated_bytes = 0;
    size_t inference_memory_bytes = 0;
    size_t kv_cache_bytes = 0;
    size_t vector_store_bytes = 0;
    size_t model_weights_bytes = 0;
    int max_threads = 2;
    bool memory_pressure = false;
    EvictionStage current_eviction_stage = EvictionStage::NONE;
};

class ResourceGovernor {
public:
    explicit ResourceGovernor(VulkanDevice* gpu_device = nullptr);

    // Static system queries
    static size_t get_host_total_ram_bytes();
    static size_t get_host_available_ram_bytes();
    static size_t get_process_rss_bytes();
    static int get_max_allowed_threads();
    static void enforce_thread_limits();

    // Snapshot and pressure assessment
    SystemResourceSnapshot get_snapshot() const;
    bool is_under_memory_pressure() const;

    // Component memory accounting
    void track_inference_memory(size_t bytes);
    void track_kv_cache(size_t bytes);
    void track_vector_store(size_t bytes);
    void track_model_weights(size_t bytes);
    size_t get_total_tracked_bytes() const;

    // Headroom safety checks
    bool can_admit_host_ram(size_t required_bytes) const;

    // Proactive 6-stage eviction cascade
    EvictionStage assess_eviction_stage() const;
    bool should_route_to_cloud() const;
    bool should_reject_optional_load() const;

private:
    VulkanDevice* gpu_device_ = nullptr;
    size_t max_allowed_ram_bytes_ = 0;

    std::atomic<size_t> inference_mem_{0};
    std::atomic<size_t> kv_cache_mem_{0};
    std::atomic<size_t> vector_store_mem_{0};
    std::atomic<size_t> model_weights_mem_{0};
};
