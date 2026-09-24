#include "ContextManager.hpp"
#include <iostream>
#include <zvec/db/collection.h>

void ContextManager::optimize_context(OpenAIRequest& req, int max_context_tokens, bool use_semantic_selection, DenseModel* nomic) {
    if (req.messages.empty()) return;
    
    if (use_semantic_selection && nomic != nullptr) {
        std::cout << "[ContextManager] Applying Nomic Embed semantic filtering..." << std::endl;
        req.messages = semantic_filter(req.messages, nomic);
    } else {
        // Simple fast-path sliding window compression
        req.messages = truncate_history(req.messages, max_context_tokens);
    }
}

std::vector<OpenAIMessage> ContextManager::truncate_history(const std::vector<OpenAIMessage>& messages, int max_context_tokens) {
    // A crude approximation: 1 token = ~4 chars
    int current_approx_tokens = 0;
    for (const auto& msg : messages) {
        current_approx_tokens += msg.content.size() / 4;
    }
    
    if (current_approx_tokens <= max_context_tokens) {
        return messages; // No compression needed
    }
    
    std::cout << "[ContextManager] Context length (" << current_approx_tokens << ") exceeds threshold (" << max_context_tokens << "). Truncating history..." << std::endl;
    
    std::vector<OpenAIMessage> optimized;
    
    // Always preserve the system prompt if it exists
    size_t start_idx = 0;
    int running_tokens = 0;
    if (messages[0].role == "system") {
        optimized.push_back(messages[0]);
        start_idx = 1;
        running_tokens += messages[0].content.size() / 4;
    }
    
    // Ensure the last message (current user query) is always included
    if (messages.size() > start_idx) {
        running_tokens += messages.back().content.size() / 4;
    }
    
    std::vector<OpenAIMessage> recent_buffer;
    // Iterate from second-to-last message backwards
    for (int i = messages.size() - 2; i >= (int)start_idx; --i) {
        int msg_tokens = messages[i].content.size() / 4;
        if (running_tokens + msg_tokens > max_context_tokens) {
            break;
        }
        recent_buffer.push_back(messages[i]);
        running_tokens += msg_tokens;
    }
    
    // Reverse the recent buffer to maintain chronological order
    for (int i = recent_buffer.size() - 1; i >= 0; --i) {
        optimized.push_back(recent_buffer[i]);
    }
    
    // Append the last message
    if (messages.size() > start_idx) {
        optimized.push_back(messages.back());
    }
    
    return optimized;
}

std::vector<OpenAIMessage> ContextManager::semantic_filter(const std::vector<OpenAIMessage>& messages, DenseModel* nomic) {
    if (messages.size() < 2) return messages; // Nothing to filter

    // TODO: Requires encoder-specific forward_pass implementation.
    // Nomic Embed is an encoder model; running it through the decoder forward_pass produces garbage.
    // For now, force the truncation path.
    std::cout << "[ContextManager] Semantic filter is currently a stub. Falling back to greedy truncation." << std::endl;
    return truncate_history(messages, 8192);
}
