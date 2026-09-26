#pragma once
#include "structural_chunk.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <sqlite3.h>

class SymbolIndex {
public:
    SymbolIndex();
    explicit SymbolIndex(sqlite3* shared_db);
    ~SymbolIndex();

    bool init(const std::string& db_path);
    bool set_shared_db(sqlite3* db);

    bool insert_chunks(const std::vector<StructuralChunk>& chunks);
    bool remove_file_symbols(const std::string& file_path);

    std::vector<StructuralChunk> find_by_symbol(const std::string& symbol_query);
    std::vector<StructuralChunk> get_file_symbols(const std::string& file_path);
    size_t count() const;

private:
    mutable std::mutex mutex_;
    sqlite3* db_ = nullptr;
    bool owns_db_ = false;

    bool create_table();
};
