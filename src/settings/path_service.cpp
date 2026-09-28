#include "path_service.hpp"
#include <unistd.h>

PathService& PathService::instance() {
    static PathService inst;
    return inst;
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

void PathService::ensure_all_directories_exist() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::error_code ec;
    std::filesystem::create_directories(database_dir_, ec);
    std::filesystem::create_directories(models_dir_, ec);
    std::filesystem::create_directories(kv_cache_dir_, ec);
}
