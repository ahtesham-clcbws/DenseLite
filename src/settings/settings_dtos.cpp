#include "settings_manager.hpp"
#include "path_service.hpp"
#include <cstdlib>

ServerConfig SettingsManager::get_server_config() const {
    ServerConfig cfg; cfg.version = DENSELITE_VERSION;
    cfg.host = get_string("server", "host", cfg.host); cfg.port = get_int("server", "port", cfg.port);
    cfg.threads = get_int("server", "threads", cfg.threads); cfg.max_payload_mb = get_int("server", "max_payload_mb", cfg.max_payload_mb);
    cfg.enable_api_auth = get_bool("server", "enable_api_auth", cfg.enable_api_auth);
    cfg.api_secret_key = get_string("server", "api_secret_key", cfg.api_secret_key);
    cfg.cors_allowed_origins = get_string("server", "cors_allowed_origins", cfg.cors_allowed_origins);
    cfg.n_batch = get_int("server", "n_batch", cfg.n_batch);
    return cfg;
}

void SettingsManager::set_server_config(const ServerConfig& c) {
    set_string("server", "host", c.host); set_int("server", "port", c.port);
    set_int("server", "threads", c.threads); set_int("server", "max_payload_mb", c.max_payload_mb);
    set_bool("server", "enable_api_auth", c.enable_api_auth); set_string("server", "api_secret_key", c.api_secret_key);
    set_string("server", "cors_allowed_origins", c.cors_allowed_origins); set_int("server", "n_batch", c.n_batch);
}

ResourceConfig SettingsManager::get_resource_config() const {
    ResourceConfig cfg;
    cfg.ram_budget_percent = get_float("resource", "ram_budget_percent", cfg.ram_budget_percent);
    cfg.gpu_budget_percent = get_float("resource", "gpu_budget_percent", cfg.gpu_budget_percent);
    cfg.headroom_safety_multiplier = get_float("resource", "headroom_safety_multiplier", cfg.headroom_safety_multiplier);
    cfg.max_kv_tokens = get_int("resource", "max_kv_tokens", cfg.max_kv_tokens);
    cfg.enable_gpu = get_bool("resource", "enable_gpu", cfg.enable_gpu);
    cfg.vram_budget_mb = get_int("resource", "vram_budget_mb", cfg.vram_budget_mb);
    return cfg;
}

void SettingsManager::set_resource_config(const ResourceConfig& c) {
    set_float("resource", "ram_budget_percent", c.ram_budget_percent);
    set_float("resource", "gpu_budget_percent", c.gpu_budget_percent);
    set_float("resource", "headroom_safety_multiplier", c.headroom_safety_multiplier);
    set_int("resource", "max_kv_tokens", c.max_kv_tokens);
    set_bool("resource", "enable_gpu", c.enable_gpu);
    set_int("resource", "vram_budget_mb", c.vram_budget_mb);
}

InferenceConfig SettingsManager::get_inference_config() const {
    InferenceConfig cfg;
    cfg.default_temperature = get_float("inference", "default_temperature", cfg.default_temperature);
    cfg.default_top_p = get_float("inference", "default_top_p", cfg.default_top_p);
    cfg.repeat_penalty = get_float("inference", "repeat_penalty", cfg.repeat_penalty);
    cfg.repeat_last_n = get_int("inference", "repeat_last_n", cfg.repeat_last_n);
    cfg.top_k = get_int("inference", "top_k", cfg.top_k);
    cfg.min_p = get_float("inference", "min_p", cfg.min_p);
    cfg.max_output_tokens = get_int("inference", "max_output_tokens", cfg.max_output_tokens);
    cfg.routing_mode = get_string("inference", "routing_mode", get_string("inference", "needle3_mode", cfg.routing_mode));
    cfg.enable_tool_dedup = get_bool("inference", "enable_tool_dedup", cfg.enable_tool_dedup);
    cfg.context_window = get_int("inference", "context_window", cfg.context_window);
    cfg.enable_context_injection = get_bool("inference", "enable_context_injection", cfg.enable_context_injection);
    cfg.system_prompt = get_string("inference", "system_prompt", cfg.system_prompt);
    return cfg;
}

void SettingsManager::set_inference_config(const InferenceConfig& c) {
    set_float("inference", "default_temperature", c.default_temperature); set_float("inference", "default_top_p", c.default_top_p);
    set_float("inference", "repeat_penalty", c.repeat_penalty); set_int("inference", "repeat_last_n", c.repeat_last_n);
    set_int("inference", "top_k", c.top_k); set_float("inference", "min_p", c.min_p);
    set_int("inference", "max_output_tokens", c.max_output_tokens);
    set_string("inference", "routing_mode", c.routing_mode); set_bool("inference", "enable_tool_dedup", c.enable_tool_dedup);
    set_int("inference", "context_window", c.context_window); set_bool("inference", "enable_context_injection", c.enable_context_injection);
    set_string("inference", "system_prompt", c.system_prompt);
}

