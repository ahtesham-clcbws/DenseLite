#include "sqlite_router.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <regex>
#include "httplib.h"

SQLiteRouter::SQLiteRouter(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "[SQLiteRouter] Failed to open DB: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
    } else {
        init_db();
    }
}

SQLiteRouter::~SQLiteRouter() {
    if (db) {
        sqlite3_close(db);
    }
}

void SQLiteRouter::init_db() {
    if (!db) return;
    const char* sql = "CREATE TABLE IF NOT EXISTS api_keys ("
                      "provider TEXT, "
                      "key_index INTEGER, "
                      "cooldown_until INTEGER, "
                      "PRIMARY KEY (provider, key_index));"
                      "CREATE TABLE IF NOT EXISTS provider_models ("
                      "provider TEXT, "
                      "model_name TEXT, "
                      "model_type TEXT, "
                      "priority INTEGER, "
                      "UNIQUE(provider, model_name));";
    
    char* err_msg = nullptr;
    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "[SQLiteRouter] SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }

    seed_default_models();
}

void SQLiteRouter::seed_default_models() {
    if (!db) return;

    // Option 1: Schema Versioning. 
    // We only wipe and update the DB if the C++ version is higher than the DB version.
    const int CURRENT_SEED_VERSION = 2;

    // Check current version in DB
    int db_version = 0;
    const char* init_meta_sql = "CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT);";
    sqlite3_exec(db, init_meta_sql, 0, 0, nullptr);

    const char* check_ver_sql = "SELECT value FROM metadata WHERE key = 'model_seed_version';";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, check_ver_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            db_version = std::stoi(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        sqlite3_finalize(stmt);
    }

    if (db_version >= CURRENT_SEED_VERSION) {
        return; // Schema is up to date, skip I/O!
    }

    std::cout << "[SQLiteRouter] Upgrading C++ model mappings to version " << CURRENT_SEED_VERSION << "..." << std::endl;

    const char* clear_sql = "DELETE FROM provider_models;";
    sqlite3_exec(db, clear_sql, 0, 0, nullptr);

    const char* insert_sql = "INSERT INTO provider_models (provider, model_name, model_type, priority) VALUES "
        "('GEMINI', 'gemini-2.5-flash-image', 'image', 1), "
        "('GEMINI', 'gemini-2.5-flash', 'text', 1), "
        "('GROQ', 'openai/gpt-oss-20b', 'text', 1), "
        "('OPENROUTER', 'liquid/lfm-2.5-2.6b:free', 'text', 1), "
        "('OPENROUTER', 'inclusionai/ling-3.0-flash-fin:free', 'text', 2), "
        "('MISTRAL', 'ministral-8b-2512', 'text', 1), "
        "('MISTRAL', 'open-mistral-nemo', 'text', 2), "
        "('COHERE', 'command-r-08-2024', 'text', 1), "
        "('NOVITA', 'zai-org/glm-5.3-flash', 'text', 1);";

    char* err_msg = nullptr;
    if (sqlite3_exec(db, insert_sql, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "[SQLiteRouter] Seed error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    } else {
        std::string update_ver = "INSERT OR REPLACE INTO metadata (key, value) VALUES ('model_seed_version', '" + std::to_string(CURRENT_SEED_VERSION) + "');";
        sqlite3_exec(db, update_ver.c_str(), 0, 0, nullptr);
    }
}

void SQLiteRouter::sync_db() {
    if (!db) return;
    // Load cooldowns from DB into RAM
    const char* sql = "SELECT provider, key_index, cooldown_until FROM api_keys;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string provider = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            int key_index = sqlite3_column_int(stmt, 1);
            long long cooldown = sqlite3_column_int64(stmt, 2);

            for (auto& k : in_memory_keys) {
                if (k.provider == provider && k.key_index == key_index) {
                    k.cooldown_until = cooldown;
                    break;
                }
            }
        }
        sqlite3_finalize(stmt);
    }
}

