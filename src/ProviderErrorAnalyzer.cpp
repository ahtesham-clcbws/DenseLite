#include "ProviderErrorAnalyzer.hpp"
#include <iostream>

ProviderErrorAnalysis ProviderErrorAnalyzer::analyze(const std::string& provider, const std::string& model, int status_code, const std::string& error_json) {
    ProviderErrorAnalysis analysis = {ErrorResolution::FATAL, "", 0};

    // 429 Too Many Requests
    if (status_code == 429 || error_json.find("rate_limit") != std::string::npos || error_json.find("429") != std::string::npos) {
        std::cout << "[ErrorAnalyzer] Rate limit detected for " << provider << std::endl;
        analysis.resolution = ErrorResolution::COOLDOWN_KEY;
        analysis.cooldown_seconds = 300; // 5 minute cooldown
        return analysis;
    }

    // 400/404 Model Not Found or Invalid Model
    if (status_code == 404 || error_json.find("model_not_found") != std::string::npos || error_json.find("does not exist") != std::string::npos) {
        std::cout << "[ErrorAnalyzer] Model " << model << " not found on " << provider << std::endl;
        analysis.resolution = ErrorResolution::RETRY_DIFFERENT_PROVIDER;
        // We no longer hardcode fallbacks here. The engine will query the router dynamically.
        return analysis;
    }

    // Context Length Exceeded
    if (status_code == 400 && (error_json.find("context_length_exceeded") != std::string::npos || error_json.find("maximum context length") != std::string::npos)) {
        std::cout << "[ErrorAnalyzer] Context length exceeded on " << provider << std::endl;
        analysis.resolution = ErrorResolution::FATAL;
        return analysis;
    }

    // 5xx Server Errors
    if (status_code >= 500) {
        std::cout << "[ErrorAnalyzer] Server error on " << provider << std::endl;
        analysis.resolution = ErrorResolution::RETRY_DIFFERENT_PROVIDER;
        analysis.cooldown_seconds = 60; // short cooldown for server errors
        return analysis;
    }

    // Default fallback
    std::cout << "[ErrorAnalyzer] Unknown error on " << provider << ". Cooling down key." << std::endl;
    analysis.resolution = ErrorResolution::COOLDOWN_KEY;
    analysis.cooldown_seconds = 60;
    return analysis;
}
