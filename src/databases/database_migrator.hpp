#pragma once

#include <string>
#include <vector>

struct DatabaseStatus {
    std::string name;
    std::string path;
    bool created{false};
    bool migrated{false};
    std::string journal_mode;
};

class DatabaseMigrator {
public:
    // Ensures all databases exist, have complete schemas, and are seeded with defaults
    static bool ensure_all_databases_ready(const std::string& base_dir, bool verbose = true);

    // Individual database self-healing initializers
    static bool bootstrap_settings_db(const std::string& db_path, DatabaseStatus& status);
    static bool bootstrap_memory_db(const std::string& db_path, DatabaseStatus& status);
    static bool bootstrap_symbols_db(const std::string& db_path, DatabaseStatus& status);

private:
    static bool execute_sql(const std::string& db_path, const char* sql, std::string& out_journal);
};
