#include "RequestAnalyzer.hpp"
#include "Formatter.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>

static std::string generate_session_id() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "req_" + std::to_string(ms);
}

#include "../dependencies/json.hpp"

using json = nlohmann::json;

OpenAIRequest RequestAnalyzer::parse_request(const std::string& raw_json_body) {
    OpenAIRequest req;
    req.session_id = generate_session_id();
    
    try {
        json j = json::parse(raw_json_body);
        req.model = j.value("model", "denselite");
        req.max_tokens = j.value("max_completion_tokens", j.value("max_tokens", 512));
        req.temperature = j.value("temperature", 0.7f);
        req.repetition_penalty = j.value("repetition_penalty", 1.15f);
        
        if (j.contains("messages") && j["messages"].is_array()) {
            for (const auto& msg : j["messages"]) {
                OpenAIMessage m;
                m.role = msg.value("role", "");
                if (msg.contains("content")) {
                    if (msg["content"].is_string()) {
                        m.content = msg["content"].get<std::string>();
                    } else if (msg["content"].is_array()) {
                        std::string combined;
                        for (const auto& part : msg["content"]) {
                            if (part.contains("type") && part["type"] == "text" && part.contains("text")) {
                                combined += part["text"].get<std::string>();
                            }
                        }
                        m.content = combined;
                    }
                }
                req.messages.push_back(m);
            }
        if (j.contains("tools") && j["tools"].is_array()) {
            for (const auto& t : j["tools"]) {
                OpenAITool tool;
                tool.type = t.value("type", "function");
                if (t.contains("function")) {
                    tool.function.name = t["function"].value("name", "");
                    tool.function.description = t["function"].value("description", "");
                    if (t["function"].contains("parameters")) {
                        tool.function.parameters_schema = t["function"]["parameters"].dump();
                    }
                }
                req.tools.push_back(tool);
            }
        }
    } catch (...) {
        std::cerr << "[RequestAnalyzer] Error parsing JSON request body" << std::endl;
    }
    
    return req;
}

std::string RequestAnalyzer::compile_prompt(const OpenAIRequest& req, const std::string& model_architecture) {
    std::string prompt = "";
    
    // Qwen / ChatML format
    for (const auto& msg : req.messages) {
        prompt += "<|im_start|>" + msg.role + "\n";
        prompt += msg.content + "\n<|im_end|>\n";
    }
    
    prompt += "<|im_start|>assistant\n";
    return prompt;
}

std::string RequestAnalyzer::categorize_request(const OpenAIRequest& req) {
    const std::vector<std::string> image_keywords = {"\"image_url\"", "data:image"};
    const std::vector<std::string> reasoning_keywords = {"architect", "plan", "calculate", "solve", "design"};
    const std::vector<std::string> coding_keywords = {"code", "function", "script", "c++", "python", "implement"};

    for (const auto& msg : req.messages) {
        for (const auto& kw : image_keywords) {
            if (msg.content.find(kw) != std::string::npos) return "image";
        }
    }

    if (!req.messages.empty()) {
        std::string last_msg = req.messages.back().content;
        std::string lower_msg = last_msg;
        std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

        for (const auto& kw : reasoning_keywords) {
            if (lower_msg.find(kw) != std::string::npos) return "reasoning";
        }

        for (const auto& kw : coding_keywords) {
            if (lower_msg.find(kw) != std::string::npos) return "coding";
        }
    }
    return "text";
}
