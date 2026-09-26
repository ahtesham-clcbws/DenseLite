#include "ResponseAnalyzer.hpp"
#include "../dependencies/json.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

ResponseAction ResponseAnalyzer::analyze(const std::string& raw_output,
                                        const std::string& stop_reason) {
    if (raw_output.empty()) return ResponseAction::MODEL_ERROR;

    // 1. Explicit stop reason check
    if (stop_reason == "length" || stop_reason == "max_tokens") {
        return ResponseAction::MODEL_CONTINUE;
    }
    if (stop_reason == "error") {
        return ResponseAction::MODEL_ERROR;
    }

    // 2. Structured JSON inspection
    bool attempted_json = false;
    std::string trimmed = raw_output;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
        trimmed.erase(trimmed.begin());
    }
    if (!trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[')) {
        attempted_json = true;
    }

    try {
        json j = json::parse(raw_output);

        if (j.contains("error")) {
            return ResponseAction::MODEL_ERROR;
        }

        // OpenAI format
        if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
            const auto& choice = j["choices"][0];
            
            if (choice.contains("finish_reason") && choice["finish_reason"].is_string()) {
                std::string fr = choice["finish_reason"].get<std::string>();
                if (fr == "length") return ResponseAction::MODEL_CONTINUE;
                if (fr == "tool_calls") return ResponseAction::TOOL_CALL;
            }

            if (choice.contains("message") && choice["message"].is_object()) {
                const auto& msg = choice["message"];
                if (msg.contains("tool_calls") && msg["tool_calls"].is_array() && !msg["tool_calls"].empty()) {
                    return ResponseAction::TOOL_CALL;
                }
            }
        }
        
        // Gemini format
        if (j.contains("candidates") && j["candidates"].is_array() && !j["candidates"].empty()) {
            const auto& cand = j["candidates"][0];
            if (cand.contains("finishReason") && cand["finishReason"].is_string()) {
                std::string fr = cand["finishReason"].get<std::string>();
                if (fr == "MAX_TOKENS") return ResponseAction::MODEL_CONTINUE;
                if (fr == "SAFETY" || fr == "RECITATION") return ResponseAction::INVALID;
            }
        }

        return ResponseAction::COMPLETE;
    } catch (...) {
        // If the output looked like JSON but threw, it is malformed JSON -> INVALID
        if (attempted_json) {
            if (raw_output.find("\"tool_calls\"") != std::string::npos) return ResponseAction::TOOL_CALL;
            if (raw_output.find("\"error\"") != std::string::npos) return ResponseAction::MODEL_ERROR;
            return ResponseAction::INVALID;
        }
    }

    // 3. Raw text analysis
    // Detect tool call pseudo-markup
    if (raw_output.find("<tool_call>") != std::string::npos ||
        raw_output.find("```tool_code") != std::string::npos) {
        return ResponseAction::TOOL_CALL;
    }

    // Check for garbage / null bytes
    for (char c : raw_output) {
        if (c == '\0' || (static_cast<unsigned char>(c) < 32 && c != '\n' && c != '\r' && c != '\t')) {
            return ResponseAction::INVALID;
        }
    }

    return ResponseAction::COMPLETE;
}
