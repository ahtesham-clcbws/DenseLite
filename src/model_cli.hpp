#pragma once

#include <string>

class ModelCli {
public:
    // Returns true if a CLI command was handled and the server should exit
    static bool handle_cli(int argc, char** argv, const std::string& base_dir);

private:
    static bool handle_inspect(const std::string& filepath);
    static bool handle_register(int argc, char** argv, const std::string& base_dir);
    static bool handle_list(const std::string& base_dir);
    static bool handle_bind_role(int argc, char** argv, const std::string& base_dir);
    static bool handle_set_role(const std::string& role, bool active, const std::string& base_dir);
};
