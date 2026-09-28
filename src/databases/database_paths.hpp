#pragma once

#include "path_service.hpp"

namespace DatabasePaths {
    inline std::string get_dir(const std::string& base_dir = "") {
        if (!base_dir.empty()) PathService::instance().set_base_dir(base_dir);
        return PathService::instance().get_database_dir();
    }

    inline void ensure_dir_exists(const std::string& base_dir = "") {
        if (!base_dir.empty()) PathService::instance().set_base_dir(base_dir);
        PathService::instance().ensure_all_directories_exist();
    }

    inline std::string settings_db(const std::string& base_dir = "") {
        if (!base_dir.empty()) PathService::instance().set_base_dir(base_dir);
        return PathService::instance().settings_db();
    }

    inline std::string state_db(const std::string& base_dir = "") {
        if (!base_dir.empty()) PathService::instance().set_base_dir(base_dir);
        return PathService::instance().state_db();
    }

    inline std::string memory_db(const std::string& base_dir = "") {
        if (!base_dir.empty()) PathService::instance().set_base_dir(base_dir);
        return PathService::instance().memory_db();
    }

    inline std::string symbols_db(const std::string& base_dir = "") {
        if (!base_dir.empty()) PathService::instance().set_base_dir(base_dir);
        return PathService::instance().symbols_db();
    }
}
