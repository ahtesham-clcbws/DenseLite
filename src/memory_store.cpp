#include "memory_store.hpp"
#include "vector_search.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <filesystem>

MemoryStore::MemoryStore() = default;

MemoryStore::~MemoryStore() {
    close();
}

bool MemoryStore::init(const std::string& sqlite_path, const std::string& zvec_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    close();
    sqlite_path_ = sqlite_path;
    zvec_path_ = zvec_path;

    if (!zvec_path_.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(zvec_path_).parent_path(), ec);
        turboquant_ = std::make_unique<TurboQuantIndex>();
        turboquant_->load(zvec_path_);
    }

    if (sqlite3_open_v2(sqlite_path_.c_str(), &db_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        db_ = nullptr;
        return false;
    }
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL; PRAGMA busy_timeout = 5000;", nullptr, nullptr, nullptr);
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
        "  updated_at INTEGER,"
        "  embedding BLOB,"
        "  workspace_id TEXT NOT NULL DEFAULT 'default',"
        "  session_id TEXT NOT NULL DEFAULT '',"
        "  actor_scope TEXT NOT NULL DEFAULT 'public',"
        "  visibility TEXT NOT NULL DEFAULT 'workspace',"
        "  source_path TEXT DEFAULT '',"
        "  source_type TEXT DEFAULT 'user',"
        "  commit_hash TEXT DEFAULT '',"
        "  importance REAL NOT NULL DEFAULT 0.5,"
        "  access_count INTEGER NOT NULL DEFAULT 0,"
        "  last_accessed_at INTEGER NOT NULL DEFAULT 0,"
        "  expires_at INTEGER DEFAULT 0,"
        "  supersedes_id TEXT DEFAULT '',"
        "  contradicts_id TEXT DEFAULT '',"
        "  status TEXT NOT NULL DEFAULT 'active'"
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
        ");"
        "CREATE INDEX IF NOT EXISTS idx_memories_workspace_status ON memories(workspace_id, status);";
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, schema, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        return false;
    }
    // Migration helpers for pre-existing tables
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN embedding BLOB;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN workspace_id TEXT NOT NULL DEFAULT 'default';", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN session_id TEXT NOT NULL DEFAULT '';", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN actor_scope TEXT NOT NULL DEFAULT 'public';", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN visibility TEXT NOT NULL DEFAULT 'workspace';", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN source_path TEXT DEFAULT '';", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN source_type TEXT DEFAULT 'user';", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN commit_hash TEXT DEFAULT '';", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN importance REAL NOT NULL DEFAULT 0.5;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN access_count INTEGER NOT NULL DEFAULT 0;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN last_accessed_at INTEGER NOT NULL DEFAULT 0;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN expires_at INTEGER DEFAULT 0;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN supersedes_id TEXT DEFAULT '';", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN contradicts_id TEXT DEFAULT '';", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "ALTER TABLE memories ADD COLUMN status TEXT NOT NULL DEFAULT 'active';", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "CREATE INDEX IF NOT EXISTS idx_memories_workspace_status ON memories(workspace_id, status);", nullptr, nullptr, nullptr);
    return true;
}

static inline std::string safe_text(sqlite3_stmt* stmt, int col) {
    const unsigned char* t = sqlite3_column_text(stmt, col);
    return t ? reinterpret_cast<const char*>(t) : "";
}

static inline MemoryEntry extract_memory_entry(sqlite3_stmt* stmt) {
    MemoryEntry entry;
    entry.key = safe_text(stmt, 0);
    entry.id = safe_text(stmt, 1);
    entry.category = static_cast<MemoryCategory>(sqlite3_column_int(stmt, 2));
    entry.value = safe_text(stmt, 3);
    entry.confidence = static_cast<float>(sqlite3_column_double(stmt, 4));
    entry.updated_at = sqlite3_column_int64(stmt, 5);
    entry.workspace_id = safe_text(stmt, 6);
    entry.session_id = safe_text(stmt, 7);
    entry.actor_scope = safe_text(stmt, 8);
    entry.visibility = safe_text(stmt, 9);
    entry.source_path = safe_text(stmt, 10);
    entry.source_type = safe_text(stmt, 11);
    entry.commit_hash = safe_text(stmt, 12);
    entry.importance = static_cast<float>(sqlite3_column_double(stmt, 13));
    entry.access_count = sqlite3_column_int(stmt, 14);
    entry.last_accessed_at = sqlite3_column_int64(stmt, 15);
    entry.expires_at = sqlite3_column_int64(stmt, 16);
    entry.supersedes_id = safe_text(stmt, 17);
    entry.contradicts_id = safe_text(stmt, 18);
    entry.status = safe_text(stmt, 19);
    if (entry.status.empty()) entry.status = "active";
    return entry;
}

