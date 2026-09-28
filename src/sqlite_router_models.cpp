#include "sqlite_router.hpp"
#include <iostream>
#include <sqlite3.h>

void SQLiteRouter::seed_default_models() {
    if (!db) return;
    const int CURRENT_SEED_VERSION = 2;
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

    if (db_version >= CURRENT_SEED_VERSION) return;

    std::cout << "[SQLiteRouter] Upgrading C++ model mappings to v" << CURRENT_SEED_VERSION << std::endl;
    sqlite3_exec(db, "DELETE FROM provider_models;", 0, 0, nullptr);

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

std::string SQLiteRouter::get_cheapest_model_for_provider(const std::string& provider, const std::string& type) {
    if (!db) return "";
    std::string model = "";
    const char* sql = "SELECT model_name FROM provider_models WHERE provider = ? AND model_type = ? ORDER BY priority ASC LIMIT 1;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
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
    const char* sql = "SELECT model_name FROM provider_models WHERE provider = ? AND model_type = ? AND model_name != ? ORDER BY priority ASC LIMIT 1;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, provider.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, current_model.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            fallback = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return fallback;
}

std::string SQLiteRouter::get_provider_for_model(const std::string& model_name) {
    if (!db) return "local";
    std::string provider = "local";
    const char* sql = "SELECT provider FROM provider_models WHERE model_name = ? LIMIT 1;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
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
    if (provider == "OPENAI") return "https://api.openai.com";
    return "";
}
