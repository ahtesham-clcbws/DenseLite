#pragma once
#include <string>

enum class ResponseAction {
    TOOL_CALL,
    INCOMPLETE,
    COMPLETE,
    MODEL_ERROR
};

class ResponseAnalyzer {
public:
    static ResponseAction analyze(const std::string& raw_output);
};
