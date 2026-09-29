#pragma once

#include "RequestAnalyzer.hpp"
#include "model.hpp"
#include <string>

struct RoutingDecision {
    std::string intent; // e.g., "reasoning", "coding", "image", "audio", "text"
    std::string domain; // e.g., "general", "software", "creative"
    std::string complexity; // e.g., "low", "high"
    std::string required_capabilities; // e.g., "none", "code_execution"
};

class Router {
public:
    // Analyzes and categorizes request using active strategy (ModernBERT, Needle, or fast fallback)
    static RoutingDecision analyze_request(const OpenAIRequest& req, DenseModel* needle_model = nullptr);

    // Helper to parse JSON string from routing models into RoutingDecision struct
    static RoutingDecision parse_decision_json(const std::string& json_output);
};

// Backward-compatibility alias
using NeedleRouter = Router;
