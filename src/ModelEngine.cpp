#include "ModelEngine.hpp"
#include "httplib.h"
#include "infer.hpp" // For AVX2 inference
#include <iostream>

ModelEngine::ModelEngine(std::map<std::string, DenseModel>& resident_models, SQLiteRouter& router)
    : local_models(resident_models), sqlite_router(router) {}

#include "../dependencies/json.hpp"

using json = nlohmann::json;

int ModelEngine::infer(const std::string& model_name, const OpenAIRequest& req, const std::string& compiled_prompt, std::string& output) {
    // 1. Ask SQLiteRouter if this is a Cloud model or Local model
    std::string provider = sqlite_router.get_provider_for_model(model_name);

    if (provider == "local" || provider.empty()) {
        return infer_local(model_name, compiled_prompt, output);
    } else {
        APIKeyStatus key_status = sqlite_router.get_next_available_key(provider);
        std::string api_key = key_status.key_value;
        std::string provider_url = sqlite_router.get_provider_url(provider);
        return infer_cloud(model_name, provider_url, api_key, req, output);
    }
}

int ModelEngine::infer_cloud(const std::string& model_name, const std::string& provider_url, const std::string& api_key, const OpenAIRequest& req, std::string& output) {
    std::cout << "[ModelEngine] Routing inference to Cloud (" << provider_url << ") for model: " << model_name << "\n";
    
    // Support Gemini v1beta formatting vs standard OpenAI
    bool is_gemini = (provider_url.find("generativelanguage") != std::string::npos);
    bool is_openrouter = (provider_url.find("openrouter") != std::string::npos);
    std::string endpoint = is_gemini ? "/v1beta/models/" + model_name + ":generateContent" 
                                     : (is_openrouter ? "/api/v1/chat/completions" : "/v1/chat/completions");

    try {
        httplib::Client cli(provider_url.c_str());
        cli.set_read_timeout(120);
        
        httplib::Headers headers = {
            {"Content-Type", "application/json"}
        };
        
        if (is_gemini) {
            headers.emplace("x-goog-api-key", api_key);
        } else {
            headers.emplace("Authorization", "Bearer " + api_key);
        }
        
        json payload = json::object();
        
        if (is_gemini) {
            // Build Gemini format
            json contents = json::array();
            for (const auto& m : req.messages) {
                json part = json::object();
                part["text"] = m.content;
                json content = json::object();
                content["role"] = m.role == "assistant" ? "model" : "user";
                content["parts"] = json::array({part});
                contents.push_back(content);
            }
            payload["contents"] = contents;
            
            json generationConfig = json::object();
            generationConfig["temperature"] = req.temperature;
            generationConfig["maxOutputTokens"] = req.max_tokens;
            payload["generationConfig"] = generationConfig;
            
            // TODO: Tools for Gemini
        } else {
            // Build OpenAI format
            payload["model"] = model_name;
            payload["temperature"] = req.temperature;
            payload["max_tokens"] = req.max_tokens;
            
            json messages = json::array();
            for (const auto& m : req.messages) {
                json msg = json::object();
                msg["role"] = m.role;
                msg["content"] = m.content;
                if (!m.name.empty()) msg["name"] = m.name;
                if (!m.tool_call_id.empty()) msg["tool_call_id"] = m.tool_call_id;
                messages.push_back(msg);
            }
            payload["messages"] = messages;
            
            if (!req.tools.empty()) {
                json tools = json::array();
                for (const auto& t : req.tools) {
                    json tool = json::object();
                    tool["type"] = t.type;
                    json func = json::object();
                    func["name"] = t.function.name;
                    func["description"] = t.function.description;
                    if (!t.function.parameters_schema.empty()) {
                        try {
                            func["parameters"] = json::parse(t.function.parameters_schema);
                        } catch (...) {}
                    }
                    tool["function"] = func;
                    tools.push_back(tool);
                }
                payload["tools"] = tools;
            }
        }
        
        auto res = cli.Post(endpoint.c_str(), headers, payload.dump(), "application/json");
        
        if (!res) {
            output = "{\"error\": \"Connection failed\"}";
            return 503;
        }
        
        output = res->body;
        return res->status;
    } catch (const std::exception& e) {
        std::cerr << "[ModelEngine] Cloud inference error: " << e.what() << std::endl;
        output = std::string("{\"error\": \"") + e.what() + "\"}";
        return 503;
    }
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
    
    json response = json::object();
    json message = json::object();
    message["content"] = result_text;
    json choice = json::object();
    choice["message"] = message;
    response["choices"] = json::array({choice});
    
    output = response.dump();
    
    return 200;
}
