#include "ModelEngine.hpp"
#include "httplib.h"
#include "infer.hpp" // For AVX2 inference
#include <iostream>

ModelEngine::ModelEngine(std::map<std::string, DenseModel>& resident_models, SQLiteRouter& router)
    : local_models(resident_models), sqlite_router(router) {}

int ModelEngine::infer(const std::string& model_name, const std::string& prompt, std::string& output) {
    // 1. Ask SQLiteRouter if this is a Cloud model or Local model
    std::string provider = sqlite_router.get_provider_for_model(model_name);

    if (provider == "local" || provider.empty()) {
        return infer_local(model_name, prompt, output);
    } else {
        APIKeyStatus key_status = sqlite_router.get_next_available_key(provider);
        std::string api_key = key_status.key_value;
        std::string provider_url = sqlite_router.get_provider_url(provider);
        return infer_cloud(model_name, provider_url, api_key, prompt, output);
    }
}

int ModelEngine::infer_cloud(const std::string& model_name, const std::string& provider_url, const std::string& api_key, const std::string& prompt, std::string& output) {
    std::cout << "[ModelEngine] Routing inference to Cloud (" << provider_url << ") for model: " << model_name << "\n";
    
    httplib::Client cli(provider_url.c_str());
    cli.set_read_timeout(120);
    
    httplib::Headers headers = {
        {"Authorization", "Bearer " + api_key},
        {"Content-Type", "application/json"}
    };
    
    std::string payload = "{\"model\": \"" + model_name + "\", \"messages\": [{\"role\": \"user\", \"content\": \"" + Formatter::json_escape(prompt) + "\"}]}";
    
    auto res = cli.Post("/v1/chat/completions", headers, payload, "application/json");
    
    if (!res) {
        output = "{\"error\": \"Connection failed\"}";
        return 503;
    }
    
    output = res->body;
    return res->status;
}

#include "Formatter.hpp"

int ModelEngine::infer_local(const std::string& model_name, const std::string& prompt, std::string& output) {
    std::cout << "[ModelEngine] Routing inference to Local AVX2 engine for model: " << model_name << "\n";
    
    auto it = local_models.find(model_name);
    if (it == local_models.end()) {
        output = "Error: Local model not loaded in RAM.";
        return 500;
    }

    std::vector<int> tokens = tokenize(it->second.vocab, prompt);
    
    std::string result_text;
    auto stream_cb = [&](const std::string& text) {
        result_text += text;
        // In a real stream, we'd chunk this back to the client.
    };
    
    generate(it->second, tokens, stream_cb, 512, 0.7f, 1.15f);
    
    output = "{\"choices\": [{\"message\": {\"content\": \"" + Formatter::json_escape(result_text) + "\"}}]}";
    
    return 200;
}
