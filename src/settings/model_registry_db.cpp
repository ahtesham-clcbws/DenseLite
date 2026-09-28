#include "model_registry_db.hpp"
#include <chrono>

static sqlite3* open_db(const std::string& db_path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) { if (db) sqlite3_close(db); return nullptr; }
    sqlite3_busy_timeout(db, 5000);
    return db;
}

static int64_t current_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static LocalModelRecord parse_model_row(sqlite3_stmt* stmt) {
    LocalModelRecord r;
    r.model_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    r.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    const char* arch = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    r.architecture = arch ? arch : "";
    r.param_count = static_cast<uint64_t>(sqlite3_column_int64(stmt, 3));
    const char* ps = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    r.param_size_str = ps ? ps : "";
    const char* qt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    r.quant_type = qt ? qt : "";
    r.context_length = static_cast<uint32_t>(sqlite3_column_int(stmt, 6));
    r.is_verified = (sqlite3_column_int(stmt, 7) != 0);
    r.updated_at = sqlite3_column_int64(stmt, 8);
    return r;
}

static bool execute_single_param(const std::string& db_path, const char* sql, const std::string& param) {
    sqlite3* db = open_db(db_path);
    if (!db) return false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_text(stmt, 1, param.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    bool changed = (sqlite3_changes(db) > 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok && changed;
}

bool ModelRegistryDB::register_model(const std::string& db_path, const LocalModelRecord& rec) {
    sqlite3* db = open_db(db_path);
    if (!db) return false;
    const char* sql = "INSERT OR REPLACE INTO local_models "
                      "(model_id, file_path, architecture, param_count, param_size_str, quant_type, context_length, is_verified, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }
    int64_t ts = rec.updated_at > 0 ? rec.updated_at : current_timestamp_ms();
    sqlite3_bind_text(stmt, 1, rec.model_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rec.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, rec.architecture.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(rec.param_count));
    sqlite3_bind_text(stmt, 5, rec.param_size_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, rec.quant_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, static_cast<int>(rec.context_length));
    sqlite3_bind_int(stmt, 8, rec.is_verified ? 1 : 0);
    sqlite3_bind_int64(stmt, 9, ts);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

bool ModelRegistryDB::get_model(const std::string& db_path, const std::string& model_id, LocalModelRecord& out) {
    sqlite3* db = open_db(db_path);
    if (!db) return false;
    const char* sql = "SELECT model_id, file_path, architecture, param_count, param_size_str, quant_type, context_length, is_verified, updated_at "
                      "FROM local_models WHERE model_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return false; }
    sqlite3_bind_text(stmt, 1, model_id.c_str(), -1, SQLITE_TRANSIENT);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) { out = parse_model_row(stmt); found = true; }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return found;
}

std::vector<LocalModelRecord> ModelRegistryDB::get_all_models(const std::string& db_path) {
    std::vector<LocalModelRecord> list;
    sqlite3* db = open_db(db_path);
    if (!db) return list;
    const char* sql = "SELECT model_id, file_path, architecture, param_count, param_size_str, quant_type, context_length, is_verified, updated_at "
                      "FROM local_models ORDER BY model_id ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return list; }
    while (sqlite3_step(stmt) == SQLITE_ROW) list.push_back(parse_model_row(stmt));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return list;
}

bool ModelRegistryDB::delete_model(const std::string& db_path, const std::string& model_id) {
    return execute_single_param(db_path, "DELETE FROM local_models WHERE model_id = ?;", model_id);
}

bool ModelRegistryDB::bind_role(const std::string& db_path, const std::string& role, const std::string& model_id, bool is_active) {
    sqlite3* db = open_db(db_path);
    if (!db) return false;
    const char* sql = "INSERT OR REPLACE INTO model_roles (role, model_id, is_active, updated_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return false; }
    sqlite3_bind_text(stmt, 1, role.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, model_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, is_active ? 1 : 0);
    sqlite3_bind_int64(stmt, 4, current_timestamp_ms());
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

bool ModelRegistryDB::set_role_active(const std::string& db_path, const std::string& role, bool is_active) {
    sqlite3* db = open_db(db_path);
    if (!db) return false;
    const char* sql = "UPDATE model_roles SET is_active = ?, updated_at = ? WHERE role = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return false; }
    sqlite3_bind_int(stmt, 1, is_active ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, current_timestamp_ms());
    sqlite3_bind_text(stmt, 3, role.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    bool changed = (sqlite3_changes(db) > 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok && changed;
}

bool ModelRegistryDB::unbind_role(const std::string& db_path, const std::string& role) {
    return execute_single_param(db_path, "DELETE FROM model_roles WHERE role = ?;", role);
}

std::vector<ModelRoleBinding> ModelRegistryDB::get_all_role_bindings(const std::string& db_path, bool active_only) {
    std::vector<ModelRoleBinding> list;
    sqlite3* db = open_db(db_path);
    if (!db) return list;
    const char* sql = active_only ? "SELECT role, model_id, is_active, updated_at FROM model_roles WHERE is_active = 1;"
                                  : "SELECT role, model_id, is_active, updated_at FROM model_roles;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return list; }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ModelRoleBinding b;
        b.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        b.model_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        b.is_active = (sqlite3_column_int(stmt, 2) != 0);
        b.updated_at = sqlite3_column_int64(stmt, 3);
        list.push_back(b);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return list;
}

std::string ModelRegistryDB::get_model_for_role(const std::string& db_path, const std::string& role) {
    sqlite3* db = open_db(db_path);
    if (!db) return "";
    const char* sql = "SELECT model_id FROM model_roles WHERE role = ? AND is_active = 1 LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) { sqlite3_close(db); return ""; }
    sqlite3_bind_text(stmt, 1, role.c_str(), -1, SQLITE_TRANSIENT);
    std::string res;
    if (sqlite3_step(stmt) == SQLITE_ROW) res = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return res;
}
