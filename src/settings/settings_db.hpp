#pragma once

#include "settings_types.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <sqlite3.h>

class SettingsDB {
public:
    SettingsDB();
    ~SettingsDB();

    SettingsDB(const SettingsDB&) = delete;
    SettingsDB& operator=(const SettingsDB&) = delete;

    bool init(const std::string& db_path);
    void close();
    bool is_open() const;

    std::string get_journal_mode();
    bool upsert(const SettingRecord& record);
    bool get(const std::string& module, const std::string& key, SettingRecord& out);
    bool get_module(const std::string& module, std::vector<SettingRecord>& out);
    bool get_all(std::vector<SettingRecord>& out);
    int count();
    int get_data_version();

private:
    sqlite3* db_{nullptr};
    std::string db_path_;
    mutable std::mutex mutex_;

    bool configure_pragmas();
    bool create_tables();
};
