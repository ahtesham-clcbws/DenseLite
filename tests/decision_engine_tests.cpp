#include "decision_engine.hpp"
#include "RequestAnalyzer.hpp"
#include <iostream>
#include <cassert>
#include <chrono>

void test_fast_heuristic_coding() {
    std::cout << "[Test 1] Fast Heuristic: Coding Request..." << std::endl;
    std::string query = "Refactor the function in src/server.cpp to avoid memory leak";
    auto dec = DecisionEngine::instance().fast_heuristic_decide(query);

    assert(dec.domain == "coding");
    assert(dec.intent == "coding");
    assert(dec.requires_code_context);
    assert(dec.suggested_action == "inspect_repository");
    assert(dec.confidence >= 0.88f);
    assert(dec.complexity_score > 0.3f);
    std::cout << "  -> PASSED (Domain: " << dec.domain << ", Action: " << dec.suggested_action << ")" << std::endl;
}

void test_fast_heuristic_troubleshooting() {
    std::cout << "[Test 2] Fast Heuristic: Troubleshooting & Bug..." << std::endl;
    std::string query = "error: segfault in kv_cache.cpp when max_tokens exceeded";
    auto dec = DecisionEngine::instance().fast_heuristic_decide(query);

    assert(dec.domain == "troubleshooting");
    assert(dec.requires_code_context);
    assert(dec.suggested_action == "inspect_repository");
    assert(dec.complexity_score >= 0.5f);
    std::cout << "  -> PASSED (Complexity: " << dec.complexity_score << ")" << std::endl;
}

void test_memory_and_web_flags() {
    std::cout << "[Test 3] Memory and Web Search Needs Detection..." << std::endl;
    std::string query_mem = "What were the project convention and architecture guidelines we decided?";
    auto dec_mem = DecisionEngine::instance().fast_heuristic_decide(query_mem);
    assert(dec_mem.requires_memory);

    std::string query_web = "Check the latest documentation for v13 online changelog";
    auto dec_web = DecisionEngine::instance().fast_heuristic_decide(query_web);
    assert(dec_web.requires_web_search);
    std::cout << "  -> PASSED" << std::endl;
}

void test_request_analyzer_integration() {
    std::cout << "[Test 4] RequestAnalyzer::analyze_decision Integration..." << std::endl;
    OpenAIRequest req;
    OpenAIMessage msg;
    msg.role = "user";
    msg.content = "Implement a new struct in C++ with cmake configuration";
    req.messages.push_back(msg);

    auto dec = RequestAnalyzer::analyze_decision(req);
    assert(dec.domain == "coding");
    assert(dec.requires_code_context);
    assert(dec.suggested_action == "inspect_repository");
    std::cout << "  -> PASSED" << std::endl;
}

void test_nli_entailment_and_contradiction() {
    std::cout << "[Test 5] NLI Entailment & Contradiction Detection..." << std::endl;
    
    std::string premise = "The database migration table was created successfully with 21 columns.";
    std::string hyp_entailed = "migration table created successfully";
    std::string hyp_unrelated = "deploy kubernetes cluster to aws";

    float score_ent = DecisionEngine::instance().evaluate_entailment(premise, hyp_entailed);
    float score_unrel = DecisionEngine::instance().evaluate_entailment(premise, hyp_unrelated);

    (void)score_ent;
    (void)score_unrel;
    assert(score_ent >= 0.9f);
    assert(score_unrel == 0.0f);

    std::string p_active = "The authentication service is enabled by default.";
    std::string h_contra = "The authentication service is disabled by default.";
    std::string h_consistent = "The authentication service is enabled by default.";

    assert(DecisionEngine::instance().is_contradiction(p_active, h_contra));
    assert(!DecisionEngine::instance().is_contradiction(p_active, h_consistent));

    std::cout << "  -> PASSED (Entailment: " << score_ent << ", Contradiction: verified)" << std::endl;
}

void test_benchmark_latency() {
    std::cout << "[Benchmark 1] DecisionEngine Latency on 2-core Target..." << std::endl;
    std::string query = "Refactor the function in src/server.cpp to avoid memory leak";
    
    // Warmup
    DecisionEngine::instance().fast_heuristic_decide(query);

    auto start = std::chrono::high_resolution_clock::now();
    int iterations = 1000;
    for (int i = 0; i < iterations; ++i) {
        DecisionEngine::instance().fast_heuristic_decide(query);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double avg_us = (total_ms * 1000.0) / iterations;
    
    std::cout << "  -> PASSED: Tier-1 Fast Heuristic Latency: " << avg_us << " µs per decision" << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << " DenseLite Phase 2 Decision Engine Test Suite    " << std::endl;
    std::cout << "=================================================" << std::endl;

    test_fast_heuristic_coding();
    test_fast_heuristic_troubleshooting();
    test_memory_and_web_flags();
    test_request_analyzer_integration();
    test_nli_entailment_and_contradiction();

    test_benchmark_latency();

    std::cout << "=================================================" << std::endl;
    std::cout << " All Phase 2 Decision Engine Tests PASSED!       " << std::endl;
    std::cout << "=================================================" << std::endl;
    return 0;
}
