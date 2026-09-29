#include "tray_routes.hpp"
#include "model_registry_db.hpp"
#include "model_inspector.hpp"
#include "model_discovery.hpp"
#include "database_paths.hpp"
#include "path_service.hpp"
#include "../dependencies/json.hpp"

using json = nlohmann::json;

namespace TrayRoutes {

void register_model_routes(httplib::Server& svr, const std::string& base_dir) {
    svr.Get("/api/models", [base_dir](const httplib::Request&, httplib::Response& res) {
        std::string db_path = DatabasePaths::settings_db(base_dir);
        ModelDiscovery::auto_discover_and_register(base_dir, db_path);
        auto models = ModelRegistryDB::get_all_models(db_path);
        auto bindings = ModelRegistryDB::get_all_role_bindings(db_path, false);
        json j;
        j["models"] = json::array();
        for (const auto& m : models) {
            j["models"].push_back({
                {"model_id", m.model_id}, {"file_path", m.file_path},
                {"architecture", m.architecture}, {"param_size_str", m.param_size_str},
                {"quant_type", m.quant_type}, {"context_length", m.context_length},
                {"is_verified", m.is_verified}
            });
        }
        j["roles"] = json::array();
        for (const auto& b : bindings) {
            j["roles"].push_back({{"role", b.role}, {"model_id", b.model_id}, {"is_active", b.is_active}});
        }
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/models/inspect", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string path = PathService::expand_user(body.value("path", ""));
            if (!PathService::instance().is_safe_model_path(path)) {
                res.set_content(json({{"valid", false}, {"error", "Access denied: model path outside approved roots"}}).dump(), "application/json");
                return;
            }
            auto insp = ModelInspector::inspect(path);
            json j{
                {"valid", insp.is_valid}, {"error", insp.error_message},
                {"architecture", insp.architecture}, {"param_size_str", insp.param_size_str},
                {"quant_type", insp.quant_type}, {"context_length", insp.context_length},
                {"pass_edge_budget", insp.is_supported_edge_size()},
                {"compatible_roles", insp.compatible_roles}
            };
            res.set_content(j.dump(), "application/json");
        } catch (...) {
            res.set_content("{\"valid\":false,\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    svr.Post("/api/models/register", [base_dir](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string path = PathService::expand_user(body.value("path", ""));
            if (!PathService::instance().is_safe_model_path(path)) {
                res.set_content(json({{"success", false}, {"error", "Access denied: model path outside approved roots"}}).dump(), "application/json");
                return;
            }
            std::string model_id = body.value("id", "");
            auto insp = ModelInspector::inspect(path);
            if (!insp.is_valid || !insp.is_supported_edge_size()) {
                res.set_content(json({{"success", false}, {"error", "Invalid or oversized model"}}).dump(), "application/json");
                return;
            }
            std::string db_path = DatabasePaths::settings_db(base_dir);
            LocalModelRecord rec{model_id, path, insp.architecture, insp.param_count, insp.param_size_str, insp.quant_type, insp.context_length, true, 0};
            bool ok = ModelRegistryDB::register_model(db_path, rec);
            if (ok && body.contains("roles") && body["roles"].is_array()) {
                for (const auto& r : body["roles"]) {
                    ModelRegistryDB::bind_role(db_path, r.get<std::string>(), model_id, true);
                }
            }
            res.set_content(json({{"success", ok}}).dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(json({{"success", false}, {"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Post("/api/roles/bind", [base_dir](const auto& req, auto& res) {
        auto b = json::parse(req.body);
        std::string db = DatabasePaths::settings_db(base_dir);
        res.set_content(json({{"success", ModelRegistryDB::bind_role(db, b.value("role", ""), b.value("model_id", ""), true)}}).dump(), "application/json");
    });

    svr.Post("/api/roles/toggle", [base_dir](const auto& req, auto& res) {
        auto b = json::parse(req.body);
        std::string db = DatabasePaths::settings_db(base_dir);
        res.set_content(json({{"success", ModelRegistryDB::set_role_active(db, b.value("role", ""), b.value("activate", true))}}).dump(), "application/json");
    });
}

} // namespace TrayRoutes
