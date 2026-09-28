#include "settings_manager.hpp"
#include "path_service.hpp"
#include <chrono>

SettingsManager& SettingsManager::instance() { static SettingsManager inst; return inst; }
SettingsManager::SettingsManager() = default;
SettingsManager::~SettingsManager() = default;

std::string SettingsManager::make_cache_key(const std::string& mod, const std::string& key) {
    return mod + ":" + key;
}

bool SettingsManager::init(const std::string& db_path) {
    if (!db_.init(db_path)) return false;
    seed_defaults_if_empty();
    reload();
    last_data_version_ = db_.get_data_version();
    PathService::instance().sync_from_storage_config(get_storage_config());
    return true;
}

std::string SettingsManager::get_journal_mode() { return db_.get_journal_mode(); }

void SettingsManager::check_and_reload() const {
    int cur_ver = db_.get_data_version();
    if (cur_ver != last_data_version_) {
        std::vector<SettingRecord> all;
        if (db_.get_all(all)) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            cache_.clear();
            for (const auto& rec : all) cache_[make_cache_key(rec.module, rec.key)] = rec;
            last_data_version_ = cur_ver;

            auto get_c = [this](const std::string& k, const std::string& d) {
                auto it = cache_.find("storage:" + k); return (it != cache_.end()) ? it->second.value : d;
            };
            StorageConfig sc;
            sc.models_dir = get_c("models_dir", sc.models_dir); sc.data_dir = get_c("data_dir", sc.data_dir);
            sc.kv_cache_dir = get_c("kv_cache_dir", sc.kv_cache_dir); sc.logs_dir = get_c("logs_dir", sc.logs_dir);
            PathService::instance().sync_from_storage_config(sc);
        }
    }
}

void SettingsManager::reload() {
    std::vector<SettingRecord> all;
    if (db_.get_all(all)) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_.clear();
        for (const auto& rec : all) cache_[make_cache_key(rec.module, rec.key)] = rec;
        last_data_version_ = db_.get_data_version();
    }
}

void SettingsManager::seed_defaults_if_empty() {
    if (db_.count() > 0) return;
    set_server_config(ServerConfig{}); set_resource_config(ResourceConfig{});
    set_inference_config(InferenceConfig{}); set_multimodal_config(MultimodalConfig{});
    set_memory_config(MemoryConfig{}); set_cloud_config(CloudConfig{});
    set_logging_config(LoggingConfig{}); set_storage_config(StorageConfig{});
}

std::string SettingsManager::get_string(const std::string& mod, const std::string& key, const std::string& def) const {
    check_and_reload();
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(make_cache_key(mod, key));
    return (it != cache_.end()) ? it->second.value : def;
}

int SettingsManager::get_int(const std::string& mod, const std::string& key, int def) const {
    check_and_reload();
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(make_cache_key(mod, key));
    if (it != cache_.end()) { try { return std::stoi(it->second.value); } catch (...) {} }
    return def;
}

float SettingsManager::get_float(const std::string& mod, const std::string& key, float def) const {
    check_and_reload();
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(make_cache_key(mod, key));
    if (it != cache_.end()) { try { return std::stof(it->second.value); } catch (...) {} }
    return def;
}

bool SettingsManager::get_bool(const std::string& mod, const std::string& key, bool def) const {
    check_and_reload();
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(make_cache_key(mod, key));
    return (it != cache_.end()) ? (it->second.value == "true" || it->second.value == "1") : def;
}

static void update_setting(SettingsDB& db, std::shared_mutex& mutex,
                           std::unordered_map<std::string, SettingRecord>& cache,
                           int& last_ver, const std::string& mod, const std::string& key,
                           const std::string& val, const std::string& type) {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch()).count();
    SettingRecord rec{mod, key, val, type, now};
    if (db.upsert(rec)) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        cache[mod + ":" + key] = rec;
        last_ver = db.get_data_version();
    }
}

void SettingsManager::set_string(const std::string& m, const std::string& k, const std::string& v) { update_setting(db_, mutex_, cache_, last_data_version_, m, k, v, "string"); }
void SettingsManager::set_int(const std::string& m, const std::string& k, int v) { update_setting(db_, mutex_, cache_, last_data_version_, m, k, std::to_string(v), "int"); }
void SettingsManager::set_float(const std::string& m, const std::string& k, float v) { update_setting(db_, mutex_, cache_, last_data_version_, m, k, std::to_string(v), "float"); }
void SettingsManager::set_bool(const std::string& m, const std::string& k, bool v) { update_setting(db_, mutex_, cache_, last_data_version_, m, k, v ? "true" : "false", "bool"); }

std::string SettingsManager::get_version() const { return DENSELITE_VERSION; }
