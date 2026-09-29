#pragma once

#include <string>
#include <cstdint>

inline constexpr const char* DENSELITE_VERSION = "3.3.0";

struct SettingRecord {
    std::string module;
    std::string key;
    std::string value;
    std::string val_type; // "string", "int", "float", "bool"
    int64_t updated_at{0};
};

struct ServerConfig {
    std::string version{DENSELITE_VERSION};
    std::string host{"0.0.0.0"};
    int port{9501};
    int threads{4};
    int max_payload_mb{32};
    bool enable_api_auth{false};
    std::string api_secret_key{""};
    std::string cors_allowed_origins{"*"};
    int n_batch{512};
};

struct ResourceConfig {
    float ram_budget_percent{0.50f};
    float gpu_budget_percent{0.85f};
    float headroom_safety_multiplier{1.10f};
    int max_kv_tokens{65536};
    bool enable_gpu{true};
    int vram_budget_mb{2048};
};

struct InferenceConfig {
    float default_temperature{0.7f};
    float default_top_p{0.9f};
    float repeat_penalty{1.15f};
    int repeat_last_n{64};
    int top_k{40};
    float min_p{0.05f};
    int max_output_tokens{512};
    std::string needle3_mode{"modernbert"}; // "modernbert", "gpu", "avx2", "hybrid", "cloud", "off"
    bool enable_tool_dedup{true};
    int context_window{65536};
    bool enable_context_injection{true};
    std::string system_prompt{"You are DenseLite, a fast, concise programming and chat assistant."};
};

struct MultimodalConfig {
    int sd_steps{20};
    float sd_cfg_scale{7.5f};
    int sd_width{512};
    int sd_height{512};
    std::string sd_negative_prompt{"ugly, blurry, distorted, low quality"};
    std::string whisper_language{"auto"};
    int whisper_beam_size{5};
};

struct MemoryConfig {
    int search_top_k{5};
    float similarity_threshold{0.70f};
    int max_snippet_lines{60};
    std::string excluded_paths{"vendor,node_modules,storage,.git,build"};
};

struct CloudConfig {
    std::string openrouter_api_key{""};
    std::string gemini_api_key{""};
    std::string openai_api_key{""};
    bool cloud_fallback_enabled{true};
    std::string cloud_priority{"openrouter,gemini,openai"};
};

struct LoggingConfig {
    std::string level{"INFO"};
    bool enable_file_logging{true};
    bool enable_console{true};
    std::string log_path{"denselite.log"};
};

struct StorageConfig {
    std::string models_dir{"~/.denselite/models"};
    std::string data_dir{"~/.denselite/data"};
    std::string kv_cache_dir{"~/.denselite/kv_cache"};
    std::string logs_dir{"~/.denselite/logs"};
};
