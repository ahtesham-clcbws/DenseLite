#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Router.hpp"

struct ModernBERTScore {
    std::string label;
    float probability = 0.0f;
    float entailment_logit = 0.0f;
    float not_entailment_logit = 0.0f;
};

class ModernBERTRouter {
public:
    static ModernBERTRouter& instance();

    bool initialize(const std::string& model_dir = "");
    bool is_available() const;

    RoutingDecision route(const std::string& user_query);
    std::vector<ModernBERTScore> rank_intents(const std::string& user_query);

private:
    ModernBERTRouter();
    ~ModernBERTRouter();

    ModernBERTRouter(const ModernBERTRouter&) = delete;
    ModernBERTRouter& operator=(const ModernBERTRouter&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};
