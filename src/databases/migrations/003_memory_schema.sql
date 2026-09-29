-- ============================================================================
-- Migration 003: Episodic, Session & Turn Memory Schema
-- Target Database: denselite_memory.db
-- Mode: WAL
-- ============================================================================

PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA temp_store = MEMORY;
PRAGMA busy_timeout = 5000;

CREATE TABLE IF NOT EXISTS memories (
    key             TEXT PRIMARY KEY,
    id              TEXT,
    category        INTEGER,
    value           TEXT,
    confidence      REAL,
    updated_at      INTEGER,
    embedding       BLOB,
    workspace_id    TEXT NOT NULL DEFAULT 'default',
    session_id      TEXT NOT NULL DEFAULT '',
    actor_scope     TEXT NOT NULL DEFAULT 'public',
    visibility      TEXT NOT NULL DEFAULT 'workspace',
    source_path     TEXT DEFAULT '',
    source_type     TEXT DEFAULT 'user',
    commit_hash     TEXT DEFAULT '',
    importance      REAL NOT NULL DEFAULT 0.5,
    access_count    INTEGER NOT NULL DEFAULT 0,
    last_accessed_at INTEGER NOT NULL DEFAULT 0,
    expires_at      INTEGER DEFAULT 0,
    supersedes_id   TEXT DEFAULT '',
    contradicts_id  TEXT DEFAULT '',
    status          TEXT NOT NULL DEFAULT 'active'
);

CREATE TABLE IF NOT EXISTS sessions (
    session_id  TEXT PRIMARY KEY,
    turn_count  INTEGER,
    updated_at  INTEGER
);

CREATE TABLE IF NOT EXISTS session_turns (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id  TEXT,
    turn_index  INTEGER,
    timestamp   INTEGER,
    role        TEXT,
    content     TEXT,
    tool_name   TEXT,
    tool_args   TEXT,
    tool_result TEXT
);

CREATE TABLE IF NOT EXISTS consolidated_archives (
    archive_id      TEXT PRIMARY KEY,
    session_id      TEXT,
    summary         TEXT,
    extracted_facts TEXT,
    created_at      INTEGER
);

CREATE INDEX IF NOT EXISTS idx_session_turns ON session_turns(session_id, turn_index);
CREATE INDEX IF NOT EXISTS idx_memories_category ON memories(category);
