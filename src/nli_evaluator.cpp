#include "nli_evaluator.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

static std::string to_lower_copy(const std::string& input) {
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

NLIEvaluator& NLIEvaluator::instance() {
    static NLIEvaluator inst;
    return inst;
}

float NLIEvaluator::entailment_score(const std::string& premise, const std::string& hypothesis) {
    if (premise.empty() || hypothesis.empty()) return 0.0f;
    std::string p = to_lower_copy(premise);
    std::string h = to_lower_copy(hypothesis);

    std::istringstream h_iss(h);
    std::string word;
    int matches = 0;
    int total_h_words = 0;
    while (h_iss >> word) {
        if (word.size() > 2) {
            total_h_words++;
            if (p.find(word) != std::string::npos) {
                matches++;
            }
        }
    }
    if (total_h_words == 0) return 1.0f;
    return std::min(1.0f, static_cast<float>(matches) / static_cast<float>(total_h_words));
}

bool NLIEvaluator::is_contradiction(const std::string& premise, const std::string& hypothesis) {
    if (premise.empty() || hypothesis.empty()) return false;
    std::string p = to_lower_copy(premise);
    std::string h = to_lower_copy(hypothesis);

    const std::vector<std::pair<std::string, std::string>> polarities = {
        {"enabled", "disabled"}, {"true", "false"}, {"active", "inactive"},
        {"allowed", "forbidden"}, {"supported", "deprecated"}, {"yes", "no"}
    };
    
    for (const auto& pair : polarities) {
        if ((p.find(pair.first) != std::string::npos && h.find(pair.second) != std::string::npos) ||
            (p.find(pair.second) != std::string::npos && h.find(pair.first) != std::string::npos)) {
            return true;
        }
    }
    return false;
}

NLILabel NLIEvaluator::evaluate(const std::string& premise, const std::string& hypothesis) {
    if (is_contradiction(premise, hypothesis)) {
        return NLILabel::CONTRADICTION;
    }
    float score = entailment_score(premise, hypothesis);
    if (score >= 0.5f) {
        return NLILabel::ENTAILMENT;
    }
    return NLILabel::NEUTRAL;
}
