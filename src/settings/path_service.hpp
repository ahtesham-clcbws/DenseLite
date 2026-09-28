#pragma once

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

    // Base Directory
    void set_base_dir(const std::string& dir);
    std::string get_base_dir() const;
    static std::string auto_resolve_base_dir();

    // Database Paths
    void set_database_dir(const std::string& dir);
    std::string get_database_dir() const;
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

    // Directory creation guards
    void ensure_all_directories_exist() const;

private:
    mutable std::shared_mutex mutex_;
    std::string base_dir_;
    std::string database_dir_;
    std::string models_dir_;
    std::string kv_cache_dir_;
    std::string log_path_;

    void recompute_defaults();
};
