#include "Router.hpp"
#include "routing/modernbert_router.hpp"
#include "settings/settings_manager.hpp"
#include <iostream>

RoutingDecision Router::analyze_request(const OpenAIRequest& req) {
    std::string user_message = "";
    if (!req.messages.empty()) {
        user_message = req.messages.back().content;
    }

    // Direct routing via MoritzLaurer/ModernBERT-large-zeroshot-v2.0
    if (!ModernBERTRouter::instance().is_available()) {
        ModernBERTRouter::instance().initialize();
    }

    if (ModernBERTRouter::instance().is_available()) {
        return ModernBERTRouter::instance().route(user_message);
    }

    return {"general", "general", "low", "none"};
}
