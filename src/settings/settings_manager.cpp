#include "settings_manager.hpp"
#include <chrono>

SettingsManager& SettingsManager::instance() {
    static SettingsManager inst;
    return inst;
}

SettingsManager::SettingsManager() = default;
SettingsManager::~SettingsManager() = default;

std::string SettingsManager::make_cache_key(const std::string& module, const std::string& key) {
    return module + ":" + key;
}

bool SettingsManager::init(const std::string& db_path) {
    if (!db_.init(db_path)) return false;
    seed_defaults_if_empty();
    reload();
    return true;
}

std::string SettingsManager::get_journal_mode() {
    return db_.get_journal_mode();
}

void SettingsManager::reload() {
    std::vector<SettingRecord> all;
    if (db_.get_all(all)) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_.clear();
        for (const auto& rec : all) {
            cache_[make_cache_key(rec.module, rec.key)] = rec;
        }
    }
}

void SettingsManager::seed_defaults_if_empty() {
    if (db_.count() > 0) return;
    set_server_config(ServerConfig{});
    set_resource_config(ResourceConfig{});
    set_inference_config(InferenceConfig{});
    set_logging_config(LoggingConfig{});
}

std::string SettingsManager::get_string(const std::string& mod, const std::string& key, const std::string& def) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(make_cache_key(mod, key));
    return (it != cache_.end()) ? it->second.value : def;
}

int SettingsManager::get_int(const std::string& mod, const std::string& key, int def) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(make_cache_key(mod, key));
    if (it != cache_.end()) {
        try { return std::stoi(it->second.value); } catch (...) {}
    }
    return def;
}

float SettingsManager::get_float(const std::string& mod, const std::string& key, float def) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(make_cache_key(mod, key));
    if (it != cache_.end()) {
        try { return std::stof(it->second.value); } catch (...) {}
    }
    return def;
}

bool SettingsManager::get_bool(const std::string& mod, const std::string& key, bool def) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(make_cache_key(mod, key));
    if (it != cache_.end()) {
        return (it->second.value == "true" || it->second.value == "1");
    }
    return def;
}

void SettingsManager::set_string(const std::string& mod, const std::string& key, const std::string& val) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count();
    SettingRecord rec{mod, key, val, "string", now};
    if (db_.upsert(rec)) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_[make_cache_key(mod, key)] = rec;
    }
}

void SettingsManager::set_int(const std::string& mod, const std::string& key, int val) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count();
    SettingRecord rec{mod, key, std::to_string(val), "int", now};
    if (db_.upsert(rec)) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_[make_cache_key(mod, key)] = rec;
    }
}

void SettingsManager::set_float(const std::string& mod, const std::string& key, float val) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count();
    SettingRecord rec{mod, key, std::to_string(val), "float", now};
    if (db_.upsert(rec)) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_[make_cache_key(mod, key)] = rec;
    }
}

void SettingsManager::set_bool(const std::string& mod, const std::string& key, bool val) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count();
    SettingRecord rec{mod, key, val ? "true" : "false", "bool", now};
    if (db_.upsert(rec)) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_[make_cache_key(mod, key)] = rec;
    }
}

std::string SettingsManager::get_version() const {
    return DENSELITE_VERSION;
}

ServerConfig SettingsManager::get_server_config() const {
    ServerConfig cfg;
    cfg.version = DENSELITE_VERSION;
    cfg.host = get_string("server", "host", cfg.host);
    cfg.port = get_int("server", "port", cfg.port);
    cfg.threads = get_int("server", "threads", cfg.threads);
    cfg.max_payload_mb = get_int("server", "max_payload_mb", cfg.max_payload_mb);
    return cfg;
}

void SettingsManager::set_server_config(const ServerConfig& cfg) {
    set_string("server", "host", cfg.host);
    set_int("server", "port", cfg.port);
    set_int("server", "threads", cfg.threads);
    set_int("server", "max_payload_mb", cfg.max_payload_mb);
}

ResourceConfig SettingsManager::get_resource_config() const {
    ResourceConfig cfg;
    cfg.ram_budget_percent = get_float("resource", "ram_budget_percent", cfg.ram_budget_percent);
    cfg.max_kv_tokens = get_int("resource", "max_kv_tokens", cfg.max_kv_tokens);
    cfg.enable_gpu = get_bool("resource", "enable_gpu", cfg.enable_gpu);
    cfg.vram_budget_mb = get_int("resource", "vram_budget_mb", cfg.vram_budget_mb);
    return cfg;
}

void SettingsManager::set_resource_config(const ResourceConfig& cfg) {
    set_float("resource", "ram_budget_percent", cfg.ram_budget_percent);
    set_int("resource", "max_kv_tokens", cfg.max_kv_tokens);
    set_bool("resource", "enable_gpu", cfg.enable_gpu);
    set_int("resource", "vram_budget_mb", cfg.vram_budget_mb);
}

InferenceConfig SettingsManager::get_inference_config() const {
    InferenceConfig cfg;
    cfg.default_temperature = get_float("inference", "default_temperature", cfg.default_temperature);
    cfg.default_top_p = get_float("inference", "default_top_p", cfg.default_top_p);
    cfg.needle3_mode = get_string("inference", "needle3_mode", cfg.needle3_mode);
    cfg.enable_tool_dedup = get_bool("inference", "enable_tool_dedup", cfg.enable_tool_dedup);
    cfg.context_window = get_int("inference", "context_window", cfg.context_window);
    return cfg;
}

void SettingsManager::set_inference_config(const InferenceConfig& cfg) {
    set_float("inference", "default_temperature", cfg.default_temperature);
    set_float("inference", "default_top_p", cfg.default_top_p);
    set_string("inference", "needle3_mode", cfg.needle3_mode);
    set_bool("inference", "enable_tool_dedup", cfg.enable_tool_dedup);
    set_int("inference", "context_window", cfg.context_window);
}

LoggingConfig SettingsManager::get_logging_config() const {
    LoggingConfig cfg;
    cfg.level = get_string("logging", "level", cfg.level);
    cfg.enable_file_logging = get_bool("logging", "enable_file_logging", cfg.enable_file_logging);
    cfg.enable_console = get_bool("logging", "enable_console", cfg.enable_console);
    cfg.log_path = get_string("logging", "log_path", cfg.log_path);
    return cfg;
}

void SettingsManager::set_logging_config(const LoggingConfig& cfg) {
    set_string("logging", "level", cfg.level);
    set_bool("logging", "enable_file_logging", cfg.enable_file_logging);
    set_bool("logging", "enable_console", cfg.enable_console);
    set_string("logging", "log_path", cfg.log_path);
}
