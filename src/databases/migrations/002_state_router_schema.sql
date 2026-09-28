-- ============================================================================
-- Migration 002: Router & State Management Schema
-- Target Database: denselite_state.db
-- Mode: WAL
-- ============================================================================

PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA temp_store = MEMORY;
PRAGMA busy_timeout = 5000;

CREATE TABLE IF NOT EXISTS metadata (
    key   TEXT PRIMARY KEY,
    value TEXT
);

CREATE TABLE IF NOT EXISTS api_keys (
    provider       TEXT NOT NULL,
    key_index      INTEGER NOT NULL,
    cooldown_until INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (provider, key_index)
);

CREATE TABLE IF NOT EXISTS provider_models (
    provider   TEXT NOT NULL,
    model_name TEXT NOT NULL,
    model_type TEXT NOT NULL, -- 'text' or 'image'
    priority   INTEGER NOT NULL DEFAULT 1,
    UNIQUE(provider, model_name)
);

CREATE INDEX IF NOT EXISTS idx_provider_type ON provider_models(provider, model_type, priority);

-- Seed Initial Provider Models
INSERT OR IGNORE INTO provider_models (provider, model_name, model_type, priority) VALUES
    ('GEMINI', 'gemini-2.5-flash-image', 'image', 1),
    ('GEMINI', 'gemini-2.5-flash', 'text', 1),
    ('GROQ', 'openai/gpt-oss-20b', 'text', 1),
    ('OPENROUTER', 'liquid/lfm-2.5-2.6b:free', 'text', 1),
    ('OPENROUTER', 'inclusionai/ling-3.0-flash-fin:free', 'text', 2),
    ('MISTRAL', 'ministral-8b-2512', 'text', 1),
    ('MISTRAL', 'open-mistral-nemo', 'text', 2),
    ('COHERE', 'command-r-08-2024', 'text', 1),
    ('NOVITA', 'zai-org/glm-5.3-flash', 'text', 1);

INSERT OR REPLACE INTO metadata (key, value) VALUES ('model_seed_version', '2');
