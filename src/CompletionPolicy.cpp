#include "CompletionPolicy.hpp"

bool CompletionPolicy::is_acceptable(const std::string& task_type, const std::string& model_output, const std::vector<std::string>& session_history) {
    // If it's a coding task, ensure we actually ran a tool at some point before accepting a simple "Done"
    if (task_type == "coding" && session_history.empty()) {
        return false;
    }
    return true;
}
