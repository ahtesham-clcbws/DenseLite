#include "resource_governor.hpp"
#include <thread>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <omp.h>

ResourceGovernor::ResourceGovernor(VulkanDevice* gpu_device)
    : gpu_device_(gpu_device) {
    size_t total_ram = get_host_total_ram_bytes();
    // Enforce 45% RAM ceiling as safe operating target
    max_allowed_ram_bytes_ = static_cast<size_t>(total_ram * 0.45);
}

size_t ResourceGovernor::get_host_total_ram_bytes() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) return 16ULL * 1024 * 1024 * 1024;
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            size_t kb = 0;
            if (sscanf(line.c_str(), "MemTotal: %zu kB", &kb) == 1) {
                return kb * 1024;
            }
        }
    }
    return 16ULL * 1024 * 1024 * 1024;
}

size_t ResourceGovernor::get_host_available_ram_bytes() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) return 4ULL * 1024 * 1024 * 1024;
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.rfind("MemAvailable:", 0) == 0) {
            size_t kb = 0;
            if (sscanf(line.c_str(), "MemAvailable: %zu kB", &kb) == 1) {
                return kb * 1024;
            }
        }
    }
    return 4ULL * 1024 * 1024 * 1024;
}

size_t ResourceGovernor::get_process_rss_bytes() {
    std::ifstream statm("/proc/self/statm");
    if (!statm.is_open()) return 0;
    size_t size = 0, resident = 0;
    if (statm >> size >> resident) {
        long page_size = sysconf(_SC_PAGESIZE);
        if (page_size > 0) {
            return resident * static_cast<size_t>(page_size);
        }
    }
    return 0;
}

int ResourceGovernor::get_max_allowed_threads() {
    int hw_concurrency = std::thread::hardware_concurrency();
    if (hw_concurrency == 0) hw_concurrency = 4;
    return std::max(1, hw_concurrency / 2); // 50% CPU allocation
}

void ResourceGovernor::enforce_thread_limits() {
    int threads = get_max_allowed_threads();
    omp_set_num_threads(threads);
}

SystemResourceSnapshot ResourceGovernor::get_snapshot() const {
    SystemResourceSnapshot snap;
    snap.host_total_ram_bytes = get_host_total_ram_bytes();
    snap.host_available_ram_bytes = get_host_available_ram_bytes();
    snap.host_process_rss_bytes = get_process_rss_bytes();
    snap.max_threads = get_max_allowed_threads();

    if (gpu_device_ && gpu_device_->is_available()) {
        auto vram = gpu_device_->memory_info();
        snap.vram_total_bytes = vram.total_vram_bytes;
        snap.vram_safe_ceiling_bytes = vram.safe_ceiling_bytes;
        snap.vram_allocated_bytes = vram.allocated_bytes;
    }

    snap.memory_pressure = is_under_memory_pressure();
    return snap;
}

bool ResourceGovernor::is_under_memory_pressure() const {
    size_t avail = get_host_available_ram_bytes();
    size_t total = get_host_total_ram_bytes();
    // Pressure if available RAM is under 10% of total, or less than 1.5 GB
    if (avail < (total * 0.10) || avail < (1536ULL * 1024 * 1024)) {
        return true;
    }
    // Pressure if process RSS exceeds 14 GB ceiling
    if (get_process_rss_bytes() > (14ULL * 1024 * 1024 * 1024)) {
        return true;
    }
    return false;
}

bool ResourceGovernor::can_admit_host_ram(size_t required_bytes) const {
    size_t avail = get_host_available_ram_bytes();
    size_t min_headroom = 1024ULL * 1024 * 1024; // 1 GB reserve
    return (avail > required_bytes + min_headroom);
}
