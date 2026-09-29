#include "Router.hpp"
#include "routing/modernbert_router.hpp"
#include "settings/settings_manager.hpp"
#include "infer.hpp"
#include <iostream>

#include "../dependencies/json.hpp"
using json = nlohmann::json;

RoutingDecision Router::parse_decision_json(const std::string& json_output) {
    RoutingDecision decision = {"text", "general", "low", "none"};
    try {
        size_t start = json_output.find_first_of('{');
        size_t end = json_output.find_last_of('}');
        if (start != std::string::npos && end != std::string::npos && end >= start) {
            std::string clean_json = json_output.substr(start, end - start + 1);
            json j = json::parse(clean_json);
            if (j.contains("intent") && j["intent"].is_string()) {
                decision.intent = j["intent"];
            }
            if (j.contains("complexity") && j["complexity"].is_string()) {
                decision.complexity = j["complexity"];
            }
            if (j.contains("domain") && j["domain"].is_string()) {
                decision.domain = j["domain"];
            }
            if (j.contains("required_capabilities") && j["required_capabilities"].is_string()) {
                decision.required_capabilities = j["required_capabilities"];
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Router] JSON parsing failed, using fallback: " << e.what() << std::endl;
    }
    return decision;
}

RoutingDecision Router::analyze_request(const OpenAIRequest& req, DenseModel* needle_model) {
    std::string user_message = "";
    if (!req.messages.empty()) {
        user_message = req.messages.back().content;
    }

    auto inf_cfg = SettingsManager::instance().get_inference_config();
    if (inf_cfg.needle3_mode == "modernbert") {
        if (!ModernBERTRouter::instance().is_available()) {
            ModernBERTRouter::instance().initialize("models/modernbert");
        }
        if (ModernBERTRouter::instance().is_available()) {
            return ModernBERTRouter::instance().route(user_message);
        }
    }

    // Fast Path (Sub-millisecond): Avoid CPU generation for obvious/short queries
    std::string fast_cat = RequestAnalyzer::categorize_request(req);
    if (!needle_model || user_message.size() < 120 || fast_cat == "image" || fast_cat == "audio") {
        return {fast_cat, "general", "low", "none"};
    }

    std::string system_prompt = "You are an internal routing model. Analyze the following user message and output a JSON object strictly adhering to this schema: {\"intent\": \"string (reasoning|coding|image|audio|text)\", \"domain\": \"string\", \"complexity\": \"string (low|high)\", \"required_capabilities\": \"string\"}. DO NOT output anything except valid JSON.";
    
    std::string prompt = "<|im_start|>system\n" + system_prompt + "\n<|im_end|>\n<|im_start|>user\n" + user_message + "\n<|im_end|>\n<|im_start|>assistant\n{";
    
    std::vector<int> tokens = tokenize(needle_model->vocab, prompt);
    std::string raw_output = "{";
    generate(*needle_model, tokens, [&](const std::string& token_text) {
        raw_output += token_text;
    }, 128, 0.0f, 1.0f);

    std::cout << "[Router] Raw LLM Decision: " << raw_output << std::endl;
    RoutingDecision decision = parse_decision_json(raw_output);
    
    if (decision.intent != "reasoning" && decision.intent != "coding" && 
        decision.intent != "image" && decision.intent != "audio" && decision.intent != "text") {
        std::cout << "[Router] Hallucinated intent '" << decision.intent << "', falling back to regex." << std::endl;
        decision.intent = RequestAnalyzer::categorize_request(req);
    }
    
    return decision;
}
