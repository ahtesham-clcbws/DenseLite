-- ============================================================================
-- Migration 001: Settings & Control Plane Engine Schema & Production Defaults
-- Target Database: denselite_settings.db
-- Mode: WAL
-- ============================================================================

PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA temp_store = MEMORY;
PRAGMA busy_timeout = 5000;

-- 1. System Settings Registry
CREATE TABLE IF NOT EXISTS system_settings (
    module     TEXT NOT NULL,
    key        TEXT NOT NULL,
    value      TEXT NOT NULL,
    val_type   TEXT NOT NULL DEFAULT 'string',
    updated_at INTEGER NOT NULL,
    PRIMARY KEY (module, key)
);

CREATE INDEX IF NOT EXISTS idx_settings_module ON system_settings(module);

-- 2. Metadata Key-Value Store
CREATE TABLE IF NOT EXISTS metadata (
    key   TEXT PRIMARY KEY,
    value TEXT
);

-- 3. API Key Cooldown State
CREATE TABLE IF NOT EXISTS api_keys (
    provider       TEXT NOT NULL,
    key_index      INTEGER NOT NULL,
    cooldown_until INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (provider, key_index)
);

-- 4. Provider Models Registry & Priorities
CREATE TABLE IF NOT EXISTS provider_models (
    provider   TEXT NOT NULL,
    model_name TEXT NOT NULL,
    model_type TEXT NOT NULL, -- 'text' or 'image'
    priority   INTEGER NOT NULL DEFAULT 1,
    UNIQUE(provider, model_name)
);

CREATE INDEX IF NOT EXISTS idx_provider_type ON provider_models(provider, model_type, priority);

-- 5. Physical Local Model Inventory
CREATE TABLE IF NOT EXISTS local_models (
    model_id        TEXT PRIMARY KEY,
    file_path       TEXT NOT NULL UNIQUE,
    architecture    TEXT NOT NULL,
    param_count     INTEGER NOT NULL,
    param_size_str  TEXT NOT NULL,
    quant_type      TEXT NOT NULL,
    context_length  INTEGER NOT NULL,
    is_verified     INTEGER NOT NULL DEFAULT 1,
    updated_at      INTEGER NOT NULL
);

-- 6. Dynamic Task Role Assignments
CREATE TABLE IF NOT EXISTS model_roles (
    role            TEXT PRIMARY KEY,
    model_id        TEXT NOT NULL,
    is_active       INTEGER NOT NULL DEFAULT 1,
    updated_at      INTEGER NOT NULL,
    FOREIGN KEY(model_id) REFERENCES local_models(model_id)
);

-- Seed Initial System Settings
INSERT OR IGNORE INTO system_settings (module, key, value, val_type, updated_at) VALUES
    ('server', 'host', '127.0.0.1', 'string', 1700000000000),
    ('server', 'port', '9501', 'int', 1700000000000),
    ('server', 'threads', '4', 'int', 1700000000000),
    ('server', 'max_payload_mb', '32', 'int', 1700000000000),
    ('server', 'enable_api_auth', 'true', 'bool', 1700000000000),
    ('server', 'api_secret_key', '', 'string', 1700000000000),
    ('server', 'cors_allowed_origins', '', 'string', 1700000000000),
    ('resource', 'ram_budget_percent', '0.50', 'float', 1700000000000),
    ('resource', 'gpu_budget_percent', '0.85', 'float', 1700000000000),
    ('resource', 'headroom_safety_multiplier', '1.10', 'float', 1700000000000),
    ('resource', 'max_kv_tokens', '65536', 'int', 1700000000000),
    ('resource', 'enable_gpu', 'true', 'bool', 1700000000000),
    ('resource', 'vram_budget_mb', '2048', 'int', 1700000000000),
    ('inference', 'default_temperature', '0.7', 'float', 1700000000000),
    ('inference', 'default_top_p', '0.9', 'float', 1700000000000),
    ('inference', 'repeat_penalty', '1.15', 'float', 1700000000000),
    ('inference', 'repeat_last_n', '64', 'int', 1700000000000),
    ('inference', 'top_k', '40', 'int', 1700000000000),
    ('inference', 'min_p', '0.05', 'float', 1700000000000),
    ('inference', 'max_output_tokens', '512', 'int', 1700000000000),
    ('inference', 'routing_mode', 'modernbert', 'string', 1700000000000),
    ('inference', 'enable_tool_dedup', 'true', 'bool', 1700000000000),
    ('inference', 'context_window', '65536', 'int', 1700000000000),
    ('inference', 'enable_context_injection', 'true', 'bool', 1700000000000),
    ('inference', 'system_prompt', 'You are DenseLite, a fast, concise programming and chat assistant.', 'string', 1700000000000),
    ('multimodal', 'sd_steps', '4', 'int', 1700000000000),
    ('multimodal', 'sd_cfg_scale', '1.5', 'float', 1700000000000),
    ('multimodal', 'sd_width', '1024', 'int', 1700000000000),
    ('multimodal', 'sd_height', '1024', 'int', 1700000000000),
    ('multimodal', 'sd_negative_prompt', 'ugly, blurry, distorted, low quality', 'string', 1700000000000),
    ('multimodal', 'whisper_language', 'auto', 'string', 1700000000000),
    ('multimodal', 'whisper_beam_size', '5', 'int', 1700000000000),
    ('logging', 'level', 'INFO', 'string', 1700000000000),
    ('logging', 'enable_file_logging', 'true', 'bool', 1700000000000),
    ('logging', 'enable_console', 'true', 'bool', 1700000000000),
    ('logging', 'log_path', 'denselite.log', 'string', 1700000000000),
    ('storage', 'models_dir', '~/.denselite/models', 'string', 1700000000000),
    ('storage', 'data_dir', '~/.denselite/data', 'string', 1700000000000),
    ('storage', 'kv_cache_dir', '~/.denselite/kv_cache', 'string', 1700000000000),
    ('storage', 'logs_dir', '~/.denselite/logs', 'string', 1700000000000);

-- Seed Initial Model Roles
INSERT OR IGNORE INTO model_roles (role, model_id, is_active, updated_at) VALUES
    ('general', 'llama-3_2-1b-instruct-abliterated_i1-q4_k_m', 1, 1700000000000),
    ('coder', 'deepseek-r1-distill-qwen-1_5b-q4_k_m', 1, 1700000000000),
    ('compressor', 'smollm2', 1, 1700000000000),
    ('embedding', 'nomic_embed', 1, 1700000000000),
    ('image_gen', 'sdxl_lightning_4step', 1, 1700000000000),
    ('vision_sd', 'sdxl_lightning_4step', 1, 1700000000000),
    ('audio_stt', 'whisper_turbo', 1, 1700000000000),
    ('router', 'modernbert_router', 1, 1700000000000);

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

INSERT OR REPLACE INTO metadata (key, value) VALUES ('model_seed_version', '3');
