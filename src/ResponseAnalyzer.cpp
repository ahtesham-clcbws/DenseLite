#include "ResponseAnalyzer.hpp"

ResponseAction ResponseAnalyzer::analyze(const std::string& raw_output) {
    if (raw_output.find("\"tool_calls\"") != std::string::npos) {
        return ResponseAction::TOOL_CALL;
    }
    if (raw_output.find("\"error\"") != std::string::npos || raw_output.empty()) {
        return ResponseAction::MODEL_ERROR;
    }
    if (raw_output.find("\"finish_reason\":\"length\"") != std::string::npos) {
        return ResponseAction::INCOMPLETE;
    }
    return ResponseAction::COMPLETE;
}
