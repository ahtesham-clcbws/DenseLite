#pragma once

#include <string>
#include <thread>
#include <memory>
#include "httplib.h"

class TrayServer {
public:
    static TrayServer& instance();

    bool start(const std::string& base_dir, int port = 9500);
    void stop();

private:
    TrayServer() = default;
    ~TrayServer() { stop(); }

    void register_routes(httplib::Server& svr, const std::string& base_dir);

    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<std::thread> thread_;
    std::string base_dir_;
    int port_{9500};
};
