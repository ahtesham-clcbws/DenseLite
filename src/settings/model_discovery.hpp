#pragma once

#include <string>

class ModelDiscovery {
public:
    static std::string resolve_model_path(const std::string& raw_path, const std::string& base_dir = "");
    static int auto_discover_and_register(const std::string& base_dir, const std::string& db_path);
};
