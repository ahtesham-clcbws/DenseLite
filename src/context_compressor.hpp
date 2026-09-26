#pragma once
#include "RequestAnalyzer.hpp"
#include "tokenizer.hpp"
#include <vector>
#include <string>

class ContextCompressor {
public:
    // Compress messages to fit strictly within target_token_budget
    static std::vector<OpenAIMessage> compress(const std::vector<OpenAIMessage>& messages,
                                               const Tokenizer* tokenizer,
                                               size_t target_token_budget);

    // L1: Deduplicate consecutive or identical content messages
    static std::vector<OpenAIMessage> deduplicate(const std::vector<OpenAIMessage>& messages);

    // L4: Compact session history using chronological sliding window while preserving system prompt and latest user task
    static std::vector<OpenAIMessage> sliding_window(const std::vector<OpenAIMessage>& messages,
                                                     const Tokenizer* tokenizer,
                                                     size_t max_budget);
};
