#include "Formatter.hpp"
#include <chrono>
#include "../dependencies/json.hpp"

using json = nlohmann::json;

std::string Formatter::json_escape(const std::string& s) {
    std::string dumped = json(s).dump();
    return dumped.substr(1, dumped.size() - 2);
}

std::string Formatter::json_unescape(const std::string& s) {
    try {
        std::string quoted = "\"" + s + "\"";
        return json::parse(quoted).get<std::string>();
    } catch (...) {
        return s;
    }
}

std::string Formatter::format_sse_delta(const std::string& content_chunk) {
    json delta = { {"content", content_chunk} };
    json choice = { {"delta", delta} };
    json root = { {"choices", json::array({choice})} };
    return "data: " + root.dump() + "\n\n";
}

std::string Formatter::format_sse_tool_call(const std::string& tool_name, const std::string& tool_arguments, const std::string& session_id) {
    json function = { {"name", tool_name}, {"arguments", tool_arguments} };
    json call = { {"index", 0}, {"id", "call_" + session_id}, {"type", "function"}, {"function", function} };
    json delta = { {"tool_calls", json::array({call})} };
    json choice = { {"delta", delta} };
    json root = { {"choices", json::array({choice})} };
    return "data: " + root.dump() + "\n\n";
}

std::string Formatter::format_sse_done() {
    return "data: [DONE]\n\n";
}
