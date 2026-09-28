#include "sqlite_router.hpp"
#include "settings/settings_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdlib>

SQLiteRouter::SQLiteRouter(const std::string& db_path) {
    if (sqlite3_open_v2(db_path.c_str(), &db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
        std::cerr << "[SQLiteRouter] Failed to open DB: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
    } else {
        const char* pragmas = "PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL; PRAGMA busy_timeout = 5000;";
        sqlite3_exec(db, pragmas, nullptr, nullptr, nullptr);
        init_db();
    }
}

SQLiteRouter::~SQLiteRouter() {
    if (db) sqlite3_close(db);
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

void SQLiteRouter::sync_db() {
    if (!db) return;
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
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        std::string name = line.substr(0, eq_pos);
        std::string val = line.substr(eq_pos + 1);
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }
        size_t last_underscore = name.rfind('_');
        if (last_underscore != std::string::npos && last_underscore < name.size() - 1) {
            std::string prefix = name.substr(0, last_underscore);
            if (prefix.find("_API_KEY") != std::string::npos) {
                std::string provider = prefix.substr(0, prefix.find("_API_KEY"));
                try {
                    int index = std::stoi(name.substr(last_underscore + 1));
                    in_memory_keys.push_back({provider, index, name, val, 0});
                } catch (...) {}
            }
        } else if (name.find("_API_KEY") != std::string::npos) {
            std::string provider = name.substr(0, name.find("_API_KEY"));
            in_memory_keys.push_back({provider, 1, name, val, 0});
        }
    }
    sync_db();
}

APIKeyStatus SQLiteRouter::get_next_available_key(const std::string& provider) {
    // 1. Check database settings first
    auto cloud_cfg = SettingsManager::instance().get_cloud_config();
    std::string db_key = "";
    if (provider == "OPENROUTER") db_key = cloud_cfg.openrouter_api_key;
    else if (provider == "GEMINI") db_key = cloud_cfg.gemini_api_key;
    else if (provider == "OPENAI") db_key = cloud_cfg.openai_api_key;

    if (!db_key.empty()) {
        return APIKeyStatus{provider, 0, "DB_SETTINGS", db_key, 0};
    }

    // 2. Fallback to .env in-memory keys
    sync_db();
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (const auto& k : in_memory_keys) {
        if (k.provider == provider && k.cooldown_until < now) {
            return k;
        }
    }

    // 3. Fallback to system environment variable
    std::string env_name = provider + "_API_KEY";
    const char* env_val = std::getenv(env_name.c_str());
    if (env_val && std::string(env_val).size() > 0) {
        return APIKeyStatus{provider, 0, env_name, std::string(env_val), 0};
    }
    if (provider == "GEMINI") {
        const char* g_val = std::getenv("GOOGLE_API_KEY");
        if (g_val && std::string(g_val).size() > 0) {
            return APIKeyStatus{provider, 0, "GOOGLE_API_KEY", std::string(g_val), 0};
        }
    }

    return APIKeyStatus{"", 0, "", "", 0};
}

void SQLiteRouter::mark_key_cooldown(const std::string& provider, int key_index, int cooldown_seconds) {
    if (!db) return;
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    long long cooldown_until = now + cooldown_seconds;

    for (auto& k : in_memory_keys) {
        if (k.provider == provider && k.key_index == key_index) {
            k.cooldown_until = cooldown_until;
            break;
        }
    }

    const char* sql = "INSERT OR REPLACE INTO api_keys (provider, key_index, cooldown_until) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, provider.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, key_index);
        sqlite3_bind_int64(stmt, 3, cooldown_until);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}
