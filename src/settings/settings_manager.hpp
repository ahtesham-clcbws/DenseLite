#pragma once

#include "settings_types.hpp"
#include "settings_db.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <memory>

class SettingsManager {
public:
    static SettingsManager& instance();

    SettingsManager();
    ~SettingsManager();

    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    bool init(const std::string& db_path);
    void reload();
    void seed_defaults_if_empty();
    std::string get_journal_mode();
    std::string get_version() const;

    // Primitive Type-Safe Accessors
    std::string get_string(const std::string& module, const std::string& key, const std::string& default_val = "") const;
    int get_int(const std::string& module, const std::string& key, int default_val = 0) const;
    float get_float(const std::string& module, const std::string& key, float default_val = 0.0f) const;
    bool get_bool(const std::string& module, const std::string& key, bool default_val = false) const;

    void set_string(const std::string& module, const std::string& key, const std::string& val);
    void set_int(const std::string& module, const std::string& key, int val);
    void set_float(const std::string& module, const std::string& key, float val);
    void set_bool(const std::string& module, const std::string& key, bool val);

    // High-Level DTO Accessors
    ServerConfig get_server_config() const;
    ResourceConfig get_resource_config() const;
    InferenceConfig get_inference_config() const;
    LoggingConfig get_logging_config() const;
    StorageConfig get_storage_config() const;

    void set_server_config(const ServerConfig& cfg);
    void set_resource_config(const ResourceConfig& cfg);
    void set_inference_config(const InferenceConfig& cfg);
    void set_logging_config(const LoggingConfig& cfg);
    void set_storage_config(const StorageConfig& cfg);

    void check_and_reload() const;

private:
    mutable SettingsDB db_;
    mutable std::shared_mutex mutex_;
    mutable std::unordered_map<std::string, SettingRecord> cache_;
    mutable int last_data_version_{0};

    static std::string make_cache_key(const std::string& module, const std::string& key);
};
