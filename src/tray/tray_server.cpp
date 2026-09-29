#include "tray_server.hpp"
#include "tray_routes.hpp"
#include <fstream>
#include <iostream>

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
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "http://127.0.0.1:9500"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"},
        {"Cache-Control", "no-cache, no-store, must-revalidate"}
    });

    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        std::string origin = req.get_header_value("Origin");
        if (!origin.empty() && origin.find("127.0.0.1") == std::string::npos && 
            origin.find("localhost") == std::string::npos) {
            res.status = 403;
            res.set_content("{\"error\":\"Forbidden: Cross-origin access blocked\"}", "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
    svr.set_mount_point("/", (base_dir + "/web").c_str());
    svr.Get("/favicon.ico", [base_dir](const auto&, auto& res) {
        res.set_content(read_file_content(base_dir + "/web/icon.svg"), "image/svg+xml");
    });

    TrayRoutes::register_settings_routes(svr, base_dir);
    TrayRoutes::register_system_routes(svr, base_dir);
    TrayRoutes::register_model_routes(svr, base_dir);
}

bool TrayServer::start(const std::string& base_dir, int port) {
    base_dir_ = base_dir;
    port_ = port;
    server_ = std::make_unique<httplib::Server>();
    register_routes(*server_, base_dir_);
    thread_ = std::make_unique<std::thread>([this]() {
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
