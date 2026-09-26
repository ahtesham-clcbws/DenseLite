#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include "search_engine.hpp"
#include "context_engine.hpp"

void test_exact_symbol() {
    SearchEngine engine;
    StructuralChunk chunk1{"src/auth.cpp", "AuthController", "AuthController", "cpp", 1, 50, 100, "class AuthController { void login(); };"};
    StructuralChunk chunk2{"src/user.cpp", "UserController", "UserController", "cpp", 1, 50, 101, "class UserController { void profile(); };"};
    engine.add_custom_chunk(chunk1);
    engine.add_custom_chunk(chunk2);

    auto hits = engine.search("AuthController");
    assert(!hits.empty());
    assert(hits[0].symbol_name == "AuthController");
    assert(hits[0].symbol_match == 1.0f);
    std::cout << "[PASS] test_exact_symbol\n";
}

void test_error_message() {
    SearchEngine engine;
    StructuralChunk chunk{"src/auth.cpp", "AuthController::login", "AuthController", "cpp", 10, 30, 102,
        "class AuthController { void login() { throw std::runtime_error(\"Login failed\"); } };"};
    engine.add_custom_chunk(chunk);

    std::string error_trace = "Fatal error: Uncaught exception in AuthController::login() on line 15";
    auto hits = engine.search(error_trace);
    assert(!hits.empty());
    assert(hits[0].file_path == "src/auth.cpp");
    assert(hits[0].lexical_score > 0.0f);
    std::cout << "[PASS] test_error_message\n";
}

void test_semantic_question() {
    SearchEngine engine;
    StructuralChunk chunk{"src/auth_service.cpp", "authenticate", "", "cpp", 1, 20, 103,
        "bool authenticate(string username, string password) { return verify_credentials(); }"};
    engine.add_custom_chunk(chunk);

    auto hits = engine.search("how does login work");
    assert(!hits.empty());
    assert(hits[0].semantic_similarity > 0.0f);
    std::cout << "[PASS] test_semantic_question\n";
}

void test_class_lookup() {
    StructuralChunk chunk{"src/db.hpp", "DatabaseConnection", "DatabaseConnection", "cpp", 1, 40, 104,
        "class DatabaseConnection { public: void connect(); void query(); };"};
    auto results = StructuralSearch::search_class("DatabaseConnection", {chunk});
    assert(results.size() == 1);
    assert(results[0].symbol_name == "DatabaseConnection");
    assert(results[0].structural_importance == 1.0f);
    std::cout << "[PASS] test_class_lookup\n";
}

void test_cross_file_concept() {
    SearchEngine engine;
    StructuralChunk chunk1{"src/auth.cpp", "login", "AuthController", "cpp", 1, 20, 105, "void login() { verify(); }"};
    StructuralChunk chunk2{"src/session.cpp", "create_session", "SessionManager", "cpp", 1, 20, 106, "void create_session() { auth_token(); }"};
    engine.add_custom_chunk(chunk1);
    engine.add_custom_chunk(chunk2);

    auto hits = engine.search("auth");
    assert(hits.size() >= 2);
    bool found_auth = false, found_session = false;
    for (const auto& h : hits) {
        if (h.file_path == "src/auth.cpp") found_auth = true;
        if (h.file_path == "src/session.cpp") found_session = true;
    }
    if (!found_auth || !found_session) {
        std::cerr << "Cross-file concept failed\n";
        std::abort();
    }
    std::cout << "[PASS] test_cross_file_concept\n";
}

void test_duplicate_deduplication() {
    SearchResult r1{"chunk1", SearchSource::CODE, "src/auth.cpp", "login", "void login() {}", 10, 20, 100, 0.2f, 0.8f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f};
    SearchResult r2{"chunk1", SearchSource::CODE, "src/auth.cpp", "login", "void login() {}", 10, 20, 100, 0.9f, 0.1f, 0.7f, 1.0f, 0.0f, 0.0f, 0.0f};

    auto fused = ResultFusion::fuse({r1, r2}, 5);
    assert(fused.size() == 1);
    assert(fused[0].semantic_similarity == 0.9f);
    assert(fused[0].lexical_score == 0.8f);
    assert(fused[0].symbol_match == 1.0f);
    std::cout << "[PASS] test_duplicate_deduplication\n";
}

void test_stale_index() {
    CodeIndexer indexer;
    indexer.init("test_search_stale.db");
    std::string path = "test_module.cpp";
    std::string v1 = "int compute() { return 42; }";
    std::string v2 = "int compute() { return 100; }";

    auto res1 = indexer.index_file(path, v1);
    assert(!res1.was_skipped);

    auto res_unchanged = indexer.index_file(path, v1);
    assert(res_unchanged.was_skipped);

    auto res2 = indexer.index_file(path, v2);
    assert(!res2.was_skipped); // Delta detected!
    std::cout << "[PASS] test_stale_index\n";
}

void test_memory_retrieval() {
    MemoryEngine mem;
    mem.init("test_search_mem.db");
    MemoryEntry entry;
    entry.key = "framework_version";
    entry.value = "Laravel 12";
    entry.category = MemoryCategory::ARCHITECTURAL_DECISION;
    mem.store().put_memory(entry);
    mem.persistent().set_entry(entry);

    SearchEngine engine(nullptr, &mem);
    auto hits = engine.search_memory("framework_version");
    assert(!hits.empty());
    assert(hits[0].content == "Laravel 12");
    std::cout << "[PASS] test_memory_retrieval\n";
}

void test_context_engine_search_evidence() {
    ContextEngine ctx;
    OpenAIRequest req;
    OpenAIMessage m1{"system", "You are an assistant.", "", ""};
    OpenAIMessage m2{"user", "Explain login.", "", ""};
    req.messages = {m1, m2};

    SearchResult evidence;
    evidence.symbol_name = "AuthController::login";
    evidence.file_path = "src/auth.cpp";
    evidence.content = "void login() { check_token(); }";

    auto result = ctx.optimize_and_compile(req, {evidence}, "qwen_main", 4096);
    assert(result.compiled_prompt.find("Retrieved Context Evidence") != std::string::npos);
    assert(result.compiled_prompt.find("AuthController::login") != std::string::npos);
    std::cout << "[PASS] test_context_engine_search_evidence\n";
}

int main() {
    std::cout << "--- Running Phase 5 Search Engine & Reranking Tests ---\n";
    test_exact_symbol();
    test_error_message();
    test_semantic_question();
    test_class_lookup();
    test_cross_file_concept();
    test_duplicate_deduplication();
    test_stale_index();
    test_memory_retrieval();
    test_context_engine_search_evidence();
    std::cout << "--- All Phase 5 Search Tests Passed! ---\n";
    return 0;
}
