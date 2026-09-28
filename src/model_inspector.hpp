#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct ModelInspectionResult {
    bool is_valid{false};
    std::string architecture;
    uint64_t param_count{0};
    std::string param_size_str;
    std::string quant_type{"UNKNOWN"};
    uint32_t context_length{0};
    std::vector<std::string> compatible_roles;
    std::string error_message;

    bool is_valid_gguf() const { return is_valid; }
    bool is_supported_edge_size() const { return param_count <= 1850000000ULL; }
    bool is_role_compatible(const std::string& role) const {
        for (const auto& r : compatible_roles) {
            if (r == role) return true;
        }
        return false;
    }
};

class ModelInspector {
public:
    static ModelInspectionResult inspect(const std::string& file_path);
    static bool is_role_compatible(const std::string& arch, const std::string& role);
    static std::string format_param_count(uint64_t count);
    static std::string quant_type_to_string(uint32_t type);
};
