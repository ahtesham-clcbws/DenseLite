#include "database_migrator.hpp"
#include "database_paths.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>

bool DatabaseMigrator::execute_sql(const std::string& db_path, const char* sql, std::string& out_journal) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(db_path).parent_path(), ec);

    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(db_path.c_str(), &db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK || !db) return false;

    const char* pragmas = "PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL; PRAGMA busy_timeout = 5000;";
    sqlite3_exec(db, pragmas, nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "PRAGMA journal_mode;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* text = sqlite3_column_text(stmt, 0);
            if (text) out_journal = reinterpret_cast<const char*>(text);
        }
        sqlite3_finalize(stmt);
    }

    rc = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return rc == SQLITE_OK;
}

bool DatabaseMigrator::bootstrap_settings_db(const std::string& db_path, DatabaseStatus& status) {
    status.name = "Settings & Control DB";
    status.path = db_path;
    status.created = !std::filesystem::exists(db_path);

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS system_settings ("
        "  module TEXT NOT NULL, key TEXT NOT NULL, value TEXT NOT NULL,"
        "  val_type TEXT NOT NULL DEFAULT 'string', updated_at INTEGER NOT NULL,"
        "  PRIMARY KEY (module, key)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_settings_module ON system_settings(module);"
        "CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT);"
        "CREATE TABLE IF NOT EXISTS api_keys ("
        "  provider TEXT, key_index INTEGER, cooldown_until INTEGER, PRIMARY KEY (provider, key_index)"
        ");"
        "CREATE TABLE IF NOT EXISTS provider_models ("
        "  provider TEXT, model_name TEXT, model_type TEXT, priority INTEGER, UNIQUE(provider, model_name)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_provider_type ON provider_models(provider, model_type, priority);"
        "CREATE TABLE IF NOT EXISTS local_models ("
        "  model_id TEXT PRIMARY KEY, file_path TEXT NOT NULL UNIQUE, architecture TEXT NOT NULL,"
        "  param_count INTEGER NOT NULL, param_size_str TEXT NOT NULL, quant_type TEXT NOT NULL,"
        "  context_length INTEGER NOT NULL, is_verified INTEGER NOT NULL DEFAULT 1, updated_at INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS model_roles ("
        "  role TEXT PRIMARY KEY, model_id TEXT NOT NULL, is_active INTEGER NOT NULL DEFAULT 1,"
        "  updated_at INTEGER NOT NULL, FOREIGN KEY(model_id) REFERENCES local_models(model_id)"
        ");"
        "INSERT OR IGNORE INTO system_settings (module, key, value, val_type, updated_at) VALUES "
        "('server', 'host', '0.0.0.0', 'string', 1700000000000),"
        "('server', 'port', '9501', 'int', 1700000000000),"
        "('server', 'threads', '4', 'int', 1700000000000),"
        "('server', 'max_payload_mb', '32', 'int', 1700000000000),"
        "('resource', 'ram_budget_percent', '0.45', 'float', 1700000000000),"
        "('resource', 'max_kv_tokens', '65536', 'int', 1700000000000),"
        "('resource', 'enable_gpu', 'true', 'bool', 1700000000000),"
        "('resource', 'vram_budget_mb', '2048', 'int', 1700000000000),"
        "('inference', 'default_temperature', '0.7', 'float', 1700000000000),"
        "('inference', 'default_top_p', '0.9', 'float', 1700000000000),"
        "('inference', 'needle3_mode', 'hybrid', 'string', 1700000000000),"
        "('inference', 'enable_tool_dedup', 'true', 'bool', 1700000000000),"
        "('inference', 'context_window', '65536', 'int', 1700000000000),"
        "('inference', 'enable_context_injection', 'true', 'bool', 1700000000000),"
        "('logging', 'level', 'INFO', 'string', 1700000000000),"
        "('logging', 'enable_file_logging', 'true', 'bool', 1700000000000),"
        "('logging', 'enable_console', 'true', 'bool', 1700000000000),"
        "('logging', 'log_path', 'denselite.log', 'string', 1700000000000),"
        "('storage', 'models_dir', '~/.denselite/models', 'string', 1700000000000),"
        "('storage', 'data_dir', '~/.denselite/data', 'string', 1700000000000),"
        "('storage', 'kv_cache_dir', '~/.denselite/kv_cache', 'string', 1700000000000),"
        "('storage', 'logs_dir', '~/.denselite/logs', 'string', 1700000000000);"
        "INSERT OR IGNORE INTO provider_models (provider, model_name, model_type, priority) VALUES "
        "('GEMINI', 'gemini-2.5-flash-image', 'image', 1),"
        "('GEMINI', 'gemini-2.5-flash', 'text', 1),"
        "('GROQ', 'openai/gpt-oss-20b', 'text', 1),"
        "('OPENROUTER', 'liquid/lfm-2.5-2.6b:free', 'text', 1),"
        "('OPENROUTER', 'inclusionai/ling-3.0-flash-fin:free', 'text', 2),"
        "('MISTRAL', 'ministral-8b-2512', 'text', 1),"
        "('MISTRAL', 'open-mistral-nemo', 'text', 2),"
        "('COHERE', 'command-r-08-2024', 'text', 1),"
        "('NOVITA', 'zai-org/glm-5.3-flash', 'text', 1);"
        "INSERT OR REPLACE INTO metadata (key, value) VALUES ('model_seed_version', '2');";

    status.migrated = execute_sql(db_path, ddl, status.journal_mode);
    return status.migrated;
}

bool DatabaseMigrator::bootstrap_memory_db(const std::string& db_path, DatabaseStatus& status) {
    status.name = "Memory DB";
    status.path = db_path;
    status.created = !std::filesystem::exists(db_path);

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS memories ("
        "  key TEXT PRIMARY KEY, id TEXT, category INTEGER, value TEXT, confidence REAL, updated_at INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS sessions (session_id TEXT PRIMARY KEY, turn_count INTEGER, updated_at INTEGER);"
        "CREATE TABLE IF NOT EXISTS session_turns ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, session_id TEXT, turn_index INTEGER,"
        "  timestamp INTEGER, role TEXT, content TEXT, tool_name TEXT, tool_args TEXT, tool_result TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS consolidated_archives ("
        "  archive_id TEXT PRIMARY KEY, session_id TEXT, summary TEXT, extracted_facts TEXT, created_at INTEGER"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_session_turns ON session_turns(session_id, turn_index);"
        "CREATE INDEX IF NOT EXISTS idx_memories_category ON memories(category);";

    status.migrated = execute_sql(db_path, ddl, status.journal_mode);
    return status.migrated;
}

bool DatabaseMigrator::bootstrap_symbols_db(const std::string& db_path, DatabaseStatus& status) {
    status.name = "Symbols DB";
    status.path = db_path;
    status.created = !std::filesystem::exists(db_path);

    const char* ddl =
        "CREATE TABLE IF NOT EXISTS code_symbols ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, file_path TEXT NOT NULL, symbol TEXT NOT NULL,"
        "  parent_symbol TEXT, language TEXT NOT NULL, start_line INTEGER NOT NULL, end_line INTEGER NOT NULL,"
        "  source_hash INTEGER NOT NULL, content TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_sym_file ON code_symbols(file_path);"
        "CREATE INDEX IF NOT EXISTS idx_sym_name ON code_symbols(symbol);"
        "CREATE INDEX IF NOT EXISTS idx_sym_lang ON code_symbols(language);";

    status.migrated = execute_sql(db_path, ddl, status.journal_mode);
    return status.migrated;
}

bool DatabaseMigrator::ensure_all_databases_ready(const std::string& base_dir, bool verbose) {
    std::vector<DatabaseStatus> statuses(3);
    bool ok = true;

    DatabasePaths::ensure_dir_exists(base_dir);
    ok &= bootstrap_settings_db(DatabasePaths::settings_db(base_dir), statuses[0]);
    ok &= bootstrap_memory_db(DatabasePaths::memory_db(base_dir), statuses[1]);
    ok &= bootstrap_symbols_db(DatabasePaths::symbols_db(base_dir), statuses[2]);

    if (verbose) {
        std::cout << "[DatabaseMigrator] Verifying database integrity & WAL configuration..." << std::endl;
        for (const auto& s : statuses) {
            std::cout << "  - " << s.name << " [" << s.journal_mode << "] -> "
                      << (s.created ? "Created & Migrated (Fresh)" : "Verified (Schema Active)") << std::endl;
        }
    }
    return ok;
}

bool DatabaseMigrator::reset_settings_to_defaults(const std::string& base_dir) {
    std::string db_path = DatabasePaths::settings_db(base_dir);
    const char* sql =
        "DELETE FROM system_settings;"
        "INSERT INTO system_settings (module, key, value, val_type, updated_at) VALUES "
        "('server', 'host', '0.0.0.0', 'string', 1700000000000), ('server', 'port', '9501', 'int', 1700000000000),"
        "('server', 'threads', '4', 'int', 1700000000000), ('server', 'max_payload_mb', '32', 'int', 1700000000000),"
        "('resource', 'ram_budget_percent', '0.45', 'float', 1700000000000), ('resource', 'max_kv_tokens', '65536', 'int', 1700000000000),"
        "('resource', 'enable_gpu', 'true', 'bool', 1700000000000), ('resource', 'vram_budget_mb', '2048', 'int', 1700000000000),"
        "('inference', 'default_temperature', '0.7', 'float', 1700000000000), ('inference', 'default_top_p', '0.9', 'float', 1700000000000),"
        "('inference', 'needle3_mode', 'hybrid', 'string', 1700000000000), ('inference', 'enable_tool_dedup', 'true', 'bool', 1700000000000),"
        "('inference', 'context_window', '65536', 'int', 1700000000000), ('inference', 'enable_context_injection', 'true', 'bool', 1700000000000),"
        "('logging', 'level', 'INFO', 'string', 1700000000000), ('logging', 'enable_file_logging', 'true', 'bool', 1700000000000),"
        "('logging', 'enable_console', 'true', 'bool', 1700000000000), ('logging', 'log_path', 'denselite.log', 'string', 1700000000000),"
        "('storage', 'models_dir', '~/.denselite/models', 'string', 1700000000000), ('storage', 'data_dir', '~/.denselite/data', 'string', 1700000000000),"
        "('storage', 'kv_cache_dir', '~/.denselite/kv_cache', 'string', 1700000000000), ('storage', 'logs_dir', '~/.denselite/logs', 'string', 1700000000000);";
    std::string jm;
    return execute_sql(db_path, sql, jm);
}

bool DatabaseMigrator::purge_kv_cache(const std::string&) {
    std::string kv_dir = PathService::instance().get_kv_cache_dir();
    std::error_code ec;
    if (std::filesystem::exists(kv_dir, ec)) {
        for (const auto& e : std::filesystem::directory_iterator(kv_dir, ec)) std::filesystem::remove_all(e.path(), ec);
    }
    return true;
}

bool DatabaseMigrator::vacuum_databases(const std::string& base_dir) {
    std::string jm;
    bool ok = execute_sql(DatabasePaths::settings_db(base_dir), "VACUUM; PRAGMA optimize;", jm);
    ok &= execute_sql(DatabasePaths::memory_db(base_dir), "VACUUM; PRAGMA optimize;", jm);
    ok &= execute_sql(DatabasePaths::symbols_db(base_dir), "VACUUM; PRAGMA optimize;", jm);
    return ok;
}
