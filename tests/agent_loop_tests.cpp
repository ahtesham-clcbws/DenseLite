#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include "ResponseAnalyzer.hpp"
#include "RecoveryPolicy.hpp"
#include "CompletionPolicy.hpp"
#include "Curator.hpp"

void test_simple_question() {
    std::string clean_resp = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"The answer is 42.\"}}]}";
    assert(ResponseAnalyzer::analyze(clean_resp) == ResponseAction::COMPLETE);
    std::cout << "[PASS] test_simple_question\n";
}

void test_tool_call_detection() {
    std::string tool_resp = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"read_file\",\"arguments\":\"{}\"}}]}}]}";
    assert(ResponseAnalyzer::analyze(tool_resp) == ResponseAction::TOOL_CALL);

    // Stop reason check
    std::string pseudo_tool = "Let me read that: <tool_call>{\"name\":\"grep\"}</tool_call>";
    assert(ResponseAnalyzer::analyze(pseudo_tool) == ResponseAction::TOOL_CALL);
    std::cout << "[PASS] test_tool_call_detection\n";
}

void test_model_continue_truncation() {
    std::string truncated_resp = "{\"choices\":[{\"message\":{\"content\":\"Here is part 1\"},\"finish_reason\":\"length\"}]}";
    assert(ResponseAnalyzer::analyze(truncated_resp) == ResponseAction::MODEL_CONTINUE);

    std::string gemini_trunc = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"part 1\"}]},\"finishReason\":\"MAX_TOKENS\"}]}";
    assert(ResponseAnalyzer::analyze(gemini_trunc) == ResponseAction::MODEL_CONTINUE);
    std::cout << "[PASS] test_model_continue_truncation\n";
}

void test_garbage_invalid() {
    std::string malformed_json = "{ \"choices\": [ { unclosed JSON ";
    assert(ResponseAnalyzer::analyze(malformed_json) == ResponseAction::INVALID);

    std::string binary_garbage = "text\x01\x02\x03";
    assert(ResponseAnalyzer::analyze(binary_garbage) == ResponseAction::INVALID);
    std::cout << "[PASS] test_garbage_invalid\n";
}

void test_provider_failure_recovery() {
    assert(RecoveryPolicy::determine_action(429, "Rate limit reached") == RecoveryAction::SWITCH_PROVIDER);
    assert(RecoveryPolicy::determine_action(429, "API key quota exceeded") == RecoveryAction::SWITCH_KEY);
    assert(RecoveryPolicy::determine_action(404, "model_not_found") == RecoveryAction::SWITCH_MODEL);
    assert(RecoveryPolicy::determine_action(502, "Bad Gateway") == RecoveryAction::RETRY_SAME);
    assert(RecoveryPolicy::determine_action(500, "Internal Server Error") == RecoveryAction::FALLBACK_LOCAL);
    std::cout << "[PASS] test_provider_failure_recovery\n";
}

void test_context_too_large_recovery() {
    assert(RecoveryPolicy::determine_action(413, "Payload too large") == RecoveryAction::REDUCE_CONTEXT);
    assert(RecoveryPolicy::determine_action(400, "This model's maximum context length is 8192 tokens") == RecoveryAction::REDUCE_CONTEXT);
    std::cout << "[PASS] test_context_too_large_recovery\n";
}

void test_completion_policy_evidence() {
    // 1. Empty output rejected
    assert(!CompletionPolicy::is_acceptable("coding", "", {}));

    // 2. Bare "Done" rejected in coding task without artifact
    assert(!CompletionPolicy::is_acceptable("coding", "Done", {}));
    assert(!CompletionPolicy::is_acceptable("coding", "I'm done", {}));

    // 3. Coding task with code block accepted
    assert(CompletionPolicy::is_acceptable("coding", "Here is the code:\n```cpp\nint main(){}\n```", {}));

    // 4. Evidence complete authoritative
    CompletionEvidence ev;
    ev.evidence_says_complete = true;
    assert(CompletionPolicy::is_acceptable("coding", "All steps completed.", {}, ev));

    // 5. Unresolved error blocks completion
    ev.unresolved_error = true;
    assert(!CompletionPolicy::is_acceptable("general", "Success", {}, ev));
    std::cout << "[PASS] test_completion_policy_evidence\n";
}

void test_curator_multi_iteration_consolidation() {
    Curator curator;
    std::vector<std::string> turns = {
        "{\"choices\":[{\"message\":{\"content\":\"Step 1: Parse input\"}}]}",
        "{\"choices\":[{\"message\":{\"content\":\"Step 2: Generate output\"}}]}"
    };
    SearchResult cite;
    cite.symbol_name = "Parser::parse";
    cite.file_path = "src/parser.cpp";

    std::string consolidated = curator.consolidate(turns, "coding", {cite});
    assert(consolidated.find("Task Consolidation") != std::string::npos);
    assert(consolidated.find("Iteration 1") != std::string::npos);
    assert(consolidated.find("Step 1: Parse input") != std::string::npos);
    assert(consolidated.find("Iteration 2") != std::string::npos);
    assert(consolidated.find("Parser::parse") != std::string::npos);
    std::cout << "[PASS] test_curator_multi_iteration_consolidation\n";
}

int main() {
    std::cout << "--- Running Phase 6 Evidence-Based Agent Loop Tests ---\n";
    test_simple_question();
    test_tool_call_detection();
    test_model_continue_truncation();
    test_garbage_invalid();
    test_provider_failure_recovery();
    test_context_too_large_recovery();
    test_completion_policy_evidence();
    test_curator_multi_iteration_consolidation();
    std::cout << "--- All Phase 6 Agent Loop Tests Passed! ---\n";
    return 0;
}
