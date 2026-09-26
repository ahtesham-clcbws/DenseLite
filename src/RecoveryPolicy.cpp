#include "RecoveryPolicy.hpp"
#include <algorithm>
#include <cctype>

static bool context_has(const std::string& ctx, const std::string& term) {
    if (term.empty() || ctx.size() < term.size()) return false;
    auto it = std::search(ctx.begin(), ctx.end(), term.begin(), term.end(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
    return it != ctx.end();
}

RecoveryAction RecoveryPolicy::determine_action(int error_code, const std::string& error_context) {
    // 1. Context length / Token limits (400 or 413)
    if (error_code == 413 ||
        (error_code == 400 && (context_has(error_context, "context") ||
                               context_has(error_context, "token limit") ||
                               context_has(error_context, "maximum context length") ||
                               context_has(error_context, "too large")))) {
        return RecoveryAction::REDUCE_CONTEXT;
    }

    // 2. Model not found (404 or 400 with model missing)
    if (error_code == 404 ||
        (error_code == 400 && (context_has(error_context, "model_not_found") ||
                               context_has(error_context, "does not exist")))) {
        return RecoveryAction::SWITCH_MODEL;
    }

    // 3. Rate limits & Quotas (429)
    if (error_code == 429) {
        if (context_has(error_context, "quota") || context_has(error_context, "key") || context_has(error_context, "account")) {
            return RecoveryAction::SWITCH_KEY;
        }
        return RecoveryAction::SWITCH_PROVIDER;
    }

    // 4. Transient gateway/timeout issues (408, 502, 504)
    if (error_code == 408 || error_code == 502 || error_code == 504) {
        return RecoveryAction::RETRY_SAME;
    }

    // 5. Server errors or complete provider failure (500, 503)
    if (error_code >= 500) {
        return RecoveryAction::FALLBACK_LOCAL;
    }

    // 6. Unrecoverable (401, 403, or other unknown client errors)
    return RecoveryAction::FAIL_SESSION;
}
