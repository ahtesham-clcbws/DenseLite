#pragma once
#include <string>
#include "httplib.h"

namespace TrayRoutes {
    void register_settings_routes(httplib::Server& svr, const std::string& base_dir);
    void register_system_routes(httplib::Server& svr, const std::string& base_dir);
    void register_model_routes(httplib::Server& svr, const std::string& base_dir);
}
