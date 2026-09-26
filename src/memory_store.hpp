#pragma once
#include "persistent_memory.hpp"
#include "session_memory.hpp"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <sqlite3.h>

class MemoryStore {
public:
    MemoryStore();
    ~MemoryStore();

    // Initializes canonical SQLite database and optional Zvec index
    bool init(const std::string& sqlite_path, const std::string& zvec_path = "");
    void close();

    // Canonical Memory CRUD
    bool put_memory(const MemoryEntry& entry, const std::vector<float>& embedding = {});
    bool get_memory(const std::string& key, MemoryEntry& out);
    bool delete_memory(const std::string& key);
    std::vector<MemoryEntry> query_memories_keyword(const std::string& query, size_t limit = 10);
    std::vector<MemoryEntry> load_all_persistent();

    // Session Persistence
    bool save_session(const SessionMemory& session);
    bool load_session(const std::string& session_id, SessionMemory& out);
    std::vector<std::string> list_sessions();

    // Archive / Compaction Storage
    bool save_archive(const std::string& archive_id,
                      const std::string& session_id,
                      const std::string& summary,
                      const std::string& extracted_facts);
    bool get_archive(const std::string& session_id, std::string& summary_out);

    bool is_open() const { return db_ != nullptr; }

private:
    mutable std::mutex mutex_;
    sqlite3* db_ = nullptr;
    std::string sqlite_path_;
    std::string zvec_path_;

    bool create_tables();
};
