#include "settings_db.hpp"
#include <iostream>

SettingsDB::SettingsDB() = default;
SettingsDB::~SettingsDB() { close(); }

bool SettingsDB::init(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    close();
    db_path_ = db_path;

    int rc = sqlite3_open_v2(db_path_.c_str(), &db_,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    if (rc != SQLITE_OK) {
        db_ = nullptr;
        return false;
    }
    return configure_pragmas() && create_tables();
}

void SettingsDB::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SettingsDB::is_open() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return db_ != nullptr;
}

bool SettingsDB::configure_pragmas() {
    if (!db_) return false;
    const char* pragmas =
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = NORMAL;"
        "PRAGMA temp_store = MEMORY;"
        "PRAGMA busy_timeout = 5000;";
    return sqlite3_exec(db_, pragmas, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SettingsDB::create_tables() {
    if (!db_) return false;
    const char* schema =
        "CREATE TABLE IF NOT EXISTS system_settings ("
        "  module TEXT NOT NULL, key TEXT NOT NULL, value TEXT NOT NULL,"
        "  val_type TEXT NOT NULL DEFAULT 'string', updated_at INTEGER NOT NULL,"
        "  PRIMARY KEY (module, key)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_settings_module ON system_settings(module);";
    return sqlite3_exec(db_, schema, nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::string SettingsDB::get_journal_mode() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return "";
    sqlite3_stmt* stmt = nullptr;
    std::string mode = "";
    if (sqlite3_prepare_v2(db_, "PRAGMA journal_mode;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* text = sqlite3_column_text(stmt, 0);
            if (text) mode = reinterpret_cast<const char*>(text);
        }
        sqlite3_finalize(stmt);
    }
    return mode;
}

bool SettingsDB::upsert(const SettingRecord& rec) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql =
        "INSERT INTO system_settings (module, key, value, val_type, updated_at) VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(module, key) DO UPDATE SET value=excluded.value, val_type=excluded.val_type, updated_at=excluded.updated_at;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, rec.module.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rec.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, rec.value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, rec.val_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, rec.updated_at);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool SettingsDB::get(const std::string& module, const std::string& key, SettingRecord& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "SELECT module, key, value, val_type, updated_at FROM system_settings WHERE module = ? AND key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, module.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.module = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        out.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        out.val_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        out.updated_at = sqlite3_column_int64(stmt, 4);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool SettingsDB::get_module(const std::string& module, std::vector<SettingRecord>& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "SELECT module, key, value, val_type, updated_at FROM system_settings WHERE module = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, module.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SettingRecord rec;
        rec.module = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.val_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.updated_at = sqlite3_column_int64(stmt, 4);
        out.push_back(rec);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SettingsDB::get_all(std::vector<SettingRecord>& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "SELECT module, key, value, val_type, updated_at FROM system_settings;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SettingRecord rec;
        rec.module = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.val_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.updated_at = sqlite3_column_int64(stmt, 4);
        out.push_back(rec);
    }
    sqlite3_finalize(stmt);
    return true;
}

int SettingsDB::count() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return 0;
    int cnt = 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM system_settings;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) cnt = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return cnt;
}

int SettingsDB::get_data_version() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return 0;
    int v = 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA data_version;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) v = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return v;
}
