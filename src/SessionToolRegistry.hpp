#pragma once
#include "RequestAnalyzer.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>

class SessionToolRegistry {
public:
    static SessionToolRegistry& instance();

    // Register tools from initial handshake or subsequent turn
    void register_tools(const std::string& session_id, const std::vector<OpenAITool>& tools);

    // Check if tools are already cached for this session
    bool has_tools(const std::string& session_id) const;

    // Retrieve all cached tools for a session
    std::vector<OpenAITool> get_all_tools(const std::string& session_id) const;

    // Retrieve specific tool schemas by name (Selective Injection)
    std::vector<OpenAITool> get_tools_by_names(const std::string& session_id, 
                                               const std::vector<std::string>& tool_names) const;

    // Clear session tool cache
    void clear_session(const std::string& session_id);

    // Set maximum session tool catalogs in RAM
    void set_max_sessions(size_t limit) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_sessions_ = limit;
    }
    size_t get_session_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return session_catalogs_.size();
    }

private:
    SessionToolRegistry() = default;
    void evict_oldest_locked();

    mutable std::mutex mutex_;
    size_t max_sessions_ = 32;
    // session_id -> (tool_name -> OpenAITool)
    std::map<std::string, std::map<std::string, OpenAITool>> session_catalogs_;
    std::map<std::string, int64_t> access_timestamps_;
};
