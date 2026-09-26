#include "code_indexer.hpp"

CodeIndexer::CodeIndexer()
    : chunker_(&languages_) {}

bool CodeIndexer::init(const std::string& db_path) {
    return symbols_.init(db_path);
}

bool CodeIndexer::set_shared_db(sqlite3* db) {
    return symbols_.set_shared_db(db);
}

IndexResult CodeIndexer::index_file(const std::string& file_path,
                                    const std::string& content,
                                    bool force) {
    IndexResult res;
    res.file_path = file_path;

    if (!force && !tracker_.has_changed(file_path, content)) {
        res.was_skipped = true;
        return res;
    }

    // Parse AST and generate structural chunks
    auto chunks = chunker_.chunk(file_path, content);
    res.chunk_count = chunks.size();

    // Invalidate existing symbols for this file
    symbols_.remove_file_symbols(file_path);

    // Insert newly parsed symbols
    if (!chunks.empty()) {
        symbols_.insert_chunks(chunks);
    }

    // Update recorded hash
    tracker_.update_hash(file_path, content);
    res.was_skipped = false;
    return res;
}

std::vector<StructuralChunk> CodeIndexer::query_symbol(const std::string& symbol_name) {
    return symbols_.find_by_symbol(symbol_name);
}

std::vector<StructuralChunk> CodeIndexer::get_file_symbols(const std::string& file_path) {
    return symbols_.get_file_symbols(file_path);
}
