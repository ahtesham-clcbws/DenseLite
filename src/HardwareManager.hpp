#pragma once
#include <thread>
#include <algorithm>
#include <fstream>
#include <string>
#include <iostream>

#include "settings_manager.hpp"

#include <omp.h>

class HardwareManager {
public:
    static int get_max_allowed_threads() {
        int hw_concurrency = std::thread::hardware_concurrency();
        if (hw_concurrency == 0) hw_concurrency = 4; // fallback
        
        int hw_limit = std::max(1, hw_concurrency / 2);

        int configured_threads = SettingsManager::instance().get_server_config().threads;
        if (configured_threads > 0) return std::min(configured_threads, hw_limit);

        return hw_limit;
    }

    static size_t get_max_allowed_ram_bytes() {
        float ram_budget_pct = SettingsManager::instance().get_resource_config().ram_budget_percent;
        if (ram_budget_pct <= 0.0f || ram_budget_pct > 1.0f) ram_budget_pct = 0.50f;

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
        return static_cast<size_t>(total_ram_bytes * ram_budget_pct);
    }

    static void enforce_limits() {
        int threads = get_max_allowed_threads();
        size_t ram = get_max_allowed_ram_bytes();
        std::cout << "[HardwareManager] Enforcing Limits -> Max Threads: " << threads 
                  << " | Max RAM: " << (ram / (1024 * 1024)) << " MB" << std::endl;
        omp_set_num_threads(threads);
    }
};
