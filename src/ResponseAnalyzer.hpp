#pragma once
#include <string>

enum class ResponseAction {
    TOOL_CALL,       // Model needs client/Zed to execute something
    MODEL_CONTINUE,  // Hit max_tokens and task is incomplete
    COMPLETE,        // Task finished with evidence
    MODEL_ERROR,     // Provider or local runtime failure
    INVALID          // Malformed or garbage output
};

class ResponseAnalyzer {
public:
    static ResponseAction analyze(const std::string& raw_output,
                                 const std::string& stop_reason = "");
};
