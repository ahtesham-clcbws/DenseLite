#include "Curator.hpp"
#include <sstream>
#include "../dependencies/json.hpp"

using json = nlohmann::json;

static std::string extract_text(const std::string& raw) {
    try {
        json j = json::parse(raw);
        if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
            const auto& choice = j["choices"][0];
            if (choice.contains("message") && choice["message"].is_object() && choice["message"].contains("content")) {
                if (choice["message"]["content"].is_string()) {
                    return choice["message"]["content"].get<std::string>();
                }
            }
            if (choice.contains("delta") && choice["delta"].is_object() && choice["delta"].contains("content")) {
                if (choice["delta"]["content"].is_string()) {
                    return choice["delta"]["content"].get<std::string>();
                }
            }
        }
        if (j.contains("candidates") && j["candidates"].is_array() && !j["candidates"].empty()) {
            const auto& cand = j["candidates"][0];
            if (cand.contains("content") && cand["content"].contains("parts") && cand["content"]["parts"].is_array()) {
                return cand["content"]["parts"][0]["text"].get<std::string>();
            }
        }
    } catch (...) {}
    return raw;
}

std::string Curator::consolidate(
    const std::vector<std::string>& intermediate_results,
    const std::string& task_type,
    const std::vector<SearchResult>& search_evidence) {

    if (intermediate_results.empty()) {
        return "";
    }

    std::stringstream consolidated;

    // Single iteration without extra evidence: direct return
    if (intermediate_results.size() == 1 && search_evidence.empty()) {
        return extract_text(intermediate_results[0]);
    }

    if (intermediate_results.size() == 1) {
        consolidated << extract_text(intermediate_results[0]);
    } else {
        consolidated << "### Task Consolidation (" << task_type << ")\n";
        for (size_t i = 0; i < intermediate_results.size(); ++i) {
            consolidated << "\n--- Iteration " << (i + 1) << " ---\n";
            consolidated << extract_text(intermediate_results[i]) << "\n";
        }
    }

    if (!search_evidence.empty()) {
        consolidated << "\n\n### Supporting Evidence Citations\n";
        for (const auto& ev : search_evidence) {
            if (!ev.symbol_name.empty()) {
                consolidated << "- **" << ev.symbol_name << "** (`" << ev.file_path << "`)\n";
            } else if (!ev.file_path.empty()) {
                consolidated << "- `" << ev.file_path << "`\n";
            }
        }
    }

    return consolidated.str();
}
