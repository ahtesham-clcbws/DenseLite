#include "context_engine.hpp"

ContextEngine::ContextEngine(TokenizerRegistry* tokenizer_registry)
    : registry_(tokenizer_registry) {}

ContextOptimizationResult ContextEngine::optimize_and_compile(
    const OpenAIRequest& req,
    const std::string& target_model,
    size_t total_context_limit) {

    ContextOptimizationResult result;
    result.original_messages_count = req.messages.size();

    const Tokenizer* tokenizer = registry_ ? registry_->get_tokenizer(target_model) : nullptr;

    auto count_tokens_fn = [&](const std::string& text) -> size_t {
        if (tokenizer && tokenizer->is_valid()) {
            return tokenizer->count_tokens(text);
        }
        return (text.size() + 3) / 4;
    };

    // 1. Identify system prompt and current task prompt to calculate initial invariant tokens
    size_t system_tokens = 0;
    size_t task_tokens = 0;
    for (size_t i = 0; i < req.messages.size(); ++i) {
        const auto& m = req.messages[i];
        if (m.role == "system") {
            system_tokens += count_tokens_fn(m.content);
        } else if (i + 1 == req.messages.size() && m.role == "user") {
            task_tokens = count_tokens_fn(m.content);
        }
    }

    // 2. Compute budget ensuring generation reserve and system prompt are protected
    result.budget_plan = ContextBudgeter::calculate_budget(total_context_limit,
                                                           system_tokens,
                                                           task_tokens);

    // 3. Compress messages if needed
    std::vector<OpenAIMessage> msgs = ContextCompressor::deduplicate(req.messages);
    if (msgs.size() < req.messages.size()) {
        result.compression_applied = true;
    }

    std::vector<OpenAIMessage> optimized_msgs = ContextCompressor::compress(
        msgs, tokenizer, result.budget_plan.max_input_tokens);

    if (optimized_msgs.size() < msgs.size()) {
        result.compression_applied = true;
    }
    result.compressed_messages_count = optimized_msgs.size();

    // 4. Compile into ChatML and verify budget compliance
    result.compiled_context = ContextCompiler::compile(
        optimized_msgs, tokenizer, result.budget_plan.max_input_tokens);
    result.compiled_prompt = result.compiled_context.prompt;

    return result;
}
