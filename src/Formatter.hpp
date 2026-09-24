#pragma once

#include <string>

class Formatter {
public:
    // Format an outgoing token delta for SSE streaming
    static std::string format_sse_delta(const std::string& content_chunk);

    // Format a tool call intercept into an OpenAI compatible tool_calls chunk
    static std::string format_sse_tool_call(const std::string& tool_name, const std::string& tool_arguments, const std::string& session_id);

    // Format the final SSE DONE marker
    static std::string format_sse_done();

    // Utility JSON formatting
    static std::string json_escape(const std::string& s);
    static std::string json_unescape(const std::string& s);
};
