#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

struct SessionTurn {
    uint32_t turn_index = 0;
    int64_t timestamp = 0;
    std::string role;
    std::string content;
    std::string tool_name;
    std::string tool_arguments;
    std::string tool_result;
};

class SessionMemory {
public:
    explicit SessionMemory(const std::string& session_id = "");

    void set_session_id(const std::string& id);
    std::string get_session_id() const;

    void add_message(const std::string& role, const std::string& content);
    void add_tool_interaction(const std::string& tool_name,
                              const std::string& arguments,
                              const std::string& result);
    void add_decision(const std::string& decision);

    std::vector<SessionTurn> get_turns() const;
    std::vector<std::string> get_decisions() const;
    size_t turn_count() const;

    std::string format_transcript() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::string session_id_;
    std::vector<SessionTurn> turns_;
    std::vector<std::string> decisions_;
};
