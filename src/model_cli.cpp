#include "model_cli.hpp"
#include "model_inspector.hpp"
#include "model_registry_db.hpp"
#include "model_discovery.hpp"
#include "database_paths.hpp"
#include "path_service.hpp"
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>

static std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

bool ModelCli::handle_cli(int argc, char** argv, const std::string& base_dir) {
    if (argc < 2) return false;
    std::string cmd = argv[1];

    if (cmd == "--inspect-model" && argc >= 3) return handle_inspect(argv[2]);
    if (cmd == "--register-model" && argc >= 3) return handle_register(argc, argv, base_dir);
    if (cmd == "--list-models") return handle_list(base_dir);
    if (cmd == "--bind-role" && argc >= 3) return handle_bind_role(argc, argv, base_dir);
    if (cmd == "--activate-role" && argc >= 3) return handle_set_role(argv[2], true, base_dir);
    if (cmd == "--deactivate-role" && argc >= 3) return handle_set_role(argv[2], false, base_dir);
    return false;
}

bool ModelCli::handle_inspect(const std::string& filepath) {
    std::string path = PathService::expand_user(filepath);
    auto res = ModelInspector::inspect(path);

    std::cout << "\n================ DenseLite Model Inspection ================\n";
    std::cout << " File:           " << path << "\n";
    if (!res.is_valid) {
        std::cout << " Status:         INVALID (" << res.error_message << ")\n";
        std::cout << "============================================================\n" << std::endl;
        return true;
    }

    std::cout << " Architecture:   " << res.architecture << "\n";
    std::cout << " Parameters:     " << res.param_size_str 
              << " (" << res.param_count << " raw)\n";
    std::cout << " Quantization:   " << res.quant_type << "\n";
    std::cout << " Context Length: " << res.context_length << "\n";
    std::cout << " Edge Size Cap:  " << (res.is_supported_edge_size() ? "PASS (<= 1.85B)" : "FAIL (> 1.85B)") << "\n";
    std::cout << " Compatible:     ";
    for (size_t i = 0; i < res.compatible_roles.size(); ++i) {
        std::cout << res.compatible_roles[i] << (i + 1 < res.compatible_roles.size() ? ", " : "");
    }
    std::cout << "\n============================================================\n" << std::endl;
    return true;
}

bool ModelCli::handle_register(int argc, char** argv, const std::string& base_dir) {
    std::string filepath = PathService::expand_user(argv[2]);
    std::string model_id;
    std::vector<std::string> roles;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--id" && i + 1 < argc) {
            model_id = argv[++i];
        } else if (arg == "--roles" && i + 1 < argc) {
            roles = split(argv[++i], ',');
        }
    }

    if (model_id.empty()) {
        std::filesystem::path p(filepath);
        model_id = p.stem().string();
    }

    auto insp = ModelInspector::inspect(filepath);
    if (!insp.is_valid) {
        std::cerr << "[Registry Error] Cannot register invalid GGUF model: " << insp.error_message << std::endl;
        return true;
    }
    if (!insp.is_supported_edge_size()) {
        std::cerr << "[Registry Error] Model parameter count (" << insp.param_size_str 
                  << ") exceeds DenseLite edge budget (<= 1.85B)." << std::endl;
        return true;
    }

    for (const auto& r : roles) {
        if (!insp.is_role_compatible(r)) {
            std::cerr << "[Registry Error] Model architecture '" << insp.architecture 
                      << "' is incompatible with role: " << r << std::endl;
            return true;
        }
    }

    std::string db_path = DatabasePaths::settings_db(base_dir);
    LocalModelRecord rec;
    rec.model_id = model_id;
    rec.file_path = filepath;
    rec.architecture = insp.architecture;
    rec.param_count = insp.param_count;
    rec.param_size_str = insp.param_size_str;
    rec.quant_type = insp.quant_type;
    rec.context_length = insp.context_length;
    rec.is_verified = true;

    if (!ModelRegistryDB::register_model(db_path, rec)) {
        std::cerr << "[Registry Error] Failed to persist model record in " << db_path << std::endl;
        return true;
    }

    for (const auto& r : roles) {
        ModelRegistryDB::bind_role(db_path, r, model_id, true);
    }

    std::cout << "[Registry] Model '" << model_id << "' registered successfully.\n"
              << "  Path:  " << filepath << "\n"
              << "  Arch:  " << insp.architecture << " (" << insp.param_size_str << ")\n"
              << "  Roles: ";
    for (const auto& r : roles) std::cout << r << " ";
    std::cout << std::endl;
    return true;
}

bool ModelCli::handle_list(const std::string& base_dir) {
    std::string db_path = DatabasePaths::settings_db(base_dir);
    ModelDiscovery::auto_discover_and_register(base_dir, db_path);
    auto models = ModelRegistryDB::get_all_models(db_path);
    auto bindings = ModelRegistryDB::get_all_role_bindings(db_path);

    std::cout << "\n================ DenseLite Registered Models ================\n";
    if (models.empty()) {
        std::cout << " (No local models registered in " << db_path << ")\n";
    }
    for (const auto& m : models) {
        std::cout << " ID:           " << m.model_id << "\n"
                  << " File:         " << m.file_path << "\n"
                  << " Architecture: " << m.architecture << "\n"
                  << " Parameters:   " << m.param_size_str << "\n"
                  << " Quantization: " << m.quant_type << "\n"
                  << " Context:      " << m.context_length << "\n"
                  << " Status:       " << (m.is_verified ? "VERIFIED" : "UNVERIFIED") << "\n"
                  << " ------------------------------------------------------------\n";
    }

    std::cout << "\n================ Active Role Bindings ================\n";
    if (bindings.empty()) {
        std::cout << " (No active roles assigned. Using fallback .env models)\n";
    }
    for (const auto& b : bindings) {
        std::cout << " Role: [" << std::left << std::setw(12) << b.role 
                  << "] -> Model: " << b.model_id 
                  << " (Active: " << (b.is_active ? "YES" : "NO") << ")\n";
    }
    std::cout << "=======================================================\n" << std::endl;
    return true;
}

bool ModelCli::handle_bind_role(int argc, char** argv, const std::string& base_dir) {
    std::string role = argv[2], model_id;
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) model_id = argv[++i];
        else if (model_id.empty() && arg[0] != '-') model_id = arg;
    }
    if (model_id.empty()) {
        std::cerr << "[Registry Error] Usage: --bind-role <role> --model <model_id>\n";
        return true;
    }
    std::string db_path = DatabasePaths::settings_db(base_dir);
    LocalModelRecord rec;
    if (!ModelRegistryDB::get_model(db_path, model_id, rec)) {
        std::cerr << "[Registry Error] Model '" << model_id << "' is not registered.\n";
        return true;
    }
    ModelRegistryDB::bind_role(db_path, role, model_id, true);
    std::cout << "[Registry] Bound role '" << role << "' -> model '" << model_id << "' (active)\n";
    return true;
}

bool ModelCli::handle_set_role(const std::string& role, bool active, const std::string& base_dir) {
    std::string db_path = DatabasePaths::settings_db(base_dir);
    if (!ModelRegistryDB::set_role_active(db_path, role, active)) {
        std::cerr << "[Registry Error] Role '" << role << "' not found.\n";
        return true;
    }
    std::cout << "[Registry] Role '" << role << "' is now " << (active ? "ACTIVE" : "INACTIVE") << ".\n";
    return true;
}
