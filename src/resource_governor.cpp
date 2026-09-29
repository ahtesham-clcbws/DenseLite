#include "resource_governor.hpp"
#include "settings_manager.hpp"
#include <thread>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <omp.h>

ResourceGovernor::ResourceGovernor(VulkanDevice* gpu_device)
    : gpu_device_(gpu_device) {
    size_t total_ram = get_host_total_ram_bytes();
    float pct = SettingsManager::instance().get_resource_config().ram_budget_percent;
    if (pct <= 0.0f || pct > 1.0f) pct = 0.45f;
    max_allowed_ram_bytes_ = static_cast<size_t>(total_ram * pct);
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
    int cfg_threads = SettingsManager::instance().get_server_config().threads;
    if (cfg_threads > 0) return cfg_threads;
    int hw_concurrency = std::thread::hardware_concurrency();
    if (hw_concurrency == 0) hw_concurrency = 4;
    return std::max(1, hw_concurrency / 2); // 50% CPU allocation
}

void ResourceGovernor::enforce_thread_limits() {
    int threads = get_max_allowed_threads();
    omp_set_num_threads(threads);
}

void ResourceGovernor::track_inference_memory(size_t bytes) { inference_mem_ = bytes; }
void ResourceGovernor::track_kv_cache(size_t bytes) { kv_cache_mem_ = bytes; }
void ResourceGovernor::track_vector_store(size_t bytes) { vector_store_mem_ = bytes; }
void ResourceGovernor::track_model_weights(size_t bytes) { model_weights_mem_ = bytes; }

size_t ResourceGovernor::get_total_tracked_bytes() const {
    return inference_mem_.load() + kv_cache_mem_.load() + 
           vector_store_mem_.load() + model_weights_mem_.load();
}

SystemResourceSnapshot ResourceGovernor::get_snapshot() const {
    SystemResourceSnapshot snap;
    snap.host_total_ram_bytes = get_host_total_ram_bytes();
    snap.host_available_ram_bytes = get_host_available_ram_bytes();
    snap.host_process_rss_bytes = get_process_rss_bytes();
    snap.max_threads = get_max_allowed_threads();
    snap.inference_memory_bytes = inference_mem_.load();
    snap.kv_cache_bytes = kv_cache_mem_.load();
    snap.vector_store_bytes = vector_store_mem_.load();
    snap.model_weights_bytes = model_weights_mem_.load();

    if (gpu_device_ && gpu_device_->is_available()) {
        auto vram = gpu_device_->memory_info();
        snap.vram_total_bytes = vram.total_vram_bytes;
        snap.vram_safe_ceiling_bytes = vram.safe_ceiling_bytes;
        snap.vram_allocated_bytes = vram.allocated_bytes;
    }

    snap.memory_pressure = is_under_memory_pressure();
    snap.current_eviction_stage = assess_eviction_stage();
    return snap;
}

bool ResourceGovernor::is_under_memory_pressure() const {
    size_t avail = get_host_available_ram_bytes();
    size_t total = get_host_total_ram_bytes();
    if (avail < (total * 0.10) || avail < (1536ULL * 1024 * 1024)) return true;
    if (get_process_rss_bytes() > (14ULL * 1024 * 1024 * 1024)) return true;
    return false;
}

bool ResourceGovernor::can_admit_host_ram(size_t required_bytes) const {
    size_t avail = get_host_available_ram_bytes();
    size_t min_headroom = 1024ULL * 1024 * 1024; // 1 GB reserve
    return (avail > required_bytes + min_headroom);
}

EvictionStage ResourceGovernor::assess_eviction_stage() const {
    size_t avail = get_host_available_ram_bytes();
    size_t total = get_host_total_ram_bytes();
    size_t rss = get_process_rss_bytes();

    if (avail < (total * 0.05) || avail < (512ULL * 1024 * 1024) || rss > (14ULL * 1024 * 1024 * 1024)) {
        return EvictionStage::ROUTE_CLOUD;
    }
    if (avail < (total * 0.08) || avail < (1024ULL * 1024 * 1024)) {
        return EvictionStage::REJECT_OPTIONAL;
    }
    if (avail < (total * 0.12) || avail < (1536ULL * 1024 * 1024)) {
        return EvictionStage::UNLOAD_WARM;
    }
    if (avail < (total * 0.16) || avail < (2048ULL * 1024 * 1024)) {
        return EvictionStage::EVICT_RETRIEVAL;
    }
    if (avail < (total * 0.20) || avail < (2560ULL * 1024 * 1024)) {
        return EvictionStage::SHRINK_CONTEXT;
    }
    if (avail < (total * 0.25) || avail < (3072ULL * 1024 * 1024)) {
        return EvictionStage::DISCARD_SCRATCH;
    }
    return EvictionStage::NONE;
}

bool ResourceGovernor::should_route_to_cloud() const {
    return assess_eviction_stage() >= EvictionStage::ROUTE_CLOUD;
}

bool ResourceGovernor::should_reject_optional_load() const {
    return assess_eviction_stage() >= EvictionStage::REJECT_OPTIONAL;
}

size_t ResourceGovernor::calculate_dynamic_context_tokens() {
    size_t total_ram = get_host_total_ram_bytes();
    size_t avail_ram = get_host_available_ram_bytes();
    float pct = SettingsManager::instance().get_resource_config().ram_budget_percent;
    if (pct <= 0.0f || pct > 1.0f) pct = 0.45f;
    size_t base_model_ceiling = static_cast<size_t>(total_ram * pct);

    // Balance RAM headroom available for dynamic KV cache (up to 90% of balance)
    size_t balance_ram = (avail_ram > base_model_ceiling) ? (avail_ram - base_model_ceiling) : (avail_ram / 2);
    size_t dynamic_kv_budget = static_cast<size_t>(balance_ram * 0.90);

    // Qwen 2.5 1.5B (GQA 2 KV heads, 128 head_dim, 28 layers, FP32) = 57,344 bytes/token
    constexpr size_t BYTES_PER_TOKEN = 57344;
    constexpr size_t REQ_64K = 65536ULL * BYTES_PER_TOKEN; // ~3.50 GB
    constexpr size_t REQ_32K = 32768ULL * BYTES_PER_TOKEN; // ~1.75 GB

    if (dynamic_kv_budget >= REQ_64K) {
        return 65536;
    } else if (dynamic_kv_budget >= REQ_32K) {
        return 32768;
    } else if (dynamic_kv_budget >= 16384ULL * BYTES_PER_TOKEN) {
        return 16384;
    } else {
        size_t tokens = dynamic_kv_budget / BYTES_PER_TOKEN;
        tokens = (tokens / 1024) * 1024;
        return std::clamp<size_t>(tokens, 2048, 8192);
    }
}
