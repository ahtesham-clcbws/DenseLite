#include "ProviderSync.hpp"
#include <iostream>
#include <regex>
#include <cctype>
#include <httplib.h>

void ProviderSync::sync_models_from_provider(sqlite3* db, const std::string& provider, const std::string& api_key) {
    if (!db) return;
    
    std::string host, path;
    bool uses_query_auth = false; // For Gemini
    
    if (provider == "GROQ") {
        host = "https://api.groq.com";
        path = "/openai/v1/models";
    } else if (provider == "OPENROUTER") {
        host = "https://openrouter.ai";
        path = "/api/v1/models";
    } else if (provider == "NOVITA") {
        host = "https://api.novita.ai";
        path = "/v3/openai/models";
    } else if (provider == "MISTRAL") {
        host = "https://api.mistral.ai";
        path = "/v1/models";
    } else if (provider == "COHERE") {
        host = "https://api.cohere.com";
        path = "/v1/models";
    } else if (provider == "GEMINI") {
        host = "https://generativelanguage.googleapis.com";
        path = "/v1beta/models?key=" + api_key;
        uses_query_auth = true;
    } else if (provider == "HUGGINGFACE") {
        std::cout << "[SQLiteRouter] HuggingFace Hub has too many models. Please use the curated SLMs in seed_default_models()." << std::endl;
        return;
    } else {
        std::cout << "[SQLiteRouter] Auto-fetch not yet supported for " << provider << std::endl;
        return;
    }
    
    std::cout << "[SQLiteRouter] Fetching latest models from " << provider << "..." << std::endl;
    
    httplib::Client cli(host);
    httplib::Headers headers;
    if (!uses_query_auth) {
        headers.emplace("Authorization", "Bearer " + api_key);
    }
    
    auto res = cli.Get(path.c_str(), headers);
    if (!res || res->status != 200) {
        std::cerr << "[SQLiteRouter] Failed to fetch models from " << provider << ". Status: " << (res ? res->status : 0) << std::endl;
        return;
    }
    
    std::string body = res->body;
    
    // Choose the right regex pattern depending on the provider's JSON schema
    std::string regex_str = R"REGEX("id"\s*:\s*"([^"]+)")REGEX";
    if (provider == "COHERE" || provider == "GEMINI") {
        regex_str = R"REGEX("name"\s*:\s*"([^"]+)")REGEX";
    }
    
    std::regex id_regex(regex_str);
    auto words_begin = std::sregex_iterator(body.begin(), body.end(), id_regex);
    auto words_end = std::sregex_iterator();
    
    // Begin transaction
    sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, nullptr);
    
    // Clear old models
    std::string del_sql = "DELETE FROM provider_models WHERE provider = ?;";
    sqlite3_stmt* del_stmt;
    if (sqlite3_prepare_v2(db, del_sql.c_str(), -1, &del_stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(del_stmt, 1, provider.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(del_stmt);
        sqlite3_finalize(del_stmt);
    }
    
    std::string ins_sql = "INSERT INTO provider_models (provider, model_name, model_type, priority) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* ins_stmt;
    if (sqlite3_prepare_v2(db, ins_sql.c_str(), -1, &ins_stmt, nullptr) == SQLITE_OK) {
        int priority = 1;
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::string model_name = (*i)[1].str();
            
            // Gemini models have a "models/" prefix which is not used in the OpenAI endpoint
            if (provider == "GEMINI" && model_name.find("models/") == 0) {
                model_name = model_name.substr(7);
            }
            
            // Very simple type matching based on Python script
            std::string model_lower = model_name;
            for(char& c : model_lower) c = std::tolower(c);
            
            std::string m_type = "text";
            if (model_lower.find("vision") != std::string::npos || model_lower.find("vl") != std::string::npos) {
                m_type = "image";
            } else if (model_lower.find("coder") != std::string::npos || model_lower.find("code") != std::string::npos) {
                m_type = "coding";
            }
            
            sqlite3_reset(ins_stmt);
            sqlite3_bind_text(ins_stmt, 1, provider.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins_stmt, 2, model_name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ins_stmt, 3, m_type.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(ins_stmt, 4, priority++);
            sqlite3_step(ins_stmt);
        }
        sqlite3_finalize(ins_stmt);
    }
    
    sqlite3_exec(db, "COMMIT;", 0, 0, nullptr);
    std::cout << "[SQLiteRouter] Synced models for " << provider << std::endl;
}
