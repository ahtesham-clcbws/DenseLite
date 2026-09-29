#include "decision_engine.hpp"
#include "routing/modernbert_router.hpp"
#include "RequestAnalyzer.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

static std::string to_lower_copy(const std::string& str) {
    std::string out = str;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

DecisionEngine& DecisionEngine::instance() {
    static DecisionEngine inst;
    return inst;
}

DecisionOutput DecisionEngine::fast_heuristic_decide(const std::string& query, const OpenAIRequest* req) {
    DecisionOutput out;
    std::string q = to_lower_copy(query);

    // If query empty, check last message of req
    if (q.empty() && req && !req->messages.empty()) {
        q = to_lower_copy(req->messages.back().content);
    }

    const std::vector<std::string> code_indicators = {
        ".cpp", ".hpp", ".h", ".c", ".ts", ".js", ".py", ".rs", ".go", ".php", ".sql", ".json",
        "function", "class ", "struct ", "refactor", "compile", "cmake", "segfault", "undefined",
        "int ", "void ", "const ", "impl", "enum ", "namespace", "include <", "import "
    };
    const std::vector<std::string> trouble_indicators = {
        "error:", "exception", "failed", "crash", "bug", "stack trace", "warning:", "broken", "fix"
    };
    const std::vector<std::string> memory_indicators = {
        "remember", "convention", "architecture", "rule", "standard", "guideline", "workspace", "project config"
    };
    const std::vector<std::string> web_indicators = {
        "latest", "recent", "search online", "changelog", "news", "documentation for v"
    };

    bool is_code = false;
    for (const auto& ind : code_indicators) {
        if (q.find(ind) != std::string::npos) { is_code = true; break; }
    }
    bool is_trouble = false;
    for (const auto& ind : trouble_indicators) {
        if (q.find(ind) != std::string::npos) { is_trouble = true; break; }
    }

    if (is_trouble) {
        out.domain = "troubleshooting";
        out.intent = is_code ? "coding" : "reasoning";
        out.requires_code_context = true;
        out.suggested_action = "inspect_repository";
        out.confidence = 0.90f;
    } else if (is_code) {
        out.domain = "coding";
        out.intent = "coding";
        out.requires_code_context = true;
        out.suggested_action = "inspect_repository";
        out.confidence = 0.92f;
    } else if (q.find("doc") != std::string::npos || q.find("readme") != std::string::npos) {
        out.domain = "documentation";
        out.intent = "text";
        out.requires_code_context = true;
        out.suggested_action = "inspect_repository";
        out.confidence = 0.85f;
    } else {
        out.domain = "general";
        out.intent = "text";
        out.suggested_action = "direct_generate";
        out.confidence = 0.75f;
    }

    for (const auto& ind : memory_indicators) {
        if (q.find(ind) != std::string::npos) {
            out.requires_memory = true;
            if (out.suggested_action == "direct_generate") out.suggested_action = "retrieve_memory";
            break;
        }
    }
    for (const auto& ind : web_indicators) {
        if (q.find(ind) != std::string::npos) { out.requires_web_search = true; break; }
    }

    // Complexity scoring
    float c_score = 0.1f;
    if (q.size() > 200) c_score += 0.25f;
    if (is_code) c_score += 0.25f;
    if (is_trouble) c_score += 0.20f;
    if (q.find("step by step") != std::string::npos || q.find("refactor") != std::string::npos ||
        q.find("architect") != std::string::npos) c_score += 0.20f;
    out.complexity_score = std::min(1.0f, c_score);

    return out;
}

DecisionOutput DecisionEngine::modernbert_decide(const std::string& query) {
    DecisionOutput out = fast_heuristic_decide(query);
    if (!ModernBERTRouter::instance().is_available()) {
        ModernBERTRouter::instance().initialize();
    }
    if (!ModernBERTRouter::instance().is_available()) {
        return out;
    }

    auto scores = ModernBERTRouter::instance().rank_intents(query);
    if (scores.empty()) return out;

    std::sort(scores.begin(), scores.end(), [](const auto& a, const auto& b) {
        return a.probability > b.probability;
    });

    const auto& top = scores.front();
    out.intent = top.label;
    out.confidence = std::max(out.confidence, top.probability);

    if (top.label == "coding") {
        out.domain = "coding";
        out.requires_code_context = true;
        out.suggested_action = "inspect_repository";
    } else if (top.label == "reasoning") {
        if (out.domain != "troubleshooting") out.domain = "general";
        out.complexity_score = std::max(out.complexity_score, 0.6f);
    }
    return out;
}

DecisionOutput DecisionEngine::decide(const std::string& query, const OpenAIRequest* req) {
    DecisionOutput fast = fast_heuristic_decide(query, req);
    if (fast.confidence >= 0.88f) {
        return fast;
    }
    if (ModernBERTRouter::instance().is_available()) {
        return modernbert_decide(query);
    }
    return fast;
}

float DecisionEngine::evaluate_entailment(const std::string& premise, const std::string& hypothesis) {
    if (premise.empty() || hypothesis.empty()) return 0.0f;
    std::string p = to_lower_copy(premise);
    std::string h = to_lower_copy(hypothesis);

    // Tokenize premise and hypothesis words
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

bool DecisionEngine::is_contradiction(const std::string& premise, const std::string& hypothesis) {
    if (premise.empty() || hypothesis.empty()) return false;
    std::string p = to_lower_copy(premise);
    std::string h = to_lower_copy(hypothesis);

    const std::vector<std::pair<std::string, std::string>> polarities = {
        {"enabled", "disabled"}, {"true", "false"}, {"active", "inactive"},
        {"allowed", "forbidden"}, {"supported", "deprecated"}
    };
    for (const auto& pair : polarities) {
        if ((p.find(pair.first) != std::string::npos && h.find(pair.second) != std::string::npos) ||
            (p.find(pair.second) != std::string::npos && h.find(pair.first) != std::string::npos)) {
            return true;
        }
    }
    return false;
}

DecisionOutput RequestAnalyzer::analyze_decision(const OpenAIRequest& req) {
    std::string last_msg = "";
    if (!req.messages.empty()) {
        last_msg = req.messages.back().content;
    }
    return DecisionEngine::instance().decide(last_msg, &req);
}

