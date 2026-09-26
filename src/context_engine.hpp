#pragma once
#include "RequestAnalyzer.hpp"
#include "tokenizer_registry.hpp"
#include "context_budgeter.hpp"
#include "context_compressor.hpp"
#include "context_compiler.hpp"
#include <string>
#include <vector>

struct ContextOptimizationResult {
    std::string compiled_prompt;
    ContextBudgetPlan budget_plan;
    CompiledContext compiled_context;
    size_t original_messages_count = 0;
    size_t compressed_messages_count = 0;
    bool compression_applied = false;
};

class ContextEngine {
public:
    explicit ContextEngine(TokenizerRegistry* tokenizer_registry = nullptr);

    // Main entry point: optimizes request messages, enforces invariants, and compiles prompt
    ContextOptimizationResult optimize_and_compile(const OpenAIRequest& req,
                                                   const std::string& target_model = "qwen_main",
                                                   size_t total_context_limit = 8192);

    void set_tokenizer_registry(TokenizerRegistry* registry) {
        registry_ = registry;
    }

private:
    TokenizerRegistry* registry_ = nullptr;
};
