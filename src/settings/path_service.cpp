#include "path_service.hpp"
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>

PathService& PathService::instance() {
    static PathService inst;
    return inst;
}

std::string PathService::expand_user(const std::string& path) {
    if (path.empty() || path[0] != '~') {
        return path;
    }
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) return path;

    if (path.length() == 1) return std::string(home);
    if (path[1] == '/') return std::string(home) + path.substr(1);
    return path;
}

std::string PathService::default_user_home_dir() {
    const char* env_home = std::getenv("DENSELITE_HOME");
    if (env_home && *env_home) {
        return std::string(env_home);
    }
    return expand_user("~/.denselite");
}

std::string PathService::auto_resolve_base_dir() {
    try {
        if (std::filesystem::exists("/proc/self/exe")) {
            auto exe_path = std::filesystem::read_symlink("/proc/self/exe");
            auto parent = exe_path.parent_path();
            if (parent.filename() == "build") {
                return parent.parent_path().string();
            }
            return parent.string();
        }
    } catch (...) {}
    return std::filesystem::current_path().string();
}

PathService::PathService() {
    set_base_dir(auto_resolve_base_dir());
}

void PathService::recompute_defaults() {
    database_dir_ = base_dir_ + "/src/databases";
    data_dir_ = database_dir_;
    models_dir_ = base_dir_ + "/models";
    kv_cache_dir_ = base_dir_ + "/denselite_kv_cache";
    log_path_ = base_dir_ + "/denselite.log";
}

void PathService::set_base_dir(const std::string& dir) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    base_dir_ = dir;
    recompute_defaults();
}

std::string PathService::get_base_dir() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return base_dir_;
}

void PathService::set_database_dir(const std::string& dir) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    database_dir_ = dir;
}

std::string PathService::get_database_dir() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return database_dir_;
}

void PathService::set_data_dir(const std::string& dir) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    data_dir_ = dir;
}

std::string PathService::get_data_dir() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return data_dir_;
}

std::string PathService::settings_db() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return database_dir_ + "/denselite_settings.db";
}

std::string PathService::state_db() const {
    return settings_db();
}

std::string PathService::memory_db() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return database_dir_ + "/denselite_memory.db";
}

std::string PathService::symbols_db() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return database_dir_ + "/denselite_symbols.db";
}

void PathService::set_models_dir(const std::string& dir) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    models_dir_ = dir;
}

std::string PathService::get_models_dir() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return models_dir_;
}

void PathService::set_kv_cache_dir(const std::string& dir) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    kv_cache_dir_ = dir;
}

std::string PathService::get_kv_cache_dir() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return kv_cache_dir_;
}

void PathService::set_log_path(const std::string& path) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    log_path_ = path;
}

std::string PathService::get_log_path() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return log_path_;
}

std::string PathService::env_file() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return base_dir_ + "/.env";
}

void PathService::sync_from_storage_config(const StorageConfig& cfg) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (!cfg.models_dir.empty()) {
        models_dir_ = expand_user(cfg.models_dir);
    }
    if (!cfg.data_dir.empty()) {
        data_dir_ = expand_user(cfg.data_dir);
    }
    if (!cfg.kv_cache_dir.empty()) {
        kv_cache_dir_ = expand_user(cfg.kv_cache_dir);
    }
    if (!cfg.logs_dir.empty()) {
        log_path_ = expand_user(cfg.logs_dir) + "/denselite.log";
    }
}

void PathService::ensure_all_directories_exist() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::error_code ec;
    std::filesystem::create_directories(database_dir_, ec);
    if (!data_dir_.empty()) std::filesystem::create_directories(data_dir_, ec);
    std::filesystem::create_directories(models_dir_, ec);
    std::filesystem::create_directories(kv_cache_dir_, ec);
    if (!log_path_.empty()) {
        std::filesystem::create_directories(std::filesystem::path(log_path_).parent_path(), ec);
    }
}
