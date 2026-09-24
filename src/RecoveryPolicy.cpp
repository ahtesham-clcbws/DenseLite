#include "RecoveryPolicy.hpp"

RecoveryAction RecoveryPolicy::determine_action(int error_code, const std::string& error_context) {
    if (error_code == 429) return RecoveryAction::SWITCH_PROVIDER;
    if (error_code == 404) return RecoveryAction::SWITCH_MODEL;
    if (error_code >= 500) return RecoveryAction::FALLBACK_LOCAL;
    
    return RecoveryAction::FAIL_SESSION;
}
