#include "SessionToolRegistry.hpp"

SessionToolRegistry& SessionToolRegistry::instance() {
    static SessionToolRegistry reg;
    return reg;
}

void SessionToolRegistry::register_tools(const std::string& session_id, const std::vector<OpenAITool>& tools) {
    if (session_id.empty() || tools.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto& catalog = session_catalogs_[session_id];
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
    return (it != session_catalogs_.end() && !it->second.empty());
}

std::vector<OpenAITool> SessionToolRegistry::get_all_tools(const std::string& session_id) const {
    std::vector<OpenAITool> result;
    if (session_id.empty()) return result;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = session_catalogs_.find(session_id);
    if (it != session_catalogs_.end()) {
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
}
