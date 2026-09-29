#pragma once
#include <string>
#include <vector>

enum class NLILabel {
    ENTAILMENT,
    CONTRADICTION,
    NEUTRAL
};

class NLIEvaluator {
public:
    static NLIEvaluator& instance();

    // Evaluate premise against hypothesis to return a label
    NLILabel evaluate(const std::string& premise, const std::string& hypothesis);

    // Contradiction detection: returns true if premise and hypothesis are mutually exclusive
    bool is_contradiction(const std::string& premise, const std::string& hypothesis);

    // NLI Entailment scoring: returns probability [0.0, 1.0] that premise entails hypothesis
    float entailment_score(const std::string& premise, const std::string& hypothesis);

private:
    NLIEvaluator() = default;
    ~NLIEvaluator() = default;
    NLIEvaluator(const NLIEvaluator&) = delete;
    NLIEvaluator& operator=(const NLIEvaluator&) = delete;
};
