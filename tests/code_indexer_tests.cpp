#include "code_indexer.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>

void test_language_detection() {
    std::cout << "[Test 1] Language Detection..." << std::endl;
    LanguageRegistry reg;
    assert(reg.detect_language("src/infer.cpp") == "cpp");
    assert(reg.detect_language("src/model.hpp") == "cpp");
    assert(reg.detect_language("scripts/eval.py") == "python");
    assert(reg.detect_language("app/Http/Controllers/AuthController.php") == "php");
    assert(reg.detect_language("web/extension.ts") == "javascript");
    assert(reg.detect_language("web/index.js") == "javascript");
    assert(reg.detect_language("README.md") == "");
    std::cout << "  -> PASSED" << std::endl;
}

void test_ast_chunking_python() {
    std::cout << "[Test 2] AST Chunking (Python)..." << std::endl;
    LanguageRegistry reg;
    ASTChunker chunker(&reg);

    std::string py_code =
        "class ModelTrainer:\n"
        "    def __init__(self, lr):\n"
        "        self.lr = lr\n"
        "\n"
        "    def train_step(self, x):\n"
        "        return x * self.lr\n";

    auto chunks = chunker.chunk("trainer.py", py_code);
    assert(!chunks.empty());

    bool found_class = false;
    bool found_train = false;
    for (const auto& c : chunks) {
        if (c.symbol.find("ModelTrainer") != std::string::npos) found_class = true;
        if (c.symbol.find("train_step") != std::string::npos) found_train = true;
    }
    assert(found_class);
    assert(found_train);
    std::cout << "  -> PASSED" << std::endl;
}

void test_ast_chunking_cpp() {
    std::cout << "[Test 3] AST Chunking (C++)..." << std::endl;
    LanguageRegistry reg;
    ASTChunker chunker(&reg);

    std::string cpp_code =
        "#include <iostream>\n"
        "class DenseEngine {\n"
        "public:\n"
        "    void execute() {\n"
        "        std::cout << \"Running\" << std::endl;\n"
        "    }\n"
        "};\n";

    auto chunks = chunker.chunk("engine.cpp", cpp_code);
    assert(!chunks.empty());

    bool found_engine = false;
    for (const auto& c : chunks) {
        if (c.symbol.find("DenseEngine") != std::string::npos) found_engine = true;
    }
    assert(found_engine);
    std::cout << "  -> PASSED" << std::endl;
}

void test_ast_chunking_php() {
    std::cout << "[Test 4] AST Chunking (PHP / Laravel)..." << std::endl;
    LanguageRegistry reg;
    ASTChunker chunker(&reg);

    std::string php_code =
        "<?php\n"
        "namespace App\\Http\\Controllers;\n"
        "class AuthController {\n"
        "    public function login() {\n"
        "        return response()->json(['status' => 'ok']);\n"
        "    }\n"
        "}\n";

    auto chunks = chunker.chunk("AuthController.php", php_code);
    assert(!chunks.empty());

    bool found_auth = false;
    for (const auto& c : chunks) {
        if (c.symbol.find("AuthController") != std::string::npos) found_auth = true;
    }
    assert(found_auth);
    std::cout << "  -> PASSED" << std::endl;
}

void test_incremental_tracking_and_indexing() {
    std::cout << "[Test 5] Incremental Change Tracking & Indexing..." << std::endl;
    std::string test_db = "/tmp/test_code_intel.db";
    std::filesystem::remove(test_db);

    CodeIndexer indexer;
    assert(indexer.init(test_db));

    std::string code_v1 = "def add(a, b):\n    return a + b\n";
    auto res1 = indexer.index_file("math.py", code_v1);
    assert(!res1.was_skipped);
    assert(res1.chunk_count >= 1);

    // Second index with IDENTICAL content must be skipped by CodeChangeTracker
    auto res2 = indexer.index_file("math.py", code_v1);
    assert(res2.was_skipped);

    // Modified content must reparse
    std::string code_v2 = "def add(a, b):\n    return a + b\n\ndef sub(a, b):\n    return a - b\n";
    auto res3 = indexer.index_file("math.py", code_v2);
    assert(!res3.was_skipped);
    assert(res3.chunk_count >= 2);

    // Query symbols
    auto syms = indexer.query_symbol("sub");
    assert(!syms.empty());
    assert(syms[0].symbol.find("sub") != std::string::npos);

    std::filesystem::remove(test_db);
    std::cout << "  -> PASSED" << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << " DenseLite Phase 4B Code Intelligence Test Suite " << std::endl;
    std::cout << "=================================================" << std::endl;

    test_language_detection();
    test_ast_chunking_python();
    test_ast_chunking_cpp();
    test_ast_chunking_php();
    test_incremental_tracking_and_indexing();

    std::cout << "=================================================" << std::endl;
    std::cout << " All Phase 4B Code Intelligence Tests PASSED!    " << std::endl;
    std::cout << "=================================================" << std::endl;
    return 0;
}
