#pragma once
#include <thread>
#include <algorithm>
#include <fstream>
#include <string>
#include <iostream>

#include <omp.h>

class HardwareManager {
public:
    static int get_max_allowed_threads() {
        // Enforce 50% CPU capacity. e.g., 4 threads -> 2 max
        int hw_concurrency = std::thread::hardware_concurrency();
        if (hw_concurrency == 0) hw_concurrency = 4; // fallback
        
        int allowed_threads = std::max(1, hw_concurrency / 2);
        return allowed_threads;
    }

    static size_t get_max_allowed_ram_bytes() {
        // Enforce 45% RAM capacity
        // Linux specific parsing of /proc/meminfo
        size_t total_ram_kb = 0;
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.rfind("MemTotal:", 0) == 0) {
                sscanf(line.c_str(), "MemTotal: %zu kB", &total_ram_kb);
                break;
            }
        }
        if (total_ram_kb == 0) return 4ULL * 1024 * 1024 * 1024; // Fallback to 4GB limits if reading fails

        size_t total_ram_bytes = total_ram_kb * 1024;
        return static_cast<size_t>(total_ram_bytes * 0.45);
    }

    static void enforce_limits() {
        int threads = get_max_allowed_threads();
        size_t ram = get_max_allowed_ram_bytes();
        std::cout << "[HardwareManager] Enforcing Limits -> Max Threads: " << threads 
                  << " | Max RAM: " << (ram / (1024 * 1024)) << " MB" << std::endl;
        omp_set_num_threads(threads);
    }
};
