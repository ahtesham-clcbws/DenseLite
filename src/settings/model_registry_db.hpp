#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sqlite3.h>

struct LocalModelRecord {
    std::string model_id;
    std::string file_path;
    std::string architecture;
    uint64_t param_count = 0;
    std::string param_size_str;
    std::string quant_type;
    uint32_t context_length = 0;
    bool is_verified = true;
    int64_t updated_at = 0;
};

struct ModelRoleBinding {
    std::string role;
    std::string model_id;
    bool is_active = true;
    int64_t updated_at = 0;
};

class ModelRegistryDB {
public:
    static bool register_model(const std::string& db_path, const LocalModelRecord& rec);
    static bool get_model(const std::string& db_path, const std::string& model_id, LocalModelRecord& out);
    static std::vector<LocalModelRecord> get_all_models(const std::string& db_path);
    static bool delete_model(const std::string& db_path, const std::string& model_id);

    static bool bind_role(const std::string& db_path, const std::string& role, const std::string& model_id, bool is_active = true);
    static bool set_role_active(const std::string& db_path, const std::string& role, bool is_active);
    static bool unbind_role(const std::string& db_path, const std::string& role);
    static std::vector<ModelRoleBinding> get_all_role_bindings(const std::string& db_path, bool active_only = false);
    static std::string get_model_for_role(const std::string& db_path, const std::string& role);
};
