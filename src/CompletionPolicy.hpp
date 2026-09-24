#pragma once
#include <string>
#include <vector>

class CompletionPolicy {
public:
    static bool is_acceptable(const std::string& task_type, const std::string& model_output, const std::vector<std::string>& session_history);
};
