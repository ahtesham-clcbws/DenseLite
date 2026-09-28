#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <sys/types.h>

class TrayProcess {
public:
    static TrayProcess& instance();

    bool is_running();
    pid_t get_pid() const { return child_pid_; }
    int64_t get_uptime_seconds() const;

    bool start(const std::string& base_dir);
    bool stop();
    bool restart(const std::string& base_dir);

    std::vector<std::string> get_recent_logs(const std::string& base_dir, size_t max_lines = 120);

private:
    TrayProcess() = default;
    ~TrayProcess() { stop(); }

    pid_t child_pid_{-1};
    std::chrono::steady_clock::time_point start_time_;
    std::mutex mutex_;
};
