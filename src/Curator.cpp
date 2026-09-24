#include "Curator.hpp"
#include <sstream>

std::string Curator::consolidate(const std::vector<std::string>& intermediate_results, const std::string& task_type) {
    if (intermediate_results.empty()) {
        return "";
    }
    
    // For simple single-shot tasks, just return the only result
    if (intermediate_results.size() == 1) {
        return intermediate_results[0];
    }

    // For multi-turn tasks (e.g., plan -> code -> test), consolidate them.
    // In a full implementation, this might use SmolLM2 for summarization.
    std::stringstream consolidated;
    consolidated << "### Task Consolidation (" << task_type << ")\n";
    
    for (size_t i = 0; i < intermediate_results.size(); ++i) {
        consolidated << "\n--- Iteration " << (i + 1) << " ---\n";
        consolidated << intermediate_results[i] << "\n";
    }

    return consolidated.str();
}