void SQLiteRouter::load_env(const std::string& env_path) {
    std::ifstream file(env_path);
    if (!file.is_open()) {
        std::cerr << "[SQLiteRouter] Could not open .env file at " << env_path << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = line.substr(0, eq_pos);
            std::string val = line.substr(eq_pos + 1);
            
            // Strip quotes if present
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                val = val.substr(1, val.size() - 2);
            }

            // Ensure this is an API key with an index, e.g. GROQ_API_KEY_1
            size_t last_underscore = name.rfind('_');
            if (last_underscore != std::string::npos && last_underscore < name.size() - 1) {
                std::string prefix = name.substr(0, last_underscore);
                if (prefix.find("_API_KEY") != std::string::npos) {
                    std::string provider = prefix.substr(0, prefix.find("_API_KEY"));
                    try {
                        int index = std::stoi(name.substr(last_underscore + 1));
                        
                        APIKeyStatus status;
                        status.provider = provider;
                        status.key_index = index;
                        status.env_var_name = name;
                        status.key_value = val;
                        status.cooldown_until = 0;
                        in_memory_keys.push_back(status);
                        
                    } catch (...) {}
                }
            }
        }
    }
    
    // Sync with DB to get existing cooldowns
    sync_db();
}

APIKeyStatus SQLiteRouter::get_next_available_key(const std::string& provider) {
    sync_db(); // Ensure we have latest cooldowns

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    for (const auto& k : in_memory_keys) {
        if (k.provider == provider && k.cooldown_until < now) {
            return k; // Found an available key!
        }
    }

    // All keys are on cooldown
    return APIKeyStatus{"", 0, "", "", 0};
}

void SQLiteRouter::mark_key_cooldown(const std::string& provider, int key_index, int cooldown_seconds) {
    if (!db) return;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    long long cooldown_until = now + cooldown_seconds;

    // Update RAM
    for (auto& k : in_memory_keys) {
        if (k.provider == provider && k.key_index == key_index) {
            k.cooldown_until = cooldown_until;
            break;
        }
    }

    // Update DB
    std::string sql = "INSERT OR REPLACE INTO api_keys (provider, key_index, cooldown_until) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, provider.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, key_index);
        sqlite3_bind_int64(stmt, 3, cooldown_until);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

std::string SQLiteRouter::get_cheapest_model_for_provider(const std::string& provider, const std::string& type) {
    if (!db) return "";
    
    std::string model = "";
    std::string sql = "SELECT model_name FROM provider_models WHERE provider = ? AND model_type = ? ORDER BY priority ASC LIMIT 1;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, provider.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            model = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return model;
}

std::string SQLiteRouter::get_fallback_model(const std::string& provider, const std::string& current_model, const std::string& type) {
    if (!db) return "";
    
    std::string fallback = "";
    // Get the next best model for this provider that is NOT the current model
    std::string sql = "SELECT model_name FROM provider_models WHERE provider = ? AND model_type = ? AND model_name != ? ORDER BY priority ASC LIMIT 1;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, provider.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, current_model.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            fallback = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    
    if (!fallback.empty()) {
        std::cout << "[SQLiteRouter] Found dynamic fallback for " << provider << ": " << fallback << std::endl;
    } else {
        std::cout << "[SQLiteRouter] No fallback models available for " << provider << std::endl;
    }
    
    return fallback;
}

std::string SQLiteRouter::get_provider_for_model(const std::string& model_name) {
    if (!db) return "local";
    std::string provider = "local";
    std::string sql = "SELECT provider FROM provider_models WHERE model_name = ? LIMIT 1;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, model_name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            provider = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return provider;
}

std::string SQLiteRouter::get_provider_url(const std::string& provider) {
    if (provider == "GROQ") return "https://api.groq.com";
    if (provider == "OPENROUTER") return "https://openrouter.ai";
    if (provider == "NOVITA") return "https://api.novita.ai";
    if (provider == "MISTRAL") return "https://api.mistral.ai";
    if (provider == "COHERE") return "https://api.cohere.com";
    if (provider == "GEMINI") return "https://generativelanguage.googleapis.com";
    return "";
}
