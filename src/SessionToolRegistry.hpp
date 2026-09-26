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

private:
    SessionToolRegistry() = default;
    mutable std::mutex mutex_;
    // session_id -> (tool_name -> OpenAITool)
    std::map<std::string, std::map<std::string, OpenAITool>> session_catalogs_;
};
