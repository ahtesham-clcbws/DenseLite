#pragma once
#include <cstddef>

struct ContextBudgetPlan {
    size_t total_context_limit = 8192;
    size_t generation_reserve = 2048;    // Reserved for model output generation (never consumed by input)
    size_t max_input_tokens = 6144;      // total_context_limit - generation_reserve

    size_t system_prompt_tokens = 0;     // Preserved 100% (never truncated)
    size_t task_prompt_tokens = 0;       // Current user query (preserved 100%)
    size_t working_memory_budget = 300;
    size_t retrieved_code_budget = 2000;
    size_t retrieved_memory_budget = 600;
    size_t history_budget = 1200;        // Allocated budget for prior turns
    size_t total_allocated_input = 0;
};

class ContextBudgeter {
public:
    // Calculates section budgets ensuring generation reserve and system prompt are strictly protected
    static ContextBudgetPlan calculate_budget(size_t total_context,
                                              size_t system_tokens,
                                              size_t task_tokens,
                                              size_t min_generation_reserve = 1024);
};
