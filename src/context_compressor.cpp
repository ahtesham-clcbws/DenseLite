#include "context_compressor.hpp"
#include <algorithm>
#include <unordered_set>

std::vector<OpenAIMessage> ContextCompressor::deduplicate(const std::vector<OpenAIMessage>& messages) {
    if (messages.size() <= 1) return messages;

    std::vector<OpenAIMessage> dedupled;
    dedupled.reserve(messages.size());

    for (size_t i = 0; i < messages.size(); ++i) {
        // Skip identical consecutive duplicate messages
        if (i > 0 && messages[i].role == messages[i - 1].role && messages[i].content == messages[i - 1].content) {
            continue;
        }
        // Skip empty messages unless it's a tool/system message
        if (messages[i].content.empty() && messages[i].role != "system") {
            continue;
        }
        dedupled.push_back(messages[i]);
    }
    return dedupled;
}

std::vector<OpenAIMessage> ContextCompressor::sliding_window(const std::vector<OpenAIMessage>& messages,
                                                             const Tokenizer* tokenizer,
                                                             size_t max_budget) {
    if (messages.empty()) return messages;

    auto count_tokens = [&](const std::string& text) -> size_t {
        if (tokenizer) return tokenizer->count_tokens(text);
        return (text.size() + 3) / 4;
    };

    size_t total_tokens = 0;
    for (const auto& msg : messages) {
        total_tokens += count_tokens(msg.content);
    }

    if (total_tokens <= max_budget) {
        return messages; // Within budget, no compression needed
    }

    std::vector<OpenAIMessage> optimized;
    size_t start_idx = 0;
    size_t running_tokens = 0;

    // Preserved Invariant 1: System prompt is NEVER truncated if present
    if (!messages.empty() && messages[0].role == "system") {
        optimized.push_back(messages[0]);
        start_idx = 1;
        running_tokens += count_tokens(messages[0].content);
    }

    // Preserved Invariant 2: Current/newest user query is ALWAYS included
    size_t last_msg_tokens = 0;
    if (messages.size() > start_idx) {
        last_msg_tokens = count_tokens(messages.back().content);
        running_tokens += last_msg_tokens;
    }

    // Collect recent history backwards
    std::vector<OpenAIMessage> recent_history;
    if (messages.size() > start_idx + 1) {
        for (int i = static_cast<int>(messages.size()) - 2; i >= static_cast<int>(start_idx); --i) {
            size_t msg_tokens = count_tokens(messages[i].content);
            if (running_tokens + msg_tokens > max_budget) {
                break;
            }
            recent_history.push_back(messages[i]);
            running_tokens += msg_tokens;
        }
    }

    // Reverse recent history to maintain chronological order
    for (int i = static_cast<int>(recent_history.size()) - 1; i >= 0; --i) {
        optimized.push_back(recent_history[i]);
    }

    // Append the final (current) user query
    if (messages.size() > start_idx) {
        optimized.push_back(messages.back());
    }

    return optimized;
}

std::vector<OpenAIMessage> ContextCompressor::compress(const std::vector<OpenAIMessage>& messages,
                                                       const Tokenizer* tokenizer,
                                                       size_t target_token_budget) {
    // Level 1: Deduplication
    auto l1 = deduplicate(messages);

    // Level 4: Sliding window compaction preserving boundaries
    return sliding_window(l1, tokenizer, target_token_budget);
}
