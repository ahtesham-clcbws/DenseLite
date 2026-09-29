#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>

enum class MemoryCategory {
    RULE,
    CONVENTION,
    DISCOVERY,
    ARCHITECTURAL_DECISION
};

struct MemoryEntry {
    std::string id;
    MemoryCategory category = MemoryCategory::DISCOVERY;
    std::string key;
    std::string value;
    float confidence = 1.0f;
    int64_t updated_at = 0;

    // Scoped Living Memory & Provenance (Phase 1)
    std::string workspace_id = "default";
    std::string session_id = "";
    std::string actor_scope = "public";
    std::string visibility = "workspace"; // "public", "workspace", "session", "private"
    std::string source_path = "";        // e.g. "composer.json", "src/auth.ts"
    std::string source_type = "user";    // "file", "user_chat", "agent_action", "compiler"
    std::string commit_hash = "";        // git commit or content hash when learned
    float importance = 0.5f;             // 0.0 to 1.0
    int access_count = 0;
    int64_t last_accessed_at = 0;
    int64_t expires_at = 0;              // 0 = never expires
    std::string supersedes_id = "";      // key/id of prior memory this replaces
    std::string contradicts_id = "";     // key/id of memory this conflicts with
    std::string status = "active";       // "active", "superseded", "contradicted", "expired"
};

struct MemoryScopeFilter {
    std::string workspace_id = "";
    std::string session_id = "";
    std::vector<std::string> allowed_visibilities = {"public", "workspace"};
    bool active_only = true;

    bool matches(const MemoryEntry& entry) const {
        if (active_only && entry.status != "active") {
            return false;
        }
        if (!workspace_id.empty() && entry.workspace_id != workspace_id &&
            entry.workspace_id != "default" && entry.visibility != "public") {
            return false;
        }
        if (!session_id.empty() && entry.visibility == "session" && entry.session_id != session_id) {
            return false;
        }
        if (!allowed_visibilities.empty()) {
            bool vis_ok = false;
            for (const auto& v : allowed_visibilities) {
                if (entry.visibility == v) {
                    vis_ok = true;
                    break;
                }
            }
            if (!vis_ok) return false;
        }
        return true;
    }
};

class PersistentMemory {
public:
    PersistentMemory() = default;

    void set_entry(const MemoryEntry& entry);
    bool get_entry(const std::string& key, MemoryEntry& out) const;
    bool remove_entry(const std::string& key);

    std::vector<MemoryEntry> get_all_by_category(MemoryCategory cat) const;
    std::vector<MemoryEntry> get_all() const;
    size_t count() const;

    std::string format_context_rules() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::map<std::string, MemoryEntry> entries_;
};
