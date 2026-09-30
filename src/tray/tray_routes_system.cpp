#include "tray_routes.hpp"
#include "tray_process.hpp"
#include "settings_manager.hpp"
#include "path_service.hpp"
#include "database_paths.hpp"
#include "database_migrator.hpp"
#include "../dependencies/json.hpp"
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <sqlite3.h>
#ifdef __APPLE__
#include <libproc.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

using json = nlohmann::json;

static double get_proc_rss_mb() {
    pid_t child_pid = TrayProcess::instance().get_pid();
    pid_t target_pid = (child_pid > 0) ? child_pid : getpid();
#ifdef __APPLE__
    struct proc_taskinfo info;
    if (proc_pidinfo(target_pid, PROC_PIDTASKINFO, 0, &info, sizeof(info)) == sizeof(info)) {
        return info.pti_resident_size / (1024.0 * 1024.0);
    }
    return 0.0;
#else
    std::string path = "/proc/" + std::to_string(target_pid) + "/statm";
    std::ifstream statm(path);
    long pages = 0;
    if (statm >> pages >> pages) {
        long page_size = sysconf(_SC_PAGESIZE);
        return (pages * page_size) / (1024.0 * 1024.0);
    }
    return 0.0;
#endif
}

static double get_system_load() {
#ifdef __APPLE__
    struct loadavg info;
    size_t size = sizeof(info);
    if (sysctlbyname("vm.loadavg", &info, &size, NULL, 0) == 0) {
        return (double)info.ldavg[0] / info.fscale;
    }
    return 0.0;
#else
    double load = 0.0;
    std::ifstream loadavg("/proc/loadavg");
    if (loadavg >> load) return load;
    return 0.0;
#endif
}

static int get_active_kv_count() {
    int count = 0;
    std::string kv_dir = PathService::expand_user(SettingsManager::instance().get_storage_config().kv_cache_dir);
    if (std::filesystem::exists(kv_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(kv_dir)) {
            if (entry.path().extension() == ".kv") count++;
        }
    }
    return count;
}

static int count_table_rows(const std::string& db_path, const std::string& tbl) {
    if (!std::filesystem::exists(db_path)) return 0;
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }
    std::string sql = "SELECT count(*) FROM " + tbl + ";";
    sqlite3_stmt* stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return count;
}

namespace TrayRoutes {

void register_system_routes(httplib::Server& svr, const std::string& base_dir) {
    svr.Get("/api/status", [](const auto&, auto& res) {
        json j{
            {"running", TrayProcess::instance().is_running()},
            {"pid", TrayProcess::instance().get_pid()},
            {"uptime_seconds", TrayProcess::instance().get_uptime_seconds()},
            {"port", SettingsManager::instance().get_server_config().port},
            {"host_ram_mb", static_cast<int>(get_proc_rss_mb())},
            {"host_cpu_load", get_system_load()},
            {"active_kv_sessions", get_active_kv_count()}
        };
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/system/stats", [base_dir](const auto&, auto& res) {
        std::string s_db = DatabasePaths::settings_db(base_dir);
        std::string m_db = DatabasePaths::memory_db(base_dir);
        std::string sym_db = DatabasePaths::symbols_db(base_dir);
        json j{
            {"settings_rows", count_table_rows(s_db, "system_settings")},
            {"registered_models", count_table_rows(s_db, "local_models")},
            {"role_bindings", count_table_rows(s_db, "role_bindings")},
            {"memory_records", count_table_rows(m_db, "persistent_memory")},
            {"indexed_symbols", count_table_rows(sym_db, "symbol_index")}
        };
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/engine/start", [base_dir](const auto&, auto& res) {
        res.set_content(json({{"success", TrayProcess::instance().start(base_dir)}}).dump(), "application/json");
    });
    svr.Post("/api/engine/stop", [](const auto&, auto& res) {
        res.set_content(json({{"success", TrayProcess::instance().stop()}}).dump(), "application/json");
    });
    svr.Post("/api/engine/restart", [base_dir](const auto&, auto& res) {
        res.set_content(json({{"success", TrayProcess::instance().restart(base_dir)}}).dump(), "application/json");
    });

    svr.Post("/api/system/purge-cache", [base_dir](const auto&, auto& res) {
        res.set_content(json({{"success", DatabaseMigrator::purge_kv_cache(base_dir)}}).dump(), "application/json");
    });
    svr.Post("/api/system/vacuum", [base_dir](const auto&, auto& res) {
        res.set_content(json({{"success", DatabaseMigrator::vacuum_databases(base_dir)}}).dump(), "application/json");
    });
    svr.Get("/api/logs/tail", [base_dir](const auto&, auto& res) {
        res.set_content(json({{"lines", TrayProcess::instance().get_recent_logs(base_dir, 150)}}).dump(), "application/json");
    });
}

} // namespace TrayRoutes
