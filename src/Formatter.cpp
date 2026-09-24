#include "Formatter.hpp"
#include <chrono>

std::string Formatter::json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string Formatter::json_unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case 'n':  out += '\n'; i++; break;
                case 'r':  out += '\r'; i++; break;
                case 't':  out += '\t'; i++; break;
                case '"':  out += '"';  i++; break;
                case '\\': out += '\\'; i++; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string Formatter::format_sse_delta(const std::string& content_chunk) {
    std::string escaped = json_escape(content_chunk);
    return "data: {\"choices\": [{\"delta\": {\"content\": \"" + escaped + "\"}}]}\n\n";
}

std::string Formatter::format_sse_tool_call(const std::string& tool_name, const std::string& tool_arguments, const std::string& session_id) {
    std::string escaped_args = json_escape(tool_arguments);
    std::string payload = "data: {\"choices\": [{\"delta\": {\"tool_calls\": [{\"index\": 0, \"id\": \"call_" + session_id + "\", \"type\": \"function\", \"function\": {\"name\": \"" + tool_name + "\", \"arguments\": \"" + escaped_args + "\"}}]}}]}\n\n";
    return payload;
}

std::string Formatter::format_sse_done() {
    return "data: [DONE]\n\n";
}
