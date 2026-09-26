#pragma once
#include "RequestAnalyzer.hpp"
#include "tokenizer.hpp"
#include <string>
#include <vector>

struct CompiledContext {
    std::string prompt;
    size_t prompt_tokens = 0;
    size_t system_tokens = 0;
    size_t history_tokens = 0;
    size_t task_tokens = 0;
    bool fits_budget = true;
};

class ContextCompiler {
public:
    // Compiles messages into a ChatML prompt formatted string
    static std::string format_chatml(const std::vector<OpenAIMessage>& messages,
                                     bool append_assistant_header = true);

    // Compiles messages, calculates exact token breakdown using tokenizer, and verifies budget
    static CompiledContext compile(const std::vector<OpenAIMessage>& messages,
                                   const Tokenizer* tokenizer,
                                   size_t max_input_tokens = 6144,
                                   bool append_assistant_header = true);
};
