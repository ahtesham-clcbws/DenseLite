#include "tokenizer.hpp"
#include "tokenizer_registry.hpp"
#include "context_budgeter.hpp"
#include "context_compressor.hpp"
#include "context_compiler.hpp"
#include "context_engine.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

void test_tokenizer_bpe() {
    Vocab vocab;
    vocab.tokens = {"<|im_start|>", "<|im_end|>", "system", "user", "assistant", "Hello", " world", "!", "\n"};
    vocab.scores = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    // Populate trie
    for (size_t id = 0; id < vocab.tokens.size(); ++id) {
        const auto& tok = vocab.tokens[id];
        TrieNode* curr = vocab.root.get();
        for (char c : tok) {
            if (curr->children.find(c) == curr->children.end()) {
                curr->children[c] = std::make_unique<TrieNode>();
            }
            curr = curr->children[c].get();
        }
        curr->token_id = static_cast<int>(id);
    }

    Tokenizer tok(&vocab, 1, 0);
    assert(tok.is_valid());
    assert(tok.vocab_size() == 9);

    std::string text = "Hello world!";
    auto tokens = tok.encode(text);
    assert(tokens.size() == 3);
    assert(tok.count_tokens(text) == 3);
    assert(tok.decode(tokens) == text);

    std::cout << "  [PASS] Tokenizer Trie BPE encoding & decoding\n";
}

void test_context_budgeter_invariants() {
    // 8192 total tokens: 25% = 2048 generation reserve, 6144 max input
    size_t system_toks = 150;
    size_t task_toks = 200;
    auto plan = ContextBudgeter::calculate_budget(8192, system_toks, task_toks, 1024);

    assert(plan.total_context_limit == 8192);
    assert(plan.generation_reserve >= 2048);
    assert(plan.max_input_tokens == 8192 - plan.generation_reserve);
    assert(plan.system_prompt_tokens == 150);
    assert(plan.task_prompt_tokens == 200);

    // Test with small context limit (e.g. 2048): min generation reserve 1024 applies
    auto small_plan = ContextBudgeter::calculate_budget(2048, 100, 100, 1024);
    assert(small_plan.generation_reserve >= 1024);
    assert(small_plan.max_input_tokens <= 1024);

    std::cout << "  [PASS] ContextBudgeter invariant protections\n";
}

void test_context_compressor() {
    std::vector<OpenAIMessage> msgs = {
        {"system", "You are an assistant."},
        {"user", "Hello"},
        {"user", "Hello"}, // duplicate
        {"assistant", "Hi there!"},
        {"user", "Tell me about Apollo."},
        {"assistant", "Apollo is an operating system."},
        {"user", "What is DenseLite?"}
    };

    // Test deduplication
    auto dedup = ContextCompressor::deduplicate(msgs);
    assert(dedup.size() == 6);
    assert(dedup[1].role == "user" && dedup[1].content == "Hello");
    assert(dedup[2].role == "assistant");

    // Test sliding window with restrictive budget
    auto compressed = ContextCompressor::sliding_window(dedup, nullptr, 30);
    // Invariants: system prompt preserved, newest query preserved
    assert(!compressed.empty());
    assert(compressed.front().role == "system");
    assert(compressed.back().role == "user");
    assert(compressed.back().content == "What is DenseLite?");

    std::cout << "  [PASS] ContextCompressor deduplication & sliding window\n";
}

void test_context_compiler() {
    std::vector<OpenAIMessage> msgs = {
        {"system", "You are an AI."},
        {"user", "Ping"}
    };

    std::string prompt = ContextCompiler::format_chatml(msgs, true);
    std::string expected = "<|im_start|>system\nYou are an AI.\n<|im_end|>\n<|im_start|>user\nPing\n<|im_end|>\n<|im_start|>assistant\n";
    assert(prompt == expected);

    auto compiled = ContextCompiler::compile(msgs, nullptr, 1000);
    assert(compiled.fits_budget == true);
    assert(compiled.system_tokens > 0);
    assert(compiled.task_tokens > 0);
    assert(compiled.prompt == expected);

    std::cout << "  [PASS] ContextCompiler ChatML prompt generation\n";
}

void test_context_engine_facade() {
    TokenizerRegistry reg;
    Vocab vocab;
    vocab.tokens = {"<|im_start|>", "<|im_end|>", "system", "user", "assistant"};
    for (size_t id = 0; id < vocab.tokens.size(); ++id) {
        vocab.scores.push_back(0.0f);
    }
    reg.register_tokenizer("qwen_main", &vocab, 1, 0);

    ContextEngine engine(&reg);

    OpenAIRequest req;
    req.model = "qwen_main";
    req.messages = {
        {"system", "System instruction"},
        {"user", "Turn 1"},
        {"assistant", "Reply 1"},
        {"user", "Final question"}
    };

    auto result = engine.optimize_and_compile(req, "qwen_main", 8192);
    assert(!result.compiled_prompt.empty());
    assert(result.budget_plan.generation_reserve >= 2048);
    assert(result.compiled_context.fits_budget);
    assert(result.compressed_messages_count == 4);

    std::cout << "  [PASS] ContextEngine end-to-end optimization\n";
}

int main() {
    std::cout << "[PHASE 3 TEST SUITE] Native BPE Tokenizer & Context Engine\n";
    test_tokenizer_bpe();
    test_context_budgeter_invariants();
    test_context_compressor();
    test_context_compiler();
    test_context_engine_facade();
    std::cout << "ALL PHASE 3 CONTEXT ENGINE TESTS PASSED!\n";
    return 0;
}
