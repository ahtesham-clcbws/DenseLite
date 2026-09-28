#include "tray_server.hpp"
#include "tray_process.hpp"
#include "settings_manager.hpp"
#include "model_registry_db.hpp"
#include "model_inspector.hpp"
#include "model_discovery.hpp"
#include "database_paths.hpp"
#include "path_service.hpp"
#include "../dependencies/json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

TrayServer& TrayServer::instance() {
    static TrayServer inst;
    return inst;
}

static std::string read_file_content(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void TrayServer::register_routes(httplib::Server& svr, const std::string& base_dir) {
    svr.set_default_headers({{"Access-Control-Allow-Origin", "*"}, {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"}, {"Access-Control-Allow-Headers", "Content-Type"}});
    svr.set_mount_point("/", (base_dir + "/web").c_str());
    svr.Get("/favicon.ico", [base_dir](const auto&, auto& res) {
        res.set_content(read_file_content(base_dir + "/web/icon.svg"), "image/svg+xml");
    });

    svr.Get("/api/status", [](const auto&, auto& res) {
        json j{{"running", TrayProcess::instance().is_running()}, {"pid", TrayProcess::instance().get_pid()},
               {"uptime_seconds", TrayProcess::instance().get_uptime_seconds()}, {"port", SettingsManager::instance().get_server_config().port}};
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/engine/start", [base_dir](const auto&, auto& res) { res.set_content(json({{"success", TrayProcess::instance().start(base_dir)}}).dump(), "application/json"); });
    svr.Post("/api/engine/stop", [](const auto&, auto& res) { res.set_content(json({{"success", TrayProcess::instance().stop()}}).dump(), "application/json"); });
    svr.Post("/api/engine/restart", [base_dir](const auto&, auto& res) { res.set_content(json({{"success", TrayProcess::instance().restart(base_dir)}}).dump(), "application/json"); });

    svr.Get("/api/settings", [](const httplib::Request&, httplib::Response& res) {
        auto& sm = SettingsManager::instance();
        json j;
        auto srv = sm.get_server_config();
        j["server"] = {{"host", srv.host}, {"port", srv.port}, {"threads", srv.threads}, {"max_payload_mb", srv.max_payload_mb}};
        auto rc = sm.get_resource_config();
        int ram_pct = (rc.ram_budget_percent <= 1.0f) ? static_cast<int>(rc.ram_budget_percent * 100.0f) : static_cast<int>(rc.ram_budget_percent);
        j["resource"] = {{"ram_budget_percent", ram_pct}, {"enable_gpu", rc.enable_gpu}, {"vram_budget_mb", rc.vram_budget_mb}};
        auto inf = sm.get_inference_config();
        j["inference"] = {{"needle3_mode", inf.needle3_mode}, {"enable_tool_dedup", inf.enable_tool_dedup},
                          {"enable_context_injection", inf.enable_context_injection}, {"context_window", inf.context_window},
                          {"default_temperature", inf.default_temperature}, {"default_top_p", inf.default_top_p}};
        auto st = sm.get_storage_config();
        j["storage"] = {{"models_dir", st.models_dir}, {"data_dir", st.data_dir}, {"kv_cache_dir", st.kv_cache_dir}, {"logs_dir", st.logs_dir}};
        auto log = sm.get_logging_config();
        j["logging"] = {{"level", log.level}, {"enable_file_logging", log.enable_file_logging}, {"enable_console", log.enable_console}, {"log_path", log.log_path}};
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/settings", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            auto& sm = SettingsManager::instance();
            if (body.contains("inference")) {
                auto inf = sm.get_inference_config(); auto j = body["inference"];
                if (j.contains("needle3_mode")) inf.needle3_mode = j["needle3_mode"];
                if (j.contains("enable_tool_dedup")) inf.enable_tool_dedup = j["enable_tool_dedup"];
                if (j.contains("enable_context_injection")) inf.enable_context_injection = j["enable_context_injection"];
                if (j.contains("context_window")) inf.context_window = j["context_window"];
                if (j.contains("default_temperature")) inf.default_temperature = j["default_temperature"];
                if (j.contains("default_top_p")) inf.default_top_p = j["default_top_p"];
                sm.set_inference_config(inf);
            }
            if (body.contains("storage")) {
                auto st = sm.get_storage_config(); auto j = body["storage"];
                if (j.contains("models_dir")) { st.models_dir = j["models_dir"]; } if (j.contains("data_dir")) { st.data_dir = j["data_dir"]; }
                if (j.contains("kv_cache_dir")) { st.kv_cache_dir = j["kv_cache_dir"]; } if (j.contains("logs_dir")) { st.logs_dir = j["logs_dir"]; }
                sm.set_storage_config(st);
            }
            if (body.contains("server")) {
                auto srv = sm.get_server_config(); auto j = body["server"];
                if (j.contains("host")) { srv.host = j["host"]; } if (j.contains("port")) { srv.port = j["port"]; }
                if (j.contains("threads")) { srv.threads = j["threads"]; } if (j.contains("max_payload_mb")) { srv.max_payload_mb = j["max_payload_mb"]; }
                sm.set_server_config(srv);
            }
            if (body.contains("resource")) {
                auto rc = sm.get_resource_config(); auto j = body["resource"];
                if (j.contains("ram_budget_percent")) { float p = j["ram_budget_percent"]; rc.ram_budget_percent = (p > 1.0f) ? (p / 100.0f) : p; }
                if (j.contains("enable_gpu")) { rc.enable_gpu = j["enable_gpu"]; } if (j.contains("vram_budget_mb")) { rc.vram_budget_mb = j["vram_budget_mb"]; }
                sm.set_resource_config(rc);
            }
            if (body.contains("logging")) {
                auto log = sm.get_logging_config(); auto j = body["logging"];
                if (j.contains("level")) { log.level = j["level"]; } if (j.contains("enable_file_logging")) { log.enable_file_logging = j["enable_file_logging"]; }
                if (j.contains("enable_console")) { log.enable_console = j["enable_console"]; } if (j.contains("log_path")) { log.log_path = j["log_path"]; }
                sm.set_logging_config(log);
            }
            res.set_content(json({{"success", true}}).dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(json({{"success", false}, {"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Get("/api/models", [base_dir](const httplib::Request&, httplib::Response& res) {
        std::string db_path = DatabasePaths::settings_db(base_dir);
        ModelDiscovery::auto_discover_and_register(base_dir, db_path);
        auto models = ModelRegistryDB::get_all_models(db_path);
        auto bindings = ModelRegistryDB::get_all_role_bindings(db_path, false);
        json j;
        j["models"] = json::array();
        for (const auto& m : models) {
            j["models"].push_back({{"model_id", m.model_id}, {"file_path", m.file_path}, {"architecture", m.architecture},
                                  {"param_size_str", m.param_size_str}, {"quant_type", m.quant_type},
                                  {"context_length", m.context_length}, {"is_verified", m.is_verified}});
        }
        for (const auto& b : bindings) j["roles"].push_back({{"role", b.role}, {"model_id", b.model_id}, {"is_active", b.is_active}});
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/models/inspect", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string path = PathService::expand_user(body.value("path", ""));
            auto insp = ModelInspector::inspect(path);
            json j{{"valid", insp.is_valid}, {"error", insp.error_message}, {"architecture", insp.architecture},
                   {"param_size_str", insp.param_size_str}, {"quant_type", insp.quant_type},
                   {"context_length", insp.context_length}, {"pass_edge_budget", insp.is_supported_edge_size()},
                   {"compatible_roles", insp.compatible_roles}};
            res.set_content(j.dump(), "application/json");
        } catch (...) {
            res.set_content("{\"valid\":false,\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    svr.Post("/api/models/register", [base_dir](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string path = PathService::expand_user(body.value("path", ""));
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
                for (const auto& r : body["roles"]) ModelRegistryDB::bind_role(db_path, r.get<std::string>(), model_id, true);
            }
            res.set_content(json({{"success", ok}}).dump(), "application/json");
        } catch (const std::exception& e) {
            res.set_content(json({{"success", false}, {"error", e.what()}}).dump(), "application/json");
        }
    });

    svr.Post("/api/roles/bind", [base_dir](const httplib::Request& req, httplib::Response& res) {
        auto body = json::parse(req.body);
        std::string db_path = DatabasePaths::settings_db(base_dir);
        bool ok = ModelRegistryDB::bind_role(db_path, body.value("role", ""), body.value("model_id", ""), true);
        res.set_content(json({{"success", ok}}).dump(), "application/json");
    });

    svr.Post("/api/roles/toggle", [base_dir](const httplib::Request& req, httplib::Response& res) {
        auto body = json::parse(req.body);
        std::string db_path = DatabasePaths::settings_db(base_dir);
        bool ok = ModelRegistryDB::set_role_active(db_path, body.value("role", ""), body.value("activate", true));
        res.set_content(json({{"success", ok}}).dump(), "application/json");
    });

    svr.Get("/api/logs/tail", [base_dir](const httplib::Request&, httplib::Response& res) {
        auto lines = TrayProcess::instance().get_recent_logs(base_dir, 150);
        res.set_content(json({{"lines", lines}}).dump(), "application/json");
    });
}

bool TrayServer::start(const std::string& base_dir, int port) {
    base_dir_ = base_dir;
    port_ = port;
    server_ = std::make_unique<httplib::Server>();
    register_routes(*server_, base_dir_);

    thread_ = std::make_unique<std::thread>([this]() {
        std::cout << "[TrayServer] Control plane listening at http://127.0.0.1:" << port_ << std::endl;
        server_->listen("127.0.0.1", port_);
    });
    return true;
}

void TrayServer::stop() {
    if (server_) server_->stop();
    if (thread_ && thread_->joinable()) thread_->join();
    server_.reset();
    thread_.reset();
}
