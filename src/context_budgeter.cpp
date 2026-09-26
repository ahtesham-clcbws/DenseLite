#include "context_budgeter.hpp"
#include <algorithm>

ContextBudgetPlan ContextBudgeter::calculate_budget(size_t total_context,
                                                    size_t system_tokens,
                                                    size_t task_tokens,
                                                    size_t min_generation_reserve) {
    ContextBudgetPlan plan;
    plan.total_context_limit = (total_context > 0) ? total_context : 8192;

    // Generation reserve: at least 25% of total context or min_generation_reserve
    size_t quarter_context = plan.total_context_limit / 4;
    plan.generation_reserve = std::max(min_generation_reserve, quarter_context);
    if (plan.generation_reserve >= plan.total_context_limit) {
        plan.generation_reserve = plan.total_context_limit / 2;
    }

    plan.max_input_tokens = plan.total_context_limit - plan.generation_reserve;
    plan.system_prompt_tokens = system_tokens;
    plan.task_prompt_tokens = task_tokens;

    size_t mandatory = system_tokens + task_tokens;

    if (mandatory >= plan.max_input_tokens) {
        // Extreme context pressure: system and task take entire input headroom
        plan.history_budget = 0;
        plan.working_memory_budget = 0;
        plan.retrieved_code_budget = 0;
        plan.retrieved_memory_budget = 0;
        plan.total_allocated_input = mandatory;
    } else {
        size_t remaining = plan.max_input_tokens - mandatory;

        // Allocate history up to 50% of remaining or up to 2048 tokens
        plan.history_budget = std::min(remaining / 2, size_t(2048));
        size_t aux_remaining = remaining - plan.history_budget;

        plan.retrieved_code_budget = std::min(aux_remaining * 5 / 10, size_t(2500));
        plan.retrieved_memory_budget = std::min(aux_remaining * 3 / 10, size_t(800));
        plan.working_memory_budget = aux_remaining - plan.retrieved_code_budget - plan.retrieved_memory_budget;

        plan.total_allocated_input = mandatory + plan.history_budget + plan.working_memory_budget 
                                   + plan.retrieved_code_budget + plan.retrieved_memory_budget;
    }

    return plan;
}