bool MemoryStore::put_memory(const MemoryEntry& entry, const std::vector<float>& embedding) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    std::vector<float> emb = embedding;
    if (emb.empty()) {
        emb = VectorSearch::embed_text(entry.value);
    }

    const char* sql = "INSERT INTO memories ("
                      "  key, id, category, value, confidence, updated_at, embedding, "
                      "  workspace_id, session_id, actor_scope, visibility, "
                      "  source_path, source_type, commit_hash, importance, "
                      "  access_count, last_accessed_at, expires_at, "
                      "  supersedes_id, contradicts_id, status"
                      ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(key) DO UPDATE SET "
                      "  value=excluded.value, confidence=excluded.confidence, "
                      "  updated_at=excluded.updated_at, embedding=excluded.embedding, "
                      "  importance=excluded.importance, status=excluded.status, "
                      "  supersedes_id=excluded.supersedes_id, contradicts_id=excluded.contradicts_id, "
                      "  access_count = memories.access_count + 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, entry.key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, entry.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, static_cast<int>(entry.category));
    sqlite3_bind_text(stmt, 4, entry.value.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 5, entry.confidence);
    sqlite3_bind_int64(stmt, 6, entry.updated_at);
    if (!emb.empty()) {
        sqlite3_bind_blob(stmt, 7, emb.data(), static_cast<int>(emb.size() * sizeof(float)), SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 7);
    }
    sqlite3_bind_text(stmt, 8, entry.workspace_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, entry.session_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, entry.actor_scope.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 11, entry.visibility.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 12, entry.source_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 13, entry.source_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 14, entry.commit_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 15, entry.importance);
    sqlite3_bind_int(stmt, 16, entry.access_count);
    sqlite3_bind_int64(stmt, 17, entry.last_accessed_at);
    sqlite3_bind_int64(stmt, 18, entry.expires_at);
    sqlite3_bind_text(stmt, 19, entry.supersedes_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 20, entry.contradicts_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 21, entry.status.c_str(), -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (ok && turboquant_ && !emb.empty()) {
        turboquant_->add(entry.key, emb);
        turboquant_->save(zvec_path_);
    }
    return ok;
}

bool MemoryStore::get_memory(const std::string& key, MemoryEntry& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "SELECT key, id, category, value, confidence, updated_at, "
                      "workspace_id, session_id, actor_scope, visibility, "
                      "source_path, source_type, commit_hash, importance, "
                      "access_count, last_accessed_at, expires_at, "
                      "supersedes_id, contradicts_id, status FROM memories WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out = extract_memory_entry(stmt);
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
    if (ok && turboquant_) {
        turboquant_->remove(key);
        turboquant_->save(zvec_path_);
    }
    return ok;
}

