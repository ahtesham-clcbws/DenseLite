-- ============================================================================
-- Migration 004: Code Intelligence & Tree-Sitter Symbol Index Schema
-- Target Database: denselite_symbols.db
-- Mode: WAL
-- ============================================================================

PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA temp_store = MEMORY;
PRAGMA busy_timeout = 5000;

CREATE TABLE IF NOT EXISTS code_symbols (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path     TEXT NOT NULL,
    symbol        TEXT NOT NULL,
    parent_symbol TEXT,
    language      TEXT NOT NULL,
    start_line    INTEGER NOT NULL,
    end_line      INTEGER NOT NULL,
    source_hash   INTEGER NOT NULL,
    content       TEXT
);

CREATE INDEX IF NOT EXISTS idx_sym_file ON code_symbols(file_path);
CREATE INDEX IF NOT EXISTS idx_sym_name ON code_symbols(symbol);
CREATE INDEX IF NOT EXISTS idx_sym_lang ON code_symbols(language);
