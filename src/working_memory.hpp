#pragma once
#include <string>
#include <vector>
#include <mutex>

struct WorkingMemorySnapshot {
    std::string objective;
    std::string current_task;
    std::string active_model;
    std::string status = "idle";
    int iteration_count = 0;
    std::vector<std::string> constraints;
};

class WorkingMemory {
public:
    WorkingMemory() = default;

    void set_objective(const std::string& obj);
    std::string get_objective() const;

    void set_current_task(const std::string& task);
    std::string get_current_task() const;

    void set_active_model(const std::string& model);
    std::string get_active_model() const;

    void set_status(const std::string& status);
    std::string get_status() const;

    void add_constraint(const std::string& constraint);
    void clear_constraints();
    std::vector<std::string> get_constraints() const;

    void increment_iteration();
    void reset_iteration();
    int get_iteration() const;

    WorkingMemorySnapshot snapshot() const;
    std::string format_context_block() const;
    void reset();

private:
    mutable std::mutex mutex_;
    WorkingMemorySnapshot state_;
};
