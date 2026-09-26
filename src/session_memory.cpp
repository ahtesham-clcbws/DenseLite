#include "session_memory.hpp"
#include <chrono>
#include <sstream>

SessionMemory::SessionMemory(const std::string& session_id)
    : session_id_(session_id) {}

void SessionMemory::set_session_id(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    session_id_ = id;
}

std::string SessionMemory::get_session_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_id_;
}

void SessionMemory::add_message(const std::string& role, const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex_);
    SessionTurn turn;
    turn.turn_index = static_cast<uint32_t>(turns_.size());
    turn.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    turn.role = role;
    turn.content = content;
    turns_.push_back(std::move(turn));
}

void SessionMemory::add_tool_interaction(const std::string& tool_name,
                                         const std::string& arguments,
                                         const std::string& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    SessionTurn turn;
    turn.turn_index = static_cast<uint32_t>(turns_.size());
    turn.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    turn.role = "tool";
    turn.tool_name = tool_name;
    turn.tool_arguments = arguments;
    turn.tool_result = result;
    turns_.push_back(std::move(turn));
}

void SessionMemory::add_decision(const std::string& decision) {
    std::lock_guard<std::mutex> lock(mutex_);
    decisions_.push_back(decision);
}

std::vector<SessionTurn> SessionMemory::get_turns() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return turns_;
}

std::vector<std::string> SessionMemory::get_decisions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return decisions_;
}

size_t SessionMemory::turn_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return turns_.size();
}

std::string SessionMemory::format_transcript() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    for (const auto& t : turns_) {
        if (t.role == "tool") {
            oss << "[Tool: " << t.tool_name << "] Args: " << t.tool_arguments
                << "\nResult: " << t.tool_result << "\n";
        } else {
            oss << "[" << t.role << "] " << t.content << "\n";
        }
    }
    if (!decisions_.empty()) {
        oss << "[Decisions Made]\n";
        for (const auto& d : decisions_) {
            oss << " - " << d << "\n";
        }
    }
    return oss.str();
}

void SessionMemory::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    turns_.clear();
    decisions_.clear();
}
