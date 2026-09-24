#include "ResponseAnalyzer.hpp"

#include "../dependencies/json.hpp"
#include <iostream>

using json = nlohmann::json;

ResponseAction ResponseAnalyzer::analyze(const std::string& raw_output) {
    if (raw_output.empty()) return ResponseAction::MODEL_ERROR;

    try {
        json j = json::parse(raw_output);

        if (j.contains("error")) {
            return ResponseAction::MODEL_ERROR;
        }

        if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
            const auto& choice = j["choices"][0];
            
            if (choice.contains("finish_reason") && choice["finish_reason"].is_string()) {
                std::string fr = choice["finish_reason"].get<std::string>();
                if (fr == "length") return ResponseAction::INCOMPLETE;
                if (fr == "tool_calls") return ResponseAction::TOOL_CALL;
            }

            if (choice.contains("message") && choice["message"].is_object()) {
                const auto& msg = choice["message"];
                if (msg.contains("tool_calls") && msg["tool_calls"].is_array() && !msg["tool_calls"].empty()) {
                    return ResponseAction::TOOL_CALL;
                }
            }
        }
        
        // Check Gemini format error
        if (j.contains("error")) {
            return ResponseAction::MODEL_ERROR;
        }
        
        // Gemini finishReason is inside candidates
        if (j.contains("candidates") && j["candidates"].is_array() && !j["candidates"].empty()) {
            const auto& cand = j["candidates"][0];
            if (cand.contains("finishReason") && cand["finishReason"].is_string()) {
                std::string fr = cand["finishReason"].get<std::string>();
                if (fr == "MAX_TOKENS") return ResponseAction::INCOMPLETE;
            }
        }

    } catch (...) {
        std::cerr << "[ResponseAnalyzer] Error parsing JSON: " << raw_output.substr(0, 100) << std::endl;
        // Fallback to basic string matching if parsing fails entirely, e.g. from a raw streaming partial output
        if (raw_output.find("\"tool_calls\"") != std::string::npos) return ResponseAction::TOOL_CALL;
        if (raw_output.find("\"error\"") != std::string::npos) return ResponseAction::MODEL_ERROR;
    }

    return ResponseAction::COMPLETE;
}
