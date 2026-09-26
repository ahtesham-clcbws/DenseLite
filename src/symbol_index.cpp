#include "symbol_index.hpp"
#include <iostream>

SymbolIndex::SymbolIndex() = default;

SymbolIndex::SymbolIndex(sqlite3* shared_db)
    : db_(shared_db), owns_db_(false) {
    create_table();
}

SymbolIndex::~SymbolIndex() {
    if (owns_db_ && db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SymbolIndex::init(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (owns_db_ && db_) {
        sqlite3_close(db_);
    }
    if (sqlite3_open_v2(db_path.c_str(), &db_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        db_ = nullptr;
        return false;
    }
    owns_db_ = true;
    return create_table();
}

bool SymbolIndex::set_shared_db(sqlite3* db) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (owns_db_ && db_) {
        sqlite3_close(db_);
    }
    db_ = db;
    owns_db_ = false;
    return create_table();
}

bool SymbolIndex::create_table() {
    if (!db_) return false;
    const char* sql =
        "CREATE TABLE IF NOT EXISTS code_symbols ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  file_path TEXT,"
        "  symbol TEXT,"
        "  parent_symbol TEXT,"
        "  language TEXT,"
        "  start_line INTEGER,"
        "  end_line INTEGER,"
        "  source_hash INTEGER,"
        "  content TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_sym_file ON code_symbols(file_path);"
        "CREATE INDEX IF NOT EXISTS idx_sym_name ON code_symbols(symbol);";
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool SymbolIndex::remove_file_symbols(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "DELETE FROM code_symbols WHERE file_path = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, file_path.c_str(), -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool SymbolIndex::insert_chunks(const std::vector<StructuralChunk>& chunks) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    const char* sql = "INSERT INTO code_symbols (file_path, symbol, parent_symbol, language, start_line, end_line, source_hash, content) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    for (const auto& c : chunks) {
        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, c.file_path.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, c.symbol.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, c.parent_symbol.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, c.language.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, c.start_line);
        sqlite3_bind_int(stmt, 6, c.end_line);
        sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(c.source_hash));
        sqlite3_bind_text(stmt, 8, c.content.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    return true;
}

std::vector<StructuralChunk> SymbolIndex::find_by_symbol(const std::string& symbol_query) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StructuralChunk> results;
    if (!db_) return results;

    const char* sql = "SELECT file_path, symbol, parent_symbol, language, start_line, end_line, source_hash, content "
                      "FROM code_symbols WHERE symbol LIKE ? LIMIT 50;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    std::string pattern = "%" + symbol_query + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StructuralChunk c;
        c.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        c.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        c.parent_symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        c.language = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        c.start_line = sqlite3_column_int(stmt, 4);
        c.end_line = sqlite3_column_int(stmt, 5);
        c.source_hash = static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));
        c.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        results.push_back(std::move(c));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<StructuralChunk> SymbolIndex::get_file_symbols(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StructuralChunk> results;
    if (!db_) return results;

    const char* sql = "SELECT file_path, symbol, parent_symbol, language, start_line, end_line, source_hash, content "
                      "FROM code_symbols WHERE file_path = ? ORDER BY start_line ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    sqlite3_bind_text(stmt, 1, file_path.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StructuralChunk c;
        c.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        c.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        c.parent_symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        c.language = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        c.start_line = sqlite3_column_int(stmt, 4);
        c.end_line = sqlite3_column_int(stmt, 5);
        c.source_hash = static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));
        c.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        results.push_back(std::move(c));
    }
    sqlite3_finalize(stmt);
    return results;
}

size_t SymbolIndex::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return 0;
    const char* sql = "SELECT COUNT(*) FROM code_symbols;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    size_t cnt = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        cnt = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return cnt;
}
