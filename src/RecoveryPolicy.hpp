#pragma once
#include <string>

enum class RecoveryAction {
    RETRY_SAME_MODEL,
    SWITCH_MODEL,
    SWITCH_PROVIDER,
    FALLBACK_LOCAL,
    FAIL_SESSION
};

class RecoveryPolicy {
public:
    static RecoveryAction determine_action(int error_code, const std::string& error_context);
};
