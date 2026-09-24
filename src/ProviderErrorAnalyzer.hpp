#pragma once

#include <string>

enum class ErrorResolution {
    RETRY_SAME_PROVIDER,
    RETRY_DIFFERENT_PROVIDER,
    COOLDOWN_KEY,
    FATAL
};

struct ProviderErrorAnalysis {
    ErrorResolution resolution;
    std::string suggested_fallback_model;
    int cooldown_seconds;
};

class ProviderErrorAnalyzer {
public:
    // Analyze the raw JSON error chunk returned from a provider and the HTTP status
    // Returns a deterministic action to take.
    static ProviderErrorAnalysis analyze(const std::string& provider, const std::string& model, int status_code, const std::string& error_json);
};
