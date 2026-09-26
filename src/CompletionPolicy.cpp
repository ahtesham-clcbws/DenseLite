#include "CompletionPolicy.hpp"
#include <algorithm>
#include <cctype>

bool CompletionPolicy::is_acceptable(
    const std::string& task_type,
    const std::string& model_output,
    const std::vector<std::string>& /*session_history*/,
    const CompletionEvidence& evidence) {

    // 1. Output must exist
    if (model_output.empty() && !evidence.has_output) {
        return false;
    }

    // 2. Unresolved error blocks completion
    if (evidence.unresolved_error) {
        return false;
    }

    // 3. If tools were called, tool results must be received
    if (evidence.tools_called && !evidence.tool_result_received) {
        return false;
    }

    // 4. Evidence says complete is authoritative
    if (evidence.evidence_says_complete) {
        return true;
    }

    // 5. Coding task invariants: "Done" or "I have finished" is NOT evidence
    std::string lower_out = model_output;
    std::transform(lower_out.begin(), lower_out.end(), lower_out.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    bool is_bare_claim = (lower_out == "done" || lower_out == "done." ||
                          lower_out == "i'm done" || lower_out == "finished");

    if (task_type == "coding") {
        if (is_bare_claim && !evidence.artifact_produced) {
            return false;
        }
        // Coding tasks are complete if code block exists, tool was run, or artifact produced
        if (model_output.find("```") != std::string::npos ||
            evidence.artifact_produced ||
            evidence.tool_result_received) {
            return true;
        }
    }

    // 6. General answers are acceptable if non-trivial
    return !is_bare_claim;
}
