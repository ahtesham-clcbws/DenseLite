#pragma once

#include <string>
#include <vector>
#include <map>
#include <sqlite3.h>

struct APIKeyStatus {
    std::string provider;
    int key_index;
    std::string env_var_name; // e.g., "GROQ_API_KEY_1"
    std::string key_value;    // The actual key (loaded from RAM, NOT saved in DB)
    long long cooldown_until; // Unix timestamp
};

class SQLiteRouter {
private:
    sqlite3* db;
    std::vector<APIKeyStatus> in_memory_keys;

    void init_db();
    void sync_db();
    void seed_default_models(); // C++ Native seeding

public:
    SQLiteRouter(const std::string& db_path);
    ~SQLiteRouter();

    // Parse the .env file and load keys into RAM
    void load_env(const std::string& env_path);

    // Get the next available key for a given provider (round-robin style)
    // Returns empty APIKeyStatus if all keys are on cooldown
    APIKeyStatus get_next_available_key(const std::string& provider);

    // Mark a key as dead/on cooldown for a certain number of seconds (e.g., 429 Too Many Requests)
    void mark_key_cooldown(const std::string& provider, int key_index, int cooldown_seconds = 300);

    // Dynamically fetch the cheapest available model from the SQLite database
    std::string get_cheapest_model_for_provider(const std::string& provider, const std::string& type = "text");

    // Dynamic fallback lookup: Gets the next best model for this provider
    std::string get_fallback_model(const std::string& provider, const std::string& current_model, const std::string& type = "text");


    // Resolves provider for a given model
    std::string get_provider_for_model(const std::string& model_name);

    // Resolves base URL for a given provider
    std::string get_provider_url(const std::string& provider);
};
