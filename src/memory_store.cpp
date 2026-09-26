#include "memory_store.hpp"
#include <iostream>
#include <chrono>

MemoryStore::MemoryStore() = default;

MemoryStore::~MemoryStore() {
    close();
}

bool MemoryStore::init(const std::string& sqlite_path, const std::string& zvec_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    close();
    sqlite_path_ = sqlite_path;
    zvec_path_ = zvec_path;

    if (sqlite3_open_v2(sqlite_path_.c_str(), &db_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        db_ = nullptr;
        return false;
    }
    return create_tables();
}

void MemoryStore::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool MemoryStore::create_tables() {
    if (!db_) return false;
    const char* schema =
        "CREATE TABLE IF NOT EXISTS memories ("
        "  key TEXT PRIMARY KEY,"
        "  id TEXT,"
        "  category INTEGER,"
        "  value TEXT,"
        "  confidence REAL,"
        "  updated_at INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  session_id TEXT PRIMARY KEY,"
        "  turn_count INTEGER,"
        "  updated_at INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS session_turns ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  session_id TEXT,"
        "  turn_index INTEGER,"
        "  timestamp INTEGER,"
        "  role TEXT,"
        "  content TEXT,"
        "  tool_name TEXT,"
        "  tool_args TEXT,"
        "  tool_result TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS consolidated_archives ("
        "  archive_id TEXT PRIMARY KEY,"
        "  session_id TEXT,"
        "  summary TEXT,"
        "  extracted_facts TEXT,"
        "  created_at INTEGER"
        ");";
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, schema, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool MemoryStore::put_memory(const MemoryEntry& entry, const std::vector<float>& /*embedding*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "INSERT OR REPLACE INTO memories (key, id, category, value, confidence, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, entry.key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, entry.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, static_cast<int>(entry.category));
    sqlite3_bind_text(stmt, 4, entry.value.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 5, entry.confidence);
    sqlite3_bind_int64(stmt, 6, entry.updated_at);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool MemoryStore::get_memory(const std::string& key, MemoryEntry& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "SELECT key, id, category, value, confidence, updated_at FROM memories WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        out.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.category = static_cast<MemoryCategory>(sqlite3_column_int(stmt, 2));
        out.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        out.confidence = static_cast<float>(sqlite3_column_double(stmt, 4));
        out.updated_at = sqlite3_column_int64(stmt, 5);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool MemoryStore::delete_memory(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "DELETE FROM memories WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<MemoryEntry> MemoryStore::query_memories_keyword(const std::string& query, size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryEntry> results;
    if (!db_) return results;

    const char* sql = "SELECT key, id, category, value, confidence, updated_at FROM memories "
                      "WHERE key LIKE ? OR value LIKE ? LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    std::string pattern = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(limit));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MemoryEntry entry;
        entry.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        entry.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        entry.category = static_cast<MemoryCategory>(sqlite3_column_int(stmt, 2));
        entry.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        entry.confidence = static_cast<float>(sqlite3_column_double(stmt, 4));
        entry.updated_at = sqlite3_column_int64(stmt, 5);
        results.push_back(std::move(entry));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<MemoryEntry> MemoryStore::load_all_persistent() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryEntry> results;
    if (!db_) return results;

    const char* sql = "SELECT key, id, category, value, confidence, updated_at FROM memories;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MemoryEntry entry;
        entry.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        entry.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        entry.category = static_cast<MemoryCategory>(sqlite3_column_int(stmt, 2));
        entry.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        entry.confidence = static_cast<float>(sqlite3_column_double(stmt, 4));
        entry.updated_at = sqlite3_column_int64(stmt, 5);
        results.push_back(std::move(entry));
    }
    sqlite3_finalize(stmt);
    return results;
}

bool MemoryStore::save_session(const SessionMemory& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    std::string sid = session.get_session_id();
    auto turns = session.get_turns();
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql_sess = "INSERT OR REPLACE INTO sessions (session_id, turn_count, updated_at) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt_s = nullptr;
    if (sqlite3_prepare_v2(db_, sql_sess, -1, &stmt_s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt_s, 1, sid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt_s, 2, static_cast<int>(turns.size()));
    sqlite3_bind_int64(stmt_s, 3, now);
    sqlite3_step(stmt_s);
    sqlite3_finalize(stmt_s);

    // Delete existing turns to cleanly rewrite session
    const char* del_sql = "DELETE FROM session_turns WHERE session_id = ?;";
    sqlite3_stmt* del_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, del_sql, -1, &del_stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(del_stmt, 1, sid.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(del_stmt);
        sqlite3_finalize(del_stmt);
    }

    const char* turn_sql = "INSERT INTO session_turns (session_id, turn_index, timestamp, role, content, tool_name, tool_args, tool_result) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* t_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, turn_sql, -1, &t_stmt, nullptr) != SQLITE_OK) return false;

    for (const auto& t : turns) {
        sqlite3_reset(t_stmt);
        sqlite3_bind_text(t_stmt, 1, sid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(t_stmt, 2, t.turn_index);
        sqlite3_bind_int64(t_stmt, 3, t.timestamp);
        sqlite3_bind_text(t_stmt, 4, t.role.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(t_stmt, 5, t.content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(t_stmt, 6, t.tool_name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(t_stmt, 7, t.tool_arguments.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(t_stmt, 8, t.tool_result.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(t_stmt);
    }
    sqlite3_finalize(t_stmt);
    return true;
}

bool MemoryStore::load_session(const std::string& session_id, SessionMemory& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    out.clear();
    out.set_session_id(session_id);

    const char* sql = "SELECT role, content, tool_name, tool_args, tool_result FROM session_turns "
                      "WHERE session_id = ? ORDER BY turn_index ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string tool_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        std::string tool_args = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        std::string tool_result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        if (role == "tool") {
            out.add_tool_interaction(tool_name, tool_args, tool_result);
        } else {
            out.add_message(role, content);
        }
    }
    sqlite3_finalize(stmt);
    return out.turn_count() > 0;
}

bool MemoryStore::save_archive(const std::string& archive_id,
                               const std::string& session_id,
                               const std::string& summary,
                               const std::string& extracted_facts) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql = "INSERT OR REPLACE INTO consolidated_archives (archive_id, session_id, summary, extracted_facts, created_at) "
                      "VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, archive_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, session_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, summary.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, extracted_facts.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, now);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool MemoryStore::get_archive(const std::string& session_id, std::string& summary_out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "SELECT summary FROM consolidated_archives WHERE session_id = ? ORDER BY created_at DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_STATIC);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        summary_out = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}
