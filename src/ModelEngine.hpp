#pragma once
#include <string>
#include <map>
#include "model.hpp"
#include "sqlite_router.hpp"

// Abstraction for Model Execution (Cloud vs Local)
class ModelEngine {
public:
    ModelEngine(std::map<std::string, DenseModel>& resident_models, SQLiteRouter& router);

    // Perform inference. Automatically routes to CloudAdapter or LocalInference.
    // Returns HTTP status code (e.g., 200, 404, 429).
    // The 'output' string will contain the raw text/json response.
    int infer(const std::string& model_name, const OpenAIRequest& req, const std::string& compiled_prompt, std::string& output);

private:
    std::map<std::string, DenseModel>& local_models;
    SQLiteRouter& sqlite_router;

    int infer_cloud(const std::string& model_name, const std::string& provider_url, const std::string& api_key, const OpenAIRequest& req, std::string& output);
    int infer_local(const std::string& model_name, const std::string& prompt, std::string& output);
};
