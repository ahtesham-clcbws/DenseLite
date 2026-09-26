#pragma once
#include "model.hpp"
#include "model_registry.hpp"
#include "model_lease.hpp"
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>

struct ModelEntry {
    DenseModel model;
    ModelState state = ModelState::COLD;
    DevicePlacement placement = DevicePlacement::CPU_RAM;
    std::atomic<int> active_users{0};
    std::chrono::steady_clock::time_point last_accessed;
    size_t memory_bytes = 0;

    ModelEntry() : last_accessed(std::chrono::steady_clock::now()) {}
};

class ModelPool {
public:
    ModelPool() = default;
    ~ModelPool();

    ModelPool(const ModelPool&) = delete;
    ModelPool& operator=(const ModelPool&) = delete;

    bool acquire_lease(const std::string& id, ModelLease& lease);
    void release_lease(const std::string& id);

    bool add_model(const std::string& id, DenseModel&& model, DevicePlacement placement, size_t mem_bytes);
    bool remove_model(const std::string& id, DenseModel& out_model);

    bool is_in_use(const std::string& id) const;
    bool can_unload(const std::string& id) const;

    ModelState get_state(const std::string& id) const;
    void set_state(const std::string& id, ModelState state);

    DevicePlacement get_placement(const std::string& id) const;
    size_t get_memory_bytes(const std::string& id) const;

    std::vector<std::string> get_loaded_model_ids() const;
    std::vector<std::string> get_evictable_model_ids(std::chrono::seconds idle_timeout) const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<ModelEntry>> entries_;
};
