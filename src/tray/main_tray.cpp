#include "tray_app.hpp"
#include "database_migrator.hpp"
#include "database_paths.hpp"
#include "settings_manager.hpp"
#include "path_service.hpp"
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    std::string base_dir = PathService::instance().get_base_dir();

    DatabaseMigrator::ensure_all_databases_ready(base_dir);
    SettingsManager::instance().init(DatabasePaths::settings_db(base_dir));

    return TrayApp::instance().run(argc, argv, base_dir);
}
