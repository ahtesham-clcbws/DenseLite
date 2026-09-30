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

    // Heuristic evaluate: uses word overlap. Do not use for hard semantic decisions.
    NLILabel heuristic_evaluate(const std::string& premise, const std::string& hypothesis);

    // Heuristic contradiction: uses simple polarity dictionary. Do not use for hard decisions.
    bool is_heuristic_contradiction(const std::string& premise, const std::string& hypothesis);

    // Heuristic entailment: returns ratio of word overlap.
    float heuristic_entailment_score(const std::string& premise, const std::string& hypothesis);

private:
    NLIEvaluator() = default;
    ~NLIEvaluator() = default;
    NLIEvaluator(const NLIEvaluator&) = delete;
    NLIEvaluator& operator=(const NLIEvaluator&) = delete;
};
