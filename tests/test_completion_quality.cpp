#include "CompletionPolicy.hpp"
#include "ResponseAnalyzer.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <iomanip>

struct CompletionTestCase {
    std::string description;
    std::string task_type;
    std::string model_output;
    CompletionEvidence evidence;
    bool expected_acceptable;
};

struct ResponseAnalyzerTestCase {
    std::string description;
    std::string raw_output;
    std::string stop_reason;
    ResponseAction expected_action;
};

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << " CompletionPolicy & ResponseAnalyzer Quality Benchmark (Point 8)" << std::endl;
    std::cout << " Hallucination Prevention & Completion Validation Test Suite" << std::endl;
    std::cout << "==========================================================" << std::endl;

    // 1. CompletionPolicy Test Corpus
    std::vector<CompletionTestCase> completion_cases = {
        // Bare claims on coding tasks without evidence must be rejected
        {"Coding: Bare 'done' claim without artifact", "coding", "Done.", {true, true, false, false, false, false, true, false}, false},
        {"Coding: Bare 'I'm done' claim without artifact", "coding", "I'm done", {true, true, false, false, false, false, true, false}, false},
        {"Coding: Bare 'Finished' claim without artifact", "coding", "finished", {true, true, false, false, false, false, true, false}, false},
        
        // Coding tasks with code blocks must be accepted
        {"Coding: Code block in output", "coding", "Here is the code:\n```cpp\nint main() { return 0; }\n```", {true, true, false, false, false, false, true, false}, true},
        
        // Coding tasks with artifact produced must be accepted
        {"Coding: Artifact produced", "coding", "Done.", {true, true, false, false, true, false, true, false}, true},
        
        // Coding tasks with tool result received must be accepted
        {"Coding: Tool result received", "coding", "The file was modified.", {true, true, true, true, false, false, true, false}, true},
        
        // Unresolved errors must ALWAYS reject
        {"Unresolved error present", "general", "Here is your answer.", {true, true, false, false, false, true, true, false}, false},
        {"Unresolved error in coding", "coding", "```cpp\nvoid f();\n```", {true, true, false, false, false, true, true, false}, false},
        
        // Tool called but no result received must reject
        {"Tool called with no result", "coding", "Calling tool...", {true, true, true, false, false, false, false, false}, false},
        
        // Empty output must reject unless evidence has output
        {"Empty output without evidence", "general", "", {false, false, false, false, false, false, false, false}, false},
        
        // General query with informative text must be accepted
        {"General query with informative answer", "general", "The capital of France is Paris.", {true, true, false, false, false, false, true, false}, true},
        
        // Authoritative evidence saying complete
        {"Authoritative evidence flag", "coding", "In progress", {true, true, false, false, false, false, false, true}, true}
    };

    size_t comp_passed = 0;
    std::cout << "\n[1] Evaluating CompletionPolicy Invariants:" << std::endl;
    for (size_t i = 0; i < completion_cases.size(); ++i) {
        const auto& tc = completion_cases[i];
        bool result = CompletionPolicy::is_acceptable(tc.task_type, tc.model_output, {}, tc.evidence);
        bool match = (result == tc.expected_acceptable);
        if (match) comp_passed++;

        std::cout << "  - Case " << std::setw(2) << (i + 1) << " [" << (match ? "OK" : "FAIL") << "]: " 
                  << tc.description << " -> Expected: " << (tc.expected_acceptable ? "ACCEPT" : "REJECT")
                  << ", Got: " << (result ? "ACCEPT" : "REJECT") << std::endl;
        assert(match && "CompletionPolicy test case failed!");
    }

    // 2. ResponseAnalyzer Test Corpus
    std::vector<ResponseAnalyzerTestCase> analyzer_cases = {
        // Truncated / malformed JSON
        {"Truncated JSON structure", "{\"choices\": [{\"message\": {\"content\": \"hello world", "", ResponseAction::INVALID},
        {"Malformed JSON object", "{key: \"value\", invalid_syntax", "", ResponseAction::INVALID},
        
        // Length / Max tokens stop reasons -> MODEL_CONTINUE
        {"Explicit stop_reason=length", "Partial text here", "length", ResponseAction::MODEL_CONTINUE},
        {"Explicit stop_reason=max_tokens", "More partial text", "max_tokens", ResponseAction::MODEL_CONTINUE},
        {"OpenAI JSON with finish_reason=length", "{\"choices\": [{\"finish_reason\": \"length\", \"message\": {\"content\": \"half text\"}}]}", "", ResponseAction::MODEL_CONTINUE},
        {"Gemini JSON with finishReason=MAX_TOKENS", "{\"candidates\": [{\"finishReason\": \"MAX_TOKENS\", \"content\": {\"parts\": []}}]}", "", ResponseAction::MODEL_CONTINUE},
        
        // Explicit error
        {"Explicit stop_reason=error", "", "error", ResponseAction::MODEL_ERROR},
        {"JSON containing error object", "{\"error\": {\"message\": \"Rate limit exceeded\", \"code\": 429}}", "", ResponseAction::MODEL_ERROR},
        
        // Tool calls
        {"OpenAI JSON with tool_calls in message", "{\"choices\": [{\"finish_reason\": \"tool_calls\", \"message\": {\"tool_calls\": [{\"id\": \"call_1\", \"function\": {\"name\": \"bash\"}}]}}]}", "", ResponseAction::TOOL_CALL},
        {"Pseudo-markup <tool_call>", "Let me check: <tool_call>{\"name\": \"run_cmd\"}</tool_call>", "", ResponseAction::TOOL_CALL},
        {"Pseudo-markup ```tool_code", "```tool_code\nrun_command()\n```", "", ResponseAction::TOOL_CALL},
        
        // Unprintable garbage / null bytes -> INVALID
        {"Garbage control bytes", std::string("Malformed \x01\x02 binary stream"), "", ResponseAction::INVALID},
        {"Embedded null byte", std::string("Corrupted\0output", 16), "", ResponseAction::INVALID},
        
        // Valid complete text
        {"Valid standard text response", "This is a clean and fully complete response from the model.", "", ResponseAction::COMPLETE}
    };

    size_t anal_passed = 0;
    std::cout << "\n[2] Evaluating ResponseAnalyzer Invariants:" << std::endl;
    for (size_t i = 0; i < analyzer_cases.size(); ++i) {
        const auto& tc = analyzer_cases[i];
        ResponseAction action = ResponseAnalyzer::analyze(tc.raw_output, tc.stop_reason);
        bool match = (action == tc.expected_action);
        if (match) anal_passed++;

        std::cout << "  - Case " << std::setw(2) << (i + 1) << " [" << (match ? "OK" : "FAIL") << "]: " 
                  << tc.description << std::endl;
        assert(match && "ResponseAnalyzer test case failed!");
    }

    std::cout << "\n[Results Summary]:" << std::endl;
    std::cout << "| Component        | Total Cases | Passed | Accuracy | Gate Status |" << std::endl;
    std::cout << "|------------------|-------------|--------|----------|-------------|" << std::endl;
    std::cout << "| CompletionPolicy | " << std::setw(11) << completion_cases.size() << " | " << std::setw(6) << comp_passed << " | 100.0%   | 🟢 PASS     |" << std::endl;
    std::cout << "| ResponseAnalyzer | " << std::setw(11) << analyzer_cases.size() << " | " << std::setw(6) << anal_passed << " | 100.0%   | 🟢 PASS     |" << std::endl;

    std::cout << "\n>> All Completion Quality & Hallucination Prevention Tests Passed!" << std::endl;
    return 0;
}
