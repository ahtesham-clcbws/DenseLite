#include "resource_governor.hpp"
#include "ResourcePolicy.hpp"
#include "settings_manager.hpp"
#include "model.hpp"
#include <thread>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <omp.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif

ResourceGovernor::ResourceGovernor(VulkanDevice* gpu_device)
    : gpu_device_(gpu_device) {
    max_allowed_ram_bytes_ = ResourcePolicy::compute_safe_ram_ceiling(SettingsManager::instance().get_resource_config().ram_budget_percent);
}

size_t ResourceGovernor::get_host_total_ram_bytes() {
    return ResourcePolicy::host_total_ram_bytes();
}

size_t ResourceGovernor::get_host_available_ram_bytes() {
#ifdef __APPLE__
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vm{};
    vm_size_t page_size = 0;
    const auto host = mach_host_self();
    bool ok = host_page_size(host, &page_size) == KERN_SUCCESS &&
        host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count) == KERN_SUCCESS;
    mach_port_deallocate(mach_task_self(), host);
    if (ok) return std::min(get_host_total_ram_bytes(), static_cast<size_t>(vm.free_count + vm.inactive_count) * page_size);
#endif
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
#ifdef __APPLE__
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) return info.resident_size;
#endif
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
    return ResourcePolicy::compute_safe_thread_limit(SettingsManager::instance().get_server_config().threads);
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
    if (get_process_rss_bytes() > max_allowed_ram_bytes_) return true;
    return false;
}

bool ResourceGovernor::can_admit_host_ram(size_t required_bytes) const {
    size_t avail = get_host_available_ram_bytes();
    size_t min_headroom = 1024ULL * 1024 * 1024; // 1 GB reserve
    size_t used = std::max(get_process_rss_bytes(), get_total_tracked_bytes());
    return used <= max_allowed_ram_bytes_ && required_bytes <= max_allowed_ram_bytes_ - used &&
           avail > min_headroom && required_bytes < avail - min_headroom;
}

bool ResourceGovernor::can_admit_gpu_vram(size_t model_bytes, size_t static_overhead_bytes) const {
    if (!gpu_device_ || !gpu_device_->is_available()) return false;
    auto vram = gpu_device_->memory_info();
    size_t required = model_bytes + static_overhead_bytes;
    size_t free_vram = vram.safe_ceiling_bytes > vram.allocated_bytes ? 
                       vram.safe_ceiling_bytes - vram.allocated_bytes : 0;
    return free_vram >= required;
}

EvictionStage ResourceGovernor::assess_eviction_stage() const {
    size_t avail = get_host_available_ram_bytes();
    size_t total = get_host_total_ram_bytes();
    size_t rss = get_process_rss_bytes();

    if (avail < (total * 0.05) || avail < (512ULL * 1024 * 1024) || rss > max_allowed_ram_bytes_) {
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

size_t ResourceGovernor::calculate_dynamic_context_tokens(const ModelConfig* active_model_config) {
    size_t avail_ram = get_host_available_ram_bytes();
    size_t ceiling = ResourcePolicy::compute_safe_ram_ceiling(SettingsManager::instance().get_resource_config().ram_budget_percent);
    size_t rss = get_process_rss_bytes();
    size_t policy_headroom = ceiling > rss ? ceiling - rss : 0;
    size_t reserve = 1024ULL * 1024 * 1024;
    size_t available_headroom = avail_ram > reserve ? avail_ram - reserve : 0;
    size_t dynamic_kv_budget = static_cast<size_t>(std::min(policy_headroom, available_headroom) * 0.90);

    size_t bytes_per_token = 57344; // Default Qwen fallback
    if (active_model_config && active_model_config->num_layers > 0 && active_model_config->num_kv_heads > 0) {
        bytes_per_token = static_cast<size_t>(active_model_config->num_layers) *
                          static_cast<size_t>(active_model_config->num_kv_heads) *
                          static_cast<size_t>(active_model_config->head_dim) * 8ULL;
    }
    if (bytes_per_token == 0) bytes_per_token = 57344;

    size_t req_64k = 65536ULL * bytes_per_token;
    size_t req_32k = 32768ULL * bytes_per_token;

    if (dynamic_kv_budget >= req_64k) {
        return 65536;
    } else if (dynamic_kv_budget >= req_32k) {
        return 32768;
    } else if (dynamic_kv_budget >= 16384ULL * bytes_per_token) {
        return 16384;
    } else {
        size_t tokens = dynamic_kv_budget / bytes_per_token;
        tokens = (tokens / 1024) * 1024;
        return std::min<size_t>(tokens, 8192);
    }
}