MultimodalConfig SettingsManager::get_multimodal_config() const {
    MultimodalConfig cfg;
    cfg.sd_steps = get_int("multimodal", "sd_steps", cfg.sd_steps);
    cfg.sd_cfg_scale = get_float("multimodal", "sd_cfg_scale", cfg.sd_cfg_scale);
    cfg.sd_width = get_int("multimodal", "sd_width", cfg.sd_width);
    cfg.sd_height = get_int("multimodal", "sd_height", cfg.sd_height);
    cfg.sd_negative_prompt = get_string("multimodal", "sd_negative_prompt", cfg.sd_negative_prompt);
    cfg.whisper_language = get_string("multimodal", "whisper_language", cfg.whisper_language);
    cfg.whisper_beam_size = get_int("multimodal", "whisper_beam_size", cfg.whisper_beam_size);
    return cfg;
}

void SettingsManager::set_multimodal_config(const MultimodalConfig& c) {
    set_int("multimodal", "sd_steps", c.sd_steps); set_float("multimodal", "sd_cfg_scale", c.sd_cfg_scale);
    set_int("multimodal", "sd_width", c.sd_width); set_int("multimodal", "sd_height", c.sd_height);
    set_string("multimodal", "sd_negative_prompt", c.sd_negative_prompt);
    set_string("multimodal", "whisper_language", c.whisper_language); set_int("multimodal", "whisper_beam_size", c.whisper_beam_size);
}

MemoryConfig SettingsManager::get_memory_config() const {
    MemoryConfig cfg;
    cfg.search_top_k = get_int("memory", "search_top_k", cfg.search_top_k);
    cfg.similarity_threshold = get_float("memory", "similarity_threshold", cfg.similarity_threshold);
    cfg.max_snippet_lines = get_int("memory", "max_snippet_lines", cfg.max_snippet_lines);
    cfg.excluded_paths = get_string("memory", "excluded_paths", cfg.excluded_paths);
    return cfg;
}

void SettingsManager::set_memory_config(const MemoryConfig& c) {
    set_int("memory", "search_top_k", c.search_top_k); set_float("memory", "similarity_threshold", c.similarity_threshold);
    set_int("memory", "max_snippet_lines", c.max_snippet_lines); set_string("memory", "excluded_paths", c.excluded_paths);
}

CloudConfig SettingsManager::get_cloud_config() const {
    CloudConfig cfg;
    cfg.openrouter_api_key = get_string("cloud", "openrouter_api_key", cfg.openrouter_api_key);
    if (cfg.openrouter_api_key.empty()) { const char* env = std::getenv("OPENROUTER_API_KEY"); if (env) cfg.openrouter_api_key = env; }
    
    cfg.gemini_api_key = get_string("cloud", "gemini_api_key", cfg.gemini_api_key);
    if (cfg.gemini_api_key.empty()) { const char* env = std::getenv("GEMINI_API_KEY"); if (env) cfg.gemini_api_key = env; }
    
    cfg.openai_api_key = get_string("cloud", "openai_api_key", cfg.openai_api_key);
    if (cfg.openai_api_key.empty()) { const char* env = std::getenv("OPENAI_API_KEY"); if (env) cfg.openai_api_key = env; }
    cfg.cloud_fallback_enabled = get_bool("cloud", "cloud_fallback_enabled", cfg.cloud_fallback_enabled);
    cfg.cloud_priority = get_string("cloud", "cloud_priority", cfg.cloud_priority);
    return cfg;
}

void SettingsManager::set_cloud_config(const CloudConfig& c) {
    set_string("cloud", "openrouter_api_key", c.openrouter_api_key);
    set_string("cloud", "gemini_api_key", c.gemini_api_key);
    set_string("cloud", "openai_api_key", c.openai_api_key);
    set_bool("cloud", "cloud_fallback_enabled", c.cloud_fallback_enabled);
    set_string("cloud", "cloud_priority", c.cloud_priority);
}

LoggingConfig SettingsManager::get_logging_config() const {
    LoggingConfig cfg;
    cfg.level = get_string("logging", "level", cfg.level);
    cfg.enable_file_logging = get_bool("logging", "enable_file_logging", cfg.enable_file_logging);
    cfg.enable_console = get_bool("logging", "enable_console", cfg.enable_console);
    cfg.log_path = get_string("logging", "log_path", cfg.log_path);
    return cfg;
}

void SettingsManager::set_logging_config(const LoggingConfig& c) {
    set_string("logging", "level", c.level); set_bool("logging", "enable_file_logging", c.enable_file_logging);
    set_bool("logging", "enable_console", c.enable_console); set_string("logging", "log_path", c.log_path);
}

StorageConfig SettingsManager::get_storage_config() const {
    StorageConfig cfg;
    cfg.models_dir = get_string("storage", "models_dir", cfg.models_dir);
    cfg.data_dir = get_string("storage", "data_dir", cfg.data_dir);
    cfg.kv_cache_dir = get_string("storage", "kv_cache_dir", cfg.kv_cache_dir);
    cfg.logs_dir = get_string("storage", "logs_dir", cfg.logs_dir);
    return cfg;
}

void SettingsManager::set_storage_config(const StorageConfig& c) {
    set_string("storage", "models_dir", c.models_dir); set_string("storage", "data_dir", c.data_dir);
    set_string("storage", "kv_cache_dir", c.kv_cache_dir); set_string("storage", "logs_dir", c.logs_dir);
    PathService::instance().sync_from_storage_config(c);
}
