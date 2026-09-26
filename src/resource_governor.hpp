#pragma once
#include "vulkan_device.hpp"
#include <cstddef>
#include <string>

struct SystemResourceSnapshot {
    size_t host_total_ram_bytes = 0;
    size_t host_available_ram_bytes = 0;
    size_t host_process_rss_bytes = 0;
    size_t vram_total_bytes = 0;
    size_t vram_safe_ceiling_bytes = 0;
    size_t vram_allocated_bytes = 0;
    int max_threads = 2;
    bool memory_pressure = false;
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

    // Headroom safety checks
    bool can_admit_host_ram(size_t required_bytes) const;

private:
    VulkanDevice* gpu_device_ = nullptr;
    size_t max_allowed_ram_bytes_ = 0;
};
