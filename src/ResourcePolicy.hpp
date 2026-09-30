#pragma once
#include <thread>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <string>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

class ResourcePolicy {
public:
    static constexpr float DEFAULT_RAM_BUDGET_PCT = 0.50f;
    static constexpr int DEFAULT_THREAD_BUDGET_PCT = 50; // 50% of hardware concurrency

    static int compute_safe_thread_limit(int configured_threads = -1) {
        return thread_limit_for_hardware(std::thread::hardware_concurrency(), configured_threads);
    }

    static int thread_limit_for_hardware(int hw_concurrency, int configured_threads = -1) {
        if (hw_concurrency == 0) hw_concurrency = 4; // fallback
        
        int safe_limit = std::max(1, (hw_concurrency * DEFAULT_THREAD_BUDGET_PCT) / 100);
        
        if (configured_threads > 0) {
            return std::min(configured_threads, safe_limit);
        }
        return safe_limit;
    }

    static size_t compute_safe_ram_ceiling(float configured_pct = -1.0f) {
        float pct = configured_pct;
        if (!(pct > 0.0f)) pct = DEFAULT_RAM_BUDGET_PCT;
        pct = std::min(pct, DEFAULT_RAM_BUDGET_PCT);

        return static_cast<size_t>(host_total_ram_bytes() * pct);
    }

    static size_t host_total_ram_bytes() {
#ifdef __APPLE__
        uint64_t bytes = 0;
        size_t length = sizeof(bytes);
        if (sysctlbyname("hw.memsize", &bytes, &length, nullptr, 0) == 0) return bytes;
#endif
        size_t total_ram_kb = 0;
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.rfind("MemTotal:", 0) == 0) {
                if (sscanf(line.c_str(), "MemTotal: %zu kB", &total_ram_kb) == 1) {
                    break;
                }
            }
        }
        if (total_ram_kb == 0) return 4ULL * 1024 * 1024 * 1024; // Fallback 4GB

        size_t total_ram_bytes = total_ram_kb * 1024;
        return total_ram_bytes;
    }
};
