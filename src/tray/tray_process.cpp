#include "tray_process.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <deque>

TrayProcess& TrayProcess::instance() {
    static TrayProcess inst;
    return inst;
}

bool TrayProcess::is_running() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (child_pid_ <= 0) return false;

    int status = 0;
    pid_t res = waitpid(child_pid_, &status, WNOHANG);
    if (res == 0) {
        return true; // Still running
    } else {
        child_pid_ = -1; // Exited
        return false;
    }
}

int64_t TrayProcess::get_uptime_seconds() const {
    if (child_pid_ <= 0) return 0;
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
}

bool TrayProcess::start(const std::string& base_dir) {
    if (is_running()) return true;

    std::lock_guard<std::mutex> lock(mutex_);
    std::string bin = base_dir + "/build/DenseLite";
    std::string log_path = base_dir + "/denselite.log";
    std::string work_dir = base_dir + "/build";
    const char* bin_cstr = bin.c_str();
    const char* log_cstr = log_path.c_str();
    const char* work_dir_cstr = work_dir.c_str();

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[TrayProcess] Fork failed\n";
        return false;
    }

    if (pid == 0) {
        // Child process: strictly async-signal-safe (zero heap allocations)
        setpgid(0, 0); // Detach into own process group
        int fd = open(log_cstr, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        chdir(work_dir_cstr);
        char* const args[] = { const_cast<char*>(bin_cstr), nullptr };
        execv(bin_cstr, args);
        _exit(127);
    }

    // Parent process
    child_pid_ = pid;
    start_time_ = std::chrono::steady_clock::now();
    std::cout << "[TrayProcess] DenseLite engine started with PID " << pid << std::endl;
    return true;
}

bool TrayProcess::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (child_pid_ <= 0) return true;

    pid_t pid = child_pid_;
    // Send SIGTERM to process group
    kill(-pid, SIGTERM);

    bool terminated = false;
    for (int i = 0; i < 30; ++i) {
        int status = 0;
        pid_t res = waitpid(pid, &status, WNOHANG);
        if (res != 0) {
            terminated = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!terminated) {
        kill(-pid, SIGKILL);
        int status = 0;
        waitpid(pid, &status, 0);
    }

    child_pid_ = -1;
    std::cout << "[TrayProcess] DenseLite engine (PID " << pid << ") stopped." << std::endl;
    return true;
}

bool TrayProcess::restart(const std::string& base_dir) {
    stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    return start(base_dir);
}

std::vector<std::string> TrayProcess::get_recent_logs(const std::string& base_dir, size_t max_lines) {
    std::string log_path = base_dir + "/denselite.log";
    std::vector<std::string> result;
    std::ifstream file(log_path);
    if (!file.is_open()) return result;

    std::deque<std::string> buffer;
    std::string line;
    while (std::getline(file, line)) {
        buffer.push_back(line);
        if (buffer.size() > max_lines) {
            buffer.pop_front();
        }
    }
    result.assign(buffer.begin(), buffer.end());
    return result;
}
