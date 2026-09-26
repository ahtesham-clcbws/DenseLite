#include "working_memory.hpp"
#include <sstream>

void WorkingMemory::set_objective(const std::string& obj) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.objective = obj;
}

std::string WorkingMemory::get_objective() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.objective;
}

void WorkingMemory::set_current_task(const std::string& task) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.current_task = task;
}

std::string WorkingMemory::get_current_task() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.current_task;
}

void WorkingMemory::set_active_model(const std::string& model) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.active_model = model;
}

std::string WorkingMemory::get_active_model() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.active_model;
}

void WorkingMemory::set_status(const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.status = status;
}

std::string WorkingMemory::get_status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.status;
}

void WorkingMemory::add_constraint(const std::string& constraint) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.constraints.push_back(constraint);
}

void WorkingMemory::clear_constraints() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.constraints.clear();
}

std::vector<std::string> WorkingMemory::get_constraints() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.constraints;
}

void WorkingMemory::increment_iteration() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.iteration_count++;
}

void WorkingMemory::reset_iteration() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.iteration_count = 0;
}

int WorkingMemory::get_iteration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.iteration_count;
}

WorkingMemorySnapshot WorkingMemory::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::string WorkingMemory::format_context_block() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.objective.empty() && state_.current_task.empty() && state_.constraints.empty()) {
        return "";
    }
    std::ostringstream oss;
    oss << "[Working Memory]\n";
    if (!state_.objective.empty()) {
        oss << "Objective: " << state_.objective << "\n";
    }
    if (!state_.current_task.empty()) {
        oss << "Current Task: " << state_.current_task << "\n";
    }
    if (!state_.active_model.empty()) {
        oss << "Model: " << state_.active_model << "\n";
    }
    if (!state_.constraints.empty()) {
        oss << "Constraints:\n";
        for (const auto& c : state_.constraints) {
            oss << " - " << c << "\n";
        }
    }
    return oss.str();
}

void WorkingMemory::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = WorkingMemorySnapshot();
}
