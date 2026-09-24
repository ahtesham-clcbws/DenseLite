#pragma once
#include <string>
#include <vector>

enum class SessionStatus {
    CREATED,
    ANALYZING,
    ROUTING,
    INFERRING,
    WAITING_FOR_TOOL,
    PROCESSING_TOOL_RESULT,
    CONTINUING,
    COMPLETED,
    FAILED,
    CANCELLED
};

class Curator {
public:
    Curator() = default;
    
    // Consolidates multiple iterative results into a cohesive final output
    std::string consolidate(const std::vector<std::string>& intermediate_results, const std::string& task_type);
};
