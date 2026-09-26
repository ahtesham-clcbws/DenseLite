#pragma once
#include <string>
#include <vector>

struct CompletionEvidence {
    bool has_output = false;
    bool has_task_type = false;
    bool tools_called = false;
    bool tool_result_received = false;
    bool artifact_produced = false;
    bool unresolved_error = false;
    bool model_says_complete = false;
    bool evidence_says_complete = false;
};

class CompletionPolicy {
public:
    static bool is_acceptable(
        const std::string& task_type,
        const std::string& model_output,
        const std::vector<std::string>& session_history,
        const CompletionEvidence& evidence = CompletionEvidence{});
};
