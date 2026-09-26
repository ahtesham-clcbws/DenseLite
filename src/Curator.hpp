#pragma once
#include <string>
#include <vector>
#include "search_result.hpp"

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
    
    // Consolidates multiple iterative results and evidence into a cohesive final output
    std::string consolidate(const std::vector<std::string>& intermediate_results,
                            const std::string& task_type,
                            const std::vector<SearchResult>& search_evidence = {});
};
