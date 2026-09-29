#pragma once

#include <string>
#include <vector>
#include <memory>

struct OpenAIRequest;

struct DecisionOutput {
    std::string domain = "general";       // "coding", "troubleshooting", "documentation", "general"
    std::string intent = "text";          // "coding", "reasoning", "image", "audio", "compressor", "text"
    bool requires_code_context = false;
    bool requires_memory = false;
    bool requires_web_search = false;
    float complexity_score = 0.0f;        // 0.0 (trivial) to 1.0 (complex multi-step)
    float confidence = 1.0f;              // 0.0 to 1.0
    std::string suggested_action = "direct_generate"; // "inspect_repository", "retrieve_memory", "direct_generate", "execute_code"
};

class DecisionEngine {
public:
    static DecisionEngine& instance();

    // Dual-Path evaluation: Fast Tier-1 (<1µs) + Deep Tier-2 ModernBERT ONNX
    DecisionOutput decide(const std::string& query, const OpenAIRequest* req = nullptr);

    // Fast-path zero-allocation heuristic evaluation
    DecisionOutput fast_heuristic_decide(const std::string& query, const OpenAIRequest* req = nullptr);

    // ModernBERT ONNX semantic classification
    DecisionOutput modernbert_decide(const std::string& query);

    // NLI Entailment scoring: returns probability [0.0, 1.0] that premise entails hypothesis
    float evaluate_entailment(const std::string& premise, const std::string& hypothesis);

    // Contradiction detection: returns true if premise and hypothesis are mutually exclusive
    bool is_contradiction(const std::string& premise, const std::string& hypothesis);

private:
    DecisionEngine() = default;
    ~DecisionEngine() = default;
    DecisionEngine(const DecisionEngine&) = delete;
    DecisionEngine& operator=(const DecisionEngine&) = delete;
};