bool MemoryStore::supersede_memory(const std::string& old_key, const std::string& new_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || old_key.empty()) return false;

    const char* sql = "UPDATE memories SET status = 'superseded', supersedes_id = ? WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, new_key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, old_key.c_str(), -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool MemoryStore::touch_memory(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || key.empty()) return false;

    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const char* sql = "UPDATE memories SET access_count = access_count + 1, last_accessed_at = ? WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<MemoryEntry> MemoryStore::query_memories_keyword(const std::string& query, size_t limit, const MemoryScopeFilter& filter) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryEntry> results;
    if (!db_) return results;

    std::string sql = "SELECT key, id, category, value, confidence, updated_at, "
                      "workspace_id, session_id, actor_scope, visibility, "
                      "source_path, source_type, commit_hash, importance, "
                      "access_count, last_accessed_at, expires_at, "
                      "supersedes_id, contradicts_id, status FROM memories "
                      "WHERE (key LIKE ? OR value LIKE ?) AND status = 'active'";

    if (!filter.workspace_id.empty()) {
        sql += " AND (workspace_id = ? OR visibility = 'global')";
    }
    if (!filter.session_id.empty()) {
        sql += " AND session_id = ?";
    }
    sql += ";";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;

    std::string pattern = "%" + query + "%";
    int param_idx = 1;
    sqlite3_bind_text(stmt, param_idx++, pattern.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, param_idx++, pattern.c_str(), -1, SQLITE_STATIC);
    
    if (!filter.workspace_id.empty()) {
        sqlite3_bind_text(stmt, param_idx++, filter.workspace_id.c_str(), -1, SQLITE_STATIC);
    }
    if (!filter.session_id.empty()) {
        sqlite3_bind_text(stmt, param_idx++, filter.session_id.c_str(), -1, SQLITE_STATIC);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MemoryEntry entry = extract_memory_entry(stmt);
        if (filter.matches(entry)) {
            results.push_back(std::move(entry));
            if (results.size() >= limit) break;
        }
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<std::pair<MemoryEntry, float>> MemoryStore::query_memories_vector(
    const std::vector<float>& query_vec, size_t limit, float threshold,
    const MemoryScopeFilter& filter) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<MemoryEntry, float>> scored;
    if (!db_ || query_vec.empty()) return scored;

    if (turboquant_) {
        size_t fetch_limit = std::max<size_t>(2000, limit * 100);
        auto tq_hits = turboquant_->search(query_vec, fetch_limit);
        if (tq_hits.empty()) return scored;

        std::string placeholders;
        for (size_t i = 0; i < tq_hits.size(); ++i) {
            placeholders += "?";
            if (i + 1 < tq_hits.size()) placeholders += ",";
        }

        std::string sql_str = "SELECT key, id, category, value, confidence, updated_at, "
                          "workspace_id, session_id, actor_scope, visibility, "
                          "source_path, source_type, commit_hash, importance, "
                          "access_count, last_accessed_at, expires_at, "
                          "supersedes_id, contradicts_id, status FROM memories WHERE key IN (" + placeholders + ");";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return scored;

        for (size_t i = 0; i < tq_hits.size(); ++i) {
            sqlite3_bind_text(stmt, i + 1, tq_hits[i].id.c_str(), -1, SQLITE_STATIC);
        }

        std::unordered_map<std::string, MemoryEntry> entry_map;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MemoryEntry entry = extract_memory_entry(stmt);
            if (filter.matches(entry)) {
                entry_map[entry.key] = std::move(entry);
            }
        }
        sqlite3_finalize(stmt);

        for (const auto& hit : tq_hits) {
            if (hit.score >= threshold) {
                auto it = entry_map.find(hit.id);
                if (it != entry_map.end()) {
                    scored.emplace_back(std::move(it->second), hit.score);
                    if (scored.size() >= limit) break;
                }
            }
        }
        return scored;
    }

    const char* sql = "SELECT key, id, category, value, confidence, updated_at, "
                      "workspace_id, session_id, actor_scope, visibility, "
                      "source_path, source_type, commit_hash, importance, "
                      "access_count, last_accessed_at, expires_at, "
                      "supersedes_id, contradicts_id, status, embedding "
                      "FROM memories WHERE embedding IS NOT NULL;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return scored;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MemoryEntry entry = extract_memory_entry(stmt);
        if (!filter.matches(entry)) {
            continue;
        }

        const void* blob = sqlite3_column_blob(stmt, 20);
        int bytes = sqlite3_column_bytes(stmt, 20);
        if (blob && bytes > 0 && (bytes % sizeof(float) == 0)) {
            size_t num_floats = bytes / sizeof(float);
            const float* float_ptr = reinterpret_cast<const float*>(blob);
            std::vector<float> doc_vec(float_ptr, float_ptr + num_floats);
            float sim = VectorSearch::cosine_similarity(query_vec, doc_vec);
            if (sim >= threshold) {
                scored.emplace_back(std::move(entry), sim);
            }
        }
    }
    sqlite3_finalize(stmt);

    std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    if (scored.size() > limit) scored.resize(limit);
    return scored;
}

std::vector<MemoryEntry> MemoryStore::load_all_persistent(const MemoryScopeFilter& filter) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MemoryEntry> results;
    if (!db_) return results;

    const char* sql = "SELECT key, id, category, value, confidence, updated_at, "
                      "workspace_id, session_id, actor_scope, visibility, "
                      "source_path, source_type, commit_hash, importance, "
                      "access_count, last_accessed_at, expires_at, "
                      "supersedes_id, contradicts_id, status FROM memories;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MemoryEntry entry = extract_memory_entry(stmt);
        if (filter.matches(entry)) {
            results.push_back(std::move(entry));
        }
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

    const char* sql_sess = "INSERT INTO sessions (session_id, turn_count, updated_at) VALUES (?, ?, ?) "
                           "ON CONFLICT(session_id) DO UPDATE SET turn_count = excluded.turn_count, updated_at = excluded.updated_at;";
    sqlite3_stmt* stmt_s = nullptr;
    if (sqlite3_prepare_v2(db_, sql_sess, -1, &stmt_s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt_s, 1, sid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt_s, 2, static_cast<int>(turns.size()));
    sqlite3_bind_int64(stmt_s, 3, now);
    sqlite3_step(stmt_s);
    sqlite3_finalize(stmt_s);

    // Delete existing turns and insert new turns in a single transaction
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
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
    if (sqlite3_prepare_v2(db_, turn_sql, -1, &t_stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

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
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
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
        std::string role = safe_text(stmt, 0);
        std::string content = safe_text(stmt, 1);
        std::string tool_name = safe_text(stmt, 2);
        std::string tool_args = safe_text(stmt, 3);
        std::string tool_result = safe_text(stmt, 4);

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

    const char* sql = "INSERT INTO consolidated_archives (archive_id, session_id, summary, extracted_facts, created_at) "
                      "VALUES (?, ?, ?, ?, ?) "
                      "ON CONFLICT(archive_id) DO UPDATE SET summary = excluded.summary, extracted_facts = excluded.extracted_facts, created_at = excluded.created_at;";
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
        summary_out = safe_text(stmt, 0);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}
