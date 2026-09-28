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
};

struct ResourceConfig {
    float ram_budget_percent{0.45f};
    int max_kv_tokens{65536};
    bool enable_gpu{true};
    int vram_budget_mb{2048};
};

struct InferenceConfig {
    float default_temperature{0.7f};
    float default_top_p{0.9f};
    std::string needle3_mode{"hybrid"}; // "gpu", "avx2", "hybrid", "cloud", "off"
    bool enable_tool_dedup{true};
    int context_window{65536};
    bool enable_context_injection{true};
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
