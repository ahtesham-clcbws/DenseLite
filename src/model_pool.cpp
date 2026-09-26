#include "model_pool.hpp"
#include "gguf_parser.hpp"

ModelPool::~ModelPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : entries_) {
        if (pair.second) {
            free_gguf_model(pair.second->model);
        }
    }
    entries_.clear();
}

bool ModelPool::acquire_lease(const std::string& id, ModelLease& lease) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it == entries_.end() || !it->second) {
        return false;
    }

    auto& entry = it->second;
    if (entry->state != ModelState::HOT && entry->state != ModelState::WARM) {
        return false;
    }

    entry->active_users.fetch_add(1, std::memory_order_relaxed);
    entry->last_accessed = std::chrono::steady_clock::now();
    entry->state = ModelState::HOT;

    lease = ModelLease(this, id, &entry->model, entry->placement);
    return true;
}

void ModelPool::release_lease(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it != entries_.end() && it->second) {
        it->second->active_users.fetch_sub(1, std::memory_order_relaxed);
        it->second->last_accessed = std::chrono::steady_clock::now();
    }
}

bool ModelPool::add_model(const std::string& id, DenseModel&& model, DevicePlacement placement, size_t mem_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto entry = std::make_unique<ModelEntry>();
    entry->model = std::move(model);
    entry->state = ModelState::HOT;
    entry->placement = placement;
    entry->memory_bytes = mem_bytes;
    entry->last_accessed = std::chrono::steady_clock::now();

    entries_[id] = std::move(entry);
    return true;
}

bool ModelPool::remove_model(const std::string& id, DenseModel& out_model) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it == entries_.end() || !it->second) {
        return false;
    }

    if (it->second->active_users.load(std::memory_order_relaxed) > 0) {
        // Eviction guard: Cannot unload a model while active leases exist!
        return false;
    }

    out_model = std::move(it->second->model);
    entries_.erase(it);
    return true;
}

bool ModelPool::is_in_use(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it != entries_.end() && it->second) {
        return (it->second->active_users.load(std::memory_order_relaxed) > 0);
    }
    return false;
}

bool ModelPool::can_unload(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it != entries_.end() && it->second) {
        return (it->second->active_users.load(std::memory_order_relaxed) == 0);
    }
    return false;
}

ModelState ModelPool::get_state(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it != entries_.end() && it->second) {
        return it->second->state;
    }
    return ModelState::COLD;
}

void ModelPool::set_state(const std::string& id, ModelState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it != entries_.end() && it->second) {
        it->second->state = state;
    }
}

DevicePlacement ModelPool::get_placement(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it != entries_.end() && it->second) {
        return it->second->placement;
    }
    return DevicePlacement::CPU_RAM;
}

size_t ModelPool::get_memory_bytes(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it != entries_.end() && it->second) {
        return it->second->memory_bytes;
    }
    return 0;
}

std::vector<std::string> ModelPool::get_loaded_model_ids() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(entries_.size());
    for (const auto& pair : entries_) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::vector<std::string> ModelPool::get_evictable_model_ids(std::chrono::seconds idle_timeout) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> evictable;
    auto now = std::chrono::steady_clock::now();
    for (const auto& pair : entries_) {
        if (!pair.second) continue;
        if (pair.second->active_users.load(std::memory_order_relaxed) == 0) {
            if (std::chrono::duration_cast<std::chrono::seconds>(now - pair.second->last_accessed) >= idle_timeout) {
                evictable.push_back(pair.first);
            }
        }
    }
    return evictable;
}
