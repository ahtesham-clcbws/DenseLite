#include "SessionToolRegistry.hpp"
#include <chrono>
#include <limits>

static int64_t current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

SessionToolRegistry& SessionToolRegistry::instance() {
    static SessionToolRegistry reg;
    return reg;
}

void SessionToolRegistry::evict_oldest_locked() {
    if (session_catalogs_.empty()) return;
    auto oldest_it = access_timestamps_.end();
    int64_t oldest_time = std::numeric_limits<int64_t>::max();

    for (auto it = access_timestamps_.begin(); it != access_timestamps_.end(); ++it) {
        if (it->second < oldest_time) {
            oldest_time = it->second;
            oldest_it = it;
        }
    }

    if (oldest_it != access_timestamps_.end()) {
        std::string evicted = oldest_it->first;
        access_timestamps_.erase(oldest_it);
        session_catalogs_.erase(evicted);
    } else {
        auto first = session_catalogs_.begin();
        if (first != session_catalogs_.end()) {
            access_timestamps_.erase(first->first);
            session_catalogs_.erase(first);
        }
    }
}

void SessionToolRegistry::register_tools(const std::string& session_id, const std::vector<OpenAITool>& tools) {
    if (session_id.empty() || tools.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);

    while (session_catalogs_.size() >= max_sessions_ && session_catalogs_.find(session_id) == session_catalogs_.end()) {
        evict_oldest_locked();
    }

    auto& catalog = session_catalogs_[session_id];
    access_timestamps_[session_id] = current_time_ms();
    for (const auto& t : tools) {
        if (!t.function.name.empty()) {
            catalog[t.function.name] = t;
        }
    }
}

bool SessionToolRegistry::has_tools(const std::string& session_id) const {
    if (session_id.empty()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = session_catalogs_.find(session_id);
    if (it != session_catalogs_.end() && !it->second.empty()) {
        const_cast<SessionToolRegistry*>(this)->access_timestamps_[session_id] = current_time_ms();
        return true;
    }
    return false;
}

std::vector<OpenAITool> SessionToolRegistry::get_all_tools(const std::string& session_id) const {
    std::vector<OpenAITool> result;
    if (session_id.empty()) return result;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = session_catalogs_.find(session_id);
    if (it != session_catalogs_.end()) {
        const_cast<SessionToolRegistry*>(this)->access_timestamps_[session_id] = current_time_ms();
        result.reserve(it->second.size());
        for (const auto& pair : it->second) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<OpenAITool> SessionToolRegistry::get_tools_by_names(const std::string& session_id,
                                                               const std::vector<std::string>& tool_names) const {
    std::vector<OpenAITool> result;
    if (session_id.empty() || tool_names.empty()) return result;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = session_catalogs_.find(session_id);
    if (it != session_catalogs_.end()) {
        const_cast<SessionToolRegistry*>(this)->access_timestamps_[session_id] = current_time_ms();
        for (const auto& name : tool_names) {
            auto tool_it = it->second.find(name);
            if (tool_it != it->second.end()) {
                result.push_back(tool_it->second);
            }
        }
    }
    return result;
}

void SessionToolRegistry::clear_session(const std::string& session_id) {
    if (session_id.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    session_catalogs_.erase(session_id);
    access_timestamps_.erase(session_id);
}
