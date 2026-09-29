#pragma once

#include "settings_types.hpp"
#include <string>
#include <mutex>
#include <shared_mutex>
#include <filesystem>

class PathService {
public:
    static PathService& instance();

    PathService();
    ~PathService() = default;

    PathService(const PathService&) = delete;
    PathService& operator=(const PathService&) = delete;

    // Helpers & Discovery
    static std::string expand_user(const std::string& path);
    static std::string default_user_home_dir();
    static std::string auto_resolve_base_dir();
    static std::string xdg_config_home();
    static std::string xdg_data_home();
    static std::string xdg_cache_home();

    // Base Directory
    void set_base_dir(const std::string& dir);
    std::string get_base_dir() const;

    // Database Paths
    void set_database_dir(const std::string& dir);
    std::string get_database_dir() const;
    void set_data_dir(const std::string& dir);
    std::string get_data_dir() const;

    std::string settings_db() const;
    std::string state_db() const;
    std::string memory_db() const;
    std::string symbols_db() const;

    // Models & Storage Paths
    void set_models_dir(const std::string& dir);
    std::string get_models_dir() const;

    void set_kv_cache_dir(const std::string& dir);
    std::string get_kv_cache_dir() const;

    void set_log_path(const std::string& path);
    std::string get_log_path() const;

    std::string env_file() const;

    // Security & Path Containment (IDs 157, 158, 172, 173)
    bool is_safe_model_path(const std::string& path) const;

    // Sync from database StorageConfig
    void sync_from_storage_config(const StorageConfig& cfg);

    // Directory creation guards
    void ensure_all_directories_exist() const;

private:
    mutable std::shared_mutex mutex_;
    std::string base_dir_;
    std::string database_dir_;
    std::string data_dir_;
    std::string models_dir_;
    std::string kv_cache_dir_;
    std::string log_path_;

    void recompute_defaults();
};
