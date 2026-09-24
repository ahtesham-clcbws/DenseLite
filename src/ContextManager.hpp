#pragma once
#include "RequestAnalyzer.hpp"
#include <vector>
#include <string>

#include "infer.hpp"

class ContextManager {
public:
    // Compress the context array if it exceeds limits, keeping system prompts and recent context
    // Optionally use Nomic Embed (semantic selection) if enabled
    static void optimize_context(OpenAIRequest& req, int max_context_tokens = 8192, bool use_semantic_selection = false, DenseModel* nomic = nullptr);
    
private:
    // Basic sliding window compression
    static std::vector<OpenAIMessage> truncate_history(const std::vector<OpenAIMessage>& messages, int max_context_tokens);
    
    // Nomic Embed semantic filtering (placeholder/stub for future local model execution)
    static std::vector<OpenAIMessage> semantic_filter(const std::vector<OpenAIMessage>& messages, DenseModel* nomic);
};
