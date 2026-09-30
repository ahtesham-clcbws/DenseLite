#pragma once

#include <string>
#include <vector>
#include <map>

struct DecisionOutput;

// Structs representing parsed OpenAI requests
struct OpenAIToolFunction {
    std::string name;
    std::string description;
    std::string parameters_schema; // Raw JSON string of the schema
};

struct OpenAITool {
    std::string type; // usually "function"
    OpenAIToolFunction function;
};

struct OpenAIMessage {
    std::string role;
    std::string content;
    std::string name;
    std::string tool_call_id;
};

struct OpenAIRequest {
    std::string model;
    std::vector<OpenAIMessage> messages;
    std::vector<OpenAITool> tools;
    int max_tokens;
    float temperature;
    float repetition_penalty;
    std::string session_id; // Added for session-based isolation
    bool use_context = true; // Set to false to disable external RAG/symbol context injection
    bool stream = true;      // Parse client streaming preference (default SSE stream)
};

#include "model.hpp"

class RequestAnalyzer {
public:
    // Parse an incoming raw JSON body from a POST /v1/chat/completions request
    static OpenAIRequest parse_request(const std::string& raw_json_body);

    // Compile the messages and tools into a single context string optimized for the target model
    static std::string compile_prompt(const OpenAIRequest& req, const ModelConfig& config);

    // ----------------------------------------------------------------------
    // THE INTELLIGENT BRIDGE (Semantic Intent Router)
    // ----------------------------------------------------------------------

    // 1. Analyzing (Prompt Classifier)
    // Categorizes the request based on content (e.g., "reasoning", "image", "text", "coding")
    static std::string categorize_request(const OpenAIRequest& req);

    // 2. Decision Engine (Phase 2): Generates rich decision context and routing action
    static struct DecisionOutput analyze_decision(const OpenAIRequest& req);
};
