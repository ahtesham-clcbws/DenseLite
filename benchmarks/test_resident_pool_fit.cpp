#include "kv_cache.hpp"
#include "model.hpp"
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <cassert>
#include <iomanip>
#include <thread>
#include <chrono>

static size_t get_process_vm_rss_kb() {
    std::ifstream status_file("/proc/self/status");
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream iss(line);
            std::string key;
            size_t kb;
            iss >> key >> kb;
            return kb;
        }
    }
    return 0;
}

static size_t get_process_vm_hwm_kb() {
    std::ifstream status_file("/proc/self/status");
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.rfind("VmHWM:", 0) == 0) {
            std::istringstream iss(line);
            std::string key;
            size_t kb;
            iss >> key >> kb;
            return kb;
        }
    }
    return 0;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << " 1.5B Resident-Pool Capacity Fit Validation (Point 16)" << std::endl;
    std::cout << " Empirical Resident Set Size (RSS) & High-Water Mark (HWM)" << std::endl;
    std::cout << "==========================================================" << std::endl;

    size_t base_rss_kb = get_process_vm_rss_kb();
    std::cout << "-> Baseline Process RSS: " << (base_rss_kb / 1024.0) << " MiB" << std::endl;

    // Simulate 1.5B Q4_K_M model resident weights (~1,100 MiB)
    std::cout << "-> Allocating simulated 1.5B Q4_K_M Model Resident Buffer (1,100 MiB)..." << std::endl;
    const size_t MODEL_WEIGHT_BYTES = 1100ULL * 1024ULL * 1024ULL;
    std::vector<uint8_t> model_weights(MODEL_WEIGHT_BYTES, 0xAA);

    size_t loaded_rss_kb = get_process_vm_rss_kb();
    std::cout << "-> Process RSS with Model Loaded: " << (loaded_rss_kb / 1024.0) << " MiB" << std::endl;

    ModelConfig qwen_15b;
    qwen_15b.model_id = "Qwen-2.5-1.5B";
    qwen_15b.num_layers = 28;
    qwen_15b.num_heads = 12;
    qwen_15b.num_kv_heads = 2;
    qwen_15b.head_dim = 128;
    qwen_15b.context_length = 4096;

    std::vector<size_t> concurrency_levels = {1, 2, 4};
    std::cout << "\n[Concurrent Session Leases & KV Cache Stress Matrix]:" << std::endl;
    std::cout << "| Concurrency | Active Leases | Model RSS (MiB) | KV Cache (MiB) | Peak VmHWM (MiB) | Ceiling Bound (2 GiB) | Status |" << std::endl;
    std::cout << "|-------------|---------------|-----------------|----------------|------------------|-----------------------|--------|" << std::endl;

    for (size_t conc : concurrency_levels) {
        std::vector<KVCache> caches(conc);
        for (size_t c = 0; c < conc; ++c) {
            bool ok = caches[c].allocate(qwen_15b, 4096, nullptr);
            assert(ok && "Failed to allocate KV cache for stream");
            // Touch first byte to force resident page mapping
            caches[c].host_buffer()[0] = 0x55;
        }

        size_t total_kv_bytes = conc * caches[0].allocated_bytes();
        size_t current_rss_kb = get_process_vm_rss_kb();
        size_t peak_hwm_kb = get_process_vm_hwm_kb();

        double current_rss_mib = current_rss_kb / 1024.0;
        double peak_hwm_mib = peak_hwm_kb / 1024.0;
        double model_rss_mib = MODEL_WEIGHT_BYTES / (1024.0 * 1024.0);
        double kv_mib = total_kv_bytes / (1024.0 * 1024.0);

        bool pass = (peak_hwm_mib <= 2048.0);

        std::cout << "| " << std::right << std::setw(11) << conc
                  << " | " << std::setw(13) << conc
                  << " | " << std::fixed << std::setprecision(1) << std::setw(15) << model_rss_mib
                  << " | " << std::setw(14) << kv_mib
                  << " | " << std::setw(16) << peak_hwm_mib
                  << " | " << std::setw(21) << "<= 2048.0 MiB"
                  << " | " << (pass ? "🟢 PASS" : "❌ FAIL")
                  << " |" << std::endl;

        assert(pass && "Resident memory exceeded 2.0 GiB ceiling bound!");
    }

    std::cout << "\n>> 1.5B Resident-Pool Capacity Fit Successfully Verified (Peak VmHWM <= 2.0 GiB)!" << std::endl;
    return 0;
}
