#include "ContextManager.hpp"
#include "context_compressor.hpp"
#include <iostream>

void ContextManager::optimize_context(OpenAIRequest& req, int max_context_tokens, bool use_semantic_selection, DenseModel* nomic) {
    if (req.messages.empty()) return;
    
    if (use_semantic_selection && nomic != nullptr) {
        req.messages = semantic_filter(req.messages, nomic);
    } else {
        req.messages = truncate_history(req.messages, max_context_tokens);
    }
}

std::vector<OpenAIMessage> ContextManager::truncate_history(const std::vector<OpenAIMessage>& messages, int max_context_tokens) {
    auto deduplicated = ContextCompressor::deduplicate(messages);
    return ContextCompressor::sliding_window(deduplicated, nullptr, static_cast<size_t>(max_context_tokens));
}

std::vector<OpenAIMessage> ContextManager::semantic_filter(const std::vector<OpenAIMessage>& messages, DenseModel* nomic) {
    if (messages.size() < 2) return messages;
    return truncate_history(messages, 8192);
}
