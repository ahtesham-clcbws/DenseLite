#pragma once
#include "language_registry.hpp"
#include "ast_chunker.hpp"
#include "symbol_index.hpp"
#include "code_change_tracker.hpp"
#include <string>
#include <vector>

struct IndexResult {
    std::string file_path;
    bool was_skipped = false;
    size_t chunk_count = 0;
};

class CodeIndexer {
public:
    CodeIndexer();

    bool init(const std::string& db_path);
    bool set_shared_db(sqlite3* db);

    // Incrementally indexes a file: checks hash, skips if unchanged, parses AST on delta
    IndexResult index_file(const std::string& file_path,
                           const std::string& content,
                           bool force = false);

    // Queries symbols by name or qualified prefix
    std::vector<StructuralChunk> query_symbol(const std::string& symbol_name);

    // Retrieves all structural symbols in a file
    std::vector<StructuralChunk> get_file_symbols(const std::string& file_path);

    LanguageRegistry& languages() { return languages_; }
    SymbolIndex& symbols() { return symbols_; }
    CodeChangeTracker& tracker() { return tracker_; }

private:
    LanguageRegistry languages_;
    ASTChunker chunker_;
    SymbolIndex symbols_;
    CodeChangeTracker tracker_;
};
