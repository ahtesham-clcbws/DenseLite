#pragma once
#include <string>

enum class RecoveryAction {
    RETRY_SAME,       // Transient network/provider error
    SWITCH_KEY,       // Key-level rate limit or quota exceeded
    SWITCH_PROVIDER,  // Provider overloaded (429/503)
    SWITCH_MODEL,     // Model not found or deprecated (404)
    REDUCE_CONTEXT,   // Context window exceeded (400/413)
    FALLBACK_LOCAL,   // Cloud service down -> fallback to local AVX2
    FAIL_SESSION      // Unrecoverable authentication/permission failure
};

class RecoveryPolicy {
public:
    static RecoveryAction determine_action(int error_code, const std::string& error_context);
};
