#include "memory_recall.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>

MemoryRecall::MemoryRecall(PersistentMemory* persistent_mem, MemoryStore* memory_store)
    : persistent_mem_(persistent_mem), memory_store_(memory_store) {}

float MemoryRecall::compute_lexical_similarity(const std::string& query, const std::string& target) const {
    if (query.empty() || target.empty()) return 0.0f;

    std::string q_lower = query;
    std::string t_lower = target;
    std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(), ::tolower);
    std::transform(t_lower.begin(), t_lower.end(), t_lower.begin(), ::tolower);

    // Substring match
    if (t_lower.find(q_lower) != std::string::npos) {
        return 1.0f;
    }

    // Token overlap
    std::vector<std::string> q_tokens;
    std::string cur;
    for (char c : q_lower) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            cur += c;
        } else if (!cur.empty()) {
            q_tokens.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) q_tokens.push_back(cur);

    if (q_tokens.empty()) return 0.0f;

    int matches = 0;
    for (const auto& tok : q_tokens) {
        if (tok.size() >= 3 && t_lower.find(tok) != std::string::npos) {
            matches++;
        }
    }
    return static_cast<float>(matches) / static_cast<float>(q_tokens.size());
}

std::vector<RecalledMemory> MemoryRecall::recall(const std::string& query, size_t limit) {
    std::vector<RecalledMemory> results;

    // 1. Check in-memory persistent rules/conventions first
    if (persistent_mem_) {
        auto all_p = persistent_mem_->get_all();
        for (const auto& entry : all_p) {
            float score = std::max(compute_lexical_similarity(query, entry.key),
                                   compute_lexical_similarity(query, entry.value));
            if (score > 0.1f) {
                RecalledMemory rm;
                rm.entry = entry;
                rm.relevance_score = score * entry.confidence;
                rm.match_source = "persistent_cache";
                results.push_back(std::move(rm));
            }
        }
    }

    // 2. Query canonical SQLite storage
    if (memory_store_) {
        auto db_entries = memory_store_->query_memories_keyword(query, limit);
        for (const auto& entry : db_entries) {
            // Avoid duplicate keys
            bool exists = false;
            for (const auto& r : results) {
                if (r.entry.key == entry.key) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                float score = std::max(compute_lexical_similarity(query, entry.key),
                                       compute_lexical_similarity(query, entry.value));
                RecalledMemory rm;
                rm.entry = entry;
                rm.relevance_score = (score > 0.0f ? score : 0.5f) * entry.confidence;
                rm.match_source = "canonical_store";
                results.push_back(std::move(rm));
            }
        }
    }

    // Sort by relevance score descending
    std::sort(results.begin(), results.end(), [](const RecalledMemory& a, const RecalledMemory& b) {
        return a.relevance_score > b.relevance_score;
    });

    if (results.size() > limit) {
        results.resize(limit);
    }
    return results;
}

std::string MemoryRecall::format_recalled_context(const std::vector<RecalledMemory>& memories) const {
    if (memories.empty()) return "";

    std::ostringstream oss;
    oss << "[Recalled Relevant Memory Context]\n";
    for (const auto& rm : memories) {
        oss << " - (" << rm.match_source << ") " << rm.entry.key << ": " << rm.entry.value << "\n";
    }
    return oss.str();
}
