#pragma once

#include "RequestAnalyzer.hpp"
#include "model.hpp"
#include <string>

struct RoutingDecision {
    std::string intent; // e.g., "reasoning", "coding", "text", "image"
    std::string domain; // e.g., "general", "software", "creative"
    std::string complexity; // e.g., "low", "high"
    std::string required_capabilities; // e.g., "none", "code_execution"
};

class NeedleRouter {
public:
    // Uses the needle model to categorize the request and returns a parsed RoutingDecision
    // If it fails or the model isn't available, it falls back to the keyword classifier
    static RoutingDecision analyze_request(const OpenAIRequest& req, DenseModel* needle_model);

    // Helper to parse the JSON string from needle into the RoutingDecision struct
    static RoutingDecision parse_decision_json(const std::string& json_output);
};
