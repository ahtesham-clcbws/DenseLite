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

static std::string json_get_string(const std::string& json, const std::string& key, size_t start_pos = 0) {
    std::string needle = "\"" + key + "\":\"";
    size_t pos = json.find(needle, start_pos);
    if (pos == std::string::npos) return "";
    size_t val_start = pos + needle.size();
    size_t i = val_start;
    while (i < json.size()) {
        if (json[i] == '\\') { i += 2; continue; }
        if (json[i] == '"') break;
        i++;
    }
    if (i >= json.size()) return "";
    return json.substr(val_start, i - val_start);
}

static int json_get_int(const std::string& json, const std::string& key, int default_val = 0) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return default_val;
    size_t val_start = pos + needle.size();
    while (val_start < json.size() && json[val_start] == ' ') val_start++;
    std::string num_str;
    while (val_start < json.size() && (json[val_start] >= '0' && json[val_start] <= '9')) {
        num_str += json[val_start++];
    }
    if (num_str.empty()) return default_val;
    return std::stoi(num_str);
}

static float json_get_float(const std::string& json, const std::string& key, float default_val = 0.0f) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return default_val;
    size_t val_start = pos + needle.size();
    while (val_start < json.size() && json[val_start] == ' ') val_start++;
    std::string num_str;
    while (val_start < json.size() && ((json[val_start] >= '0' && json[val_start] <= '9') || json[val_start] == '.' || json[val_start] == '-')) {
        num_str += json[val_start++];
    }
    if (num_str.empty()) return default_val;
    try { return std::stof(num_str); } catch (...) { return default_val; }
}

OpenAIRequest RequestAnalyzer::parse_request(const std::string& raw_json_body) {
    OpenAIRequest req;
    req.session_id = generate_session_id();
    req.model = json_get_string(raw_json_body, "model");
    if (req.model.empty()) req.model = "denselite";
    req.max_tokens = json_get_int(raw_json_body, "max_completion_tokens", 512);
    if (req.max_tokens <= 0) req.max_tokens = json_get_int(raw_json_body, "max_tokens", 512);
    req.temperature = json_get_float(raw_json_body, "temperature", 0.7f);
    req.repetition_penalty = json_get_float(raw_json_body, "repetition_penalty", 1.15f);

    size_t search_pos = 0;
    while (true) {
        size_t role_pos = raw_json_body.find("\"role\":\"", search_pos);
        if (role_pos == std::string::npos) break;
        
        std::string role = json_get_string(raw_json_body, "role", search_pos);
        std::string content = Formatter::json_unescape(json_get_string(raw_json_body, "content", search_pos));
        
        OpenAIMessage msg;
        msg.role = role;
        msg.content = content;
        req.messages.push_back(msg);
        
        search_pos = role_pos + 8;
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
    for (const auto& msg : req.messages) {
        if (msg.content.find("\"image_url\"") != std::string::npos || 
            msg.content.find("data:image") != std::string::npos) {
            return "image";
        }
    }

    if (!req.messages.empty()) {
        std::string last_msg = req.messages.back().content;
        std::string lower_msg = last_msg;
        std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

        if (lower_msg.find("architect") != std::string::npos ||
            lower_msg.find("plan") != std::string::npos ||
            lower_msg.find("calculate") != std::string::npos ||
            lower_msg.find("solve") != std::string::npos ||
            lower_msg.find("design") != std::string::npos) {
            return "reasoning";
        }

        if (lower_msg.find("code") != std::string::npos ||
            lower_msg.find("function") != std::string::npos ||
            lower_msg.find("script") != std::string::npos ||
            lower_msg.find("c++") != std::string::npos ||
            lower_msg.find("python") != std::string::npos ||
            lower_msg.find("implement") != std::string::npos) {
            return "coding";
        }
    }
    return "text";
}
