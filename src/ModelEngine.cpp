#include "ModelEngine.hpp"
#include "httplib.h"
#include "infer.hpp" // For AVX2 inference
#include "SessionKVCache.hpp"
#include "resource_governor.hpp"
#include "ModelLoader.hpp"
#include "path_service.hpp"
#include "settings/model_registry_db.hpp"
#include <filesystem>
#include <iostream>

static std::mutex g_default_models_mutex;

ModelEngine::ModelEngine(std::map<std::string, DenseModel>& resident_models, SQLiteRouter& router, std::mutex* models_mutex)
    : local_models(resident_models), sqlite_router(router), models_mutex_(models_mutex) {}

#include "../dependencies/json.hpp"

using json = nlohmann::json;

#include "settings/settings_manager.hpp"

int ModelEngine::infer(const std::string& model_name, const OpenAIRequest& req, const std::string& compiled_prompt, std::string& output) {
    // 1. Ask SQLiteRouter if this is a Cloud model or Local model
    std::string provider = sqlite_router.get_provider_for_model(model_name);

    if (provider == "local" || provider.empty()) {
        return infer_local(model_name, compiled_prompt, output, req);
    } else {
        APIKeyStatus key_status = sqlite_router.get_next_available_key(provider);
        std::string api_key = key_status.key_value;
        if (api_key.empty()) {
            output = "{\"error\": \"No active API key configured or available for provider: " + provider + "\"}";
            return 429;
        }
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
            
            auto inf_cfg = SettingsManager::instance().get_inference_config();
            float eff_temp = (req.temperature > 0.0f) ? req.temperature : inf_cfg.default_temperature;
            int eff_max = (req.max_tokens > 0) ? req.max_tokens : inf_cfg.max_output_tokens;
            json generationConfig = json::object();
            generationConfig["temperature"] = eff_temp;
            generationConfig["maxOutputTokens"] = eff_max;
            payload["generationConfig"] = generationConfig;
            
            // TODO: Tools for Gemini
        } else {
            // Build OpenAI format
            auto inf_cfg = SettingsManager::instance().get_inference_config();
            float eff_temp = (req.temperature > 0.0f) ? req.temperature : inf_cfg.default_temperature;
            int eff_max = (req.max_tokens > 0) ? req.max_tokens : inf_cfg.max_output_tokens;
            payload["model"] = model_name;
            payload["temperature"] = eff_temp;
            payload["max_tokens"] = eff_max;
            
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
            std::cout << "[ModelEngine] Connection failed with error: " << httplib::to_string(res.error()) << std::endl;
            output = "{\"error\": \"Connection failed\"}";
            return 503;
        }
        std::cout << "[ModelEngine] Cloud response status: " << res->status << std::endl;
        output = res->body;
        return res->status;
    } catch (const std::exception& e) {
        std::cerr << "[ModelEngine] Cloud inference error: " << e.what() << std::endl;
        output = std::string("{\"error\": \"") + e.what() + "\"}";
        return 503;
    }
}

#include "Formatter.hpp"

int ModelEngine::infer_local(const std::string& model_name, const std::string& prompt, std::string& output, const OpenAIRequest& req) {
    std::cout << "[ModelEngine] Routing inference to Local AVX2 engine for model: " << model_name << "\n";
    
    DenseModel* target_model_ptr = nullptr;
    {
        std::unique_lock<std::mutex> lock(models_mutex_ ? *models_mutex_ : g_default_models_mutex);
        auto it = local_models.find(model_name);
        if (it == local_models.end()) {
            std::string db_path = PathService::instance().settings_db();
            LocalModelRecord rec;
            std::string model_file;
            if (ModelRegistryDB::get_model(db_path, model_name, rec) && std::filesystem::exists(rec.file_path)) {
                model_file = rec.file_path;
            } else {
                std::string candidate = PathService::instance().get_models_dir() + "/" + model_name;
                if (std::filesystem::exists(candidate)) {
                    model_file = candidate;
                } else if (std::filesystem::exists(candidate + ".gguf")) {
                    model_file = candidate + ".gguf";
                }
            }

            if (!model_file.empty()) {
                std::cout << "[ModelEngine] Lazy loading local model on demand: " << model_file << std::endl;
                DenseModel loaded;
                std::string err;
                if (ModelLoader::load_model(model_file, loaded, err)) {
                    local_models[model_name] = std::move(loaded);
                    it = local_models.find(model_name);
                } else {
                    std::cerr << "[ModelEngine] Failed lazy loading " << model_file << ": " << err << std::endl;
                }
            }
        }

        if (it != local_models.end()) {
            target_model_ptr = &it->second;
        }
    }

    if (!target_model_ptr) {
        output = "Error: Local model '" + model_name + "' not loaded in RAM and not found on disk.";
        return 500;
    }

    DenseModel& active_model = *target_model_ptr;
    std::vector<int> tokens = tokenize(active_model.vocab, prompt);
    
    std::string result_text;
    auto stream_cb = [&](const std::string& text) {
        result_text += text;
    };
    
    auto session_kv = SessionKVCacheManager::instance().get_or_create(req.session_id, &active_model.config);
    auto inf_cfg = SettingsManager::instance().get_inference_config();
    float eff_temp = (req.temperature > 0.0f) ? req.temperature : inf_cfg.default_temperature;
    int eff_max = (req.max_tokens > 0) ? req.max_tokens : inf_cfg.max_output_tokens;
    size_t dynamic_budget = ResourceGovernor::calculate_dynamic_context_tokens();
    if (inf_cfg.context_window > 0 && dynamic_budget > static_cast<size_t>(inf_cfg.context_window)) {
        dynamic_budget = static_cast<size_t>(inf_cfg.context_window);
    }
    float rep_pen = (inf_cfg.repeat_penalty > 0.0f) ? inf_cfg.repeat_penalty : 1.15f;
    generate(active_model, tokens, stream_cb, eff_max, eff_temp, rep_pen, session_kv.get(), static_cast<int>(dynamic_budget));

    if (!req.session_id.empty()) {
        SessionKVCacheManager::instance().save_to_disk(req.session_id);
    }
    
    json response = json::object();
    json message = json::object();
    message["content"] = result_text;
    json choice = json::object();
    choice["message"] = message;
    response["choices"] = json::array({choice});
    
    output = response.dump();
    
    return 200;
}
