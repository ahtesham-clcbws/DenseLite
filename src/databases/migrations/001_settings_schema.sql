-- ============================================================================
-- Migration 001: Settings Engine Schema & Production Defaults
-- Target Database: denselite_settings.db
-- Mode: WAL
-- ============================================================================

PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA temp_store = MEMORY;
PRAGMA busy_timeout = 5000;

CREATE TABLE IF NOT EXISTS system_settings (
    module     TEXT NOT NULL,
    key        TEXT NOT NULL,
    value      TEXT NOT NULL,
    val_type   TEXT NOT NULL DEFAULT 'string',
    updated_at INTEGER NOT NULL,
    PRIMARY KEY (module, key)
);

CREATE INDEX IF NOT EXISTS idx_settings_module ON system_settings(module);

-- Seed Initial System Defaults
INSERT OR IGNORE INTO system_settings (module, key, value, val_type, updated_at) VALUES
    ('server', 'host', '0.0.0.0', 'string', 1700000000000),
    ('server', 'port', '9501', 'int', 1700000000000),
    ('server', 'threads', '4', 'int', 1700000000000),
    ('server', 'max_payload_mb', '32', 'int', 1700000000000),
    ('resource', 'ram_budget_percent', '0.45', 'float', 1700000000000),
    ('resource', 'max_kv_tokens', '65536', 'int', 1700000000000),
    ('resource', 'enable_gpu', 'true', 'bool', 1700000000000),
    ('resource', 'vram_budget_mb', '2048', 'int', 1700000000000),
    ('inference', 'default_temperature', '0.7', 'float', 1700000000000),
    ('inference', 'default_top_p', '0.9', 'float', 1700000000000),
    ('inference', 'needle3_mode', 'hybrid', 'string', 1700000000000),
    ('inference', 'enable_tool_dedup', 'true', 'bool', 1700000000000),
    ('inference', 'context_window', '65536', 'int', 1700000000000),
    ('logging', 'level', 'INFO', 'string', 1700000000000),
    ('logging', 'enable_file_logging', 'true', 'bool', 1700000000000),
    ('logging', 'enable_console', 'true', 'bool', 1700000000000),
    ('logging', 'log_path', 'denselite.log', 'string', 1700000000000);
