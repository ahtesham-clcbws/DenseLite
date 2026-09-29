#pragma once

#include "RequestAnalyzer.hpp"
#include <string>

struct RoutingDecision {
    std::string intent; // "reasoning", "coding", "image", "audio", "text", "compressor"
    std::string domain; // "general", "software", "creative"
    std::string complexity; // "low", "high"
    std::string required_capabilities; // "none", "code_execution"
};

class Router {
public:
    // Routes request directly using MoritzLaurer/ModernBERT-large-zeroshot-v2.0 ONNX model
    static RoutingDecision analyze_request(const OpenAIRequest& req);
};
