#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <numeric>
#include <iomanip>
#include <thread>
#include <atomic>

#include "vulkan_device.hpp"
#include "model_registry.hpp"
#include "model_pool.hpp"
#include "model_lease.hpp"
#include "kv_cache.hpp"
#include "resource_governor.hpp"
#include "tokenizer.hpp"
#include "tokenizer_registry.hpp"
#include "context_budgeter.hpp"
#include "context_compressor.hpp"
#include "context_compiler.hpp"
#include "context_engine.hpp"
#include "memory_engine.hpp"
#include "code_indexer.hpp"
#include <filesystem>

using Clock = std::chrono::high_resolution_clock;

void benchmark_vulkan_lifecycle() {
    std::cout << "\n======================================================\n";
    std::cout << " BENCHMARK: Phase 2 Model Lifecycle & Hardware Safety\n";
    std::cout << "======================================================\n";

    // 1. Vulkan Device Query
    auto start = Clock::now();
    VulkanDevice vk;
    auto mem = vk.memory_info();
    auto end = Clock::now();
    double query_us = std::chrono::duration<double, std::micro>(end - start).count();

    std::cout << "[1] Vulkan Device Discovery & Query:\n";
    std::cout << "    - Device: " << vk.device_name() << "\n";
    std::cout << "    - Dedicated VRAM: " << (mem.total_vram_bytes / (1024 * 1024)) << " MiB\n";
    std::cout << "    - 85% Safety Gate Limit: " << (mem.safe_ceiling_bytes / (1024 * 1024)) << " MiB\n";
    std::cout << "    - 15% Display Reserve: " << ((mem.total_vram_bytes - mem.safe_ceiling_bytes) / (1024 * 1024)) << " MiB\n";
    std::cout << "    - Query Latency: " << std::fixed << std::setprecision(2) << query_us << " us\n";

    // 2. Model Lease Acquisition & Release Throughput
    ModelPool pool;
    DenseModel dummy;
    dummy.config.embedding_length = 512;
    pool.add_model("qwen_main", std::move(dummy), DevicePlacement::CPU_RAM, 1800000000ULL);

    const int LEASE_ITERS = 100000;
    start = Clock::now();
    for (int i = 0; i < LEASE_ITERS; ++i) {
        ModelLease lease;
        pool.acquire_lease("qwen_main", lease);
        // Auto released upon RAII scope exit
    }
    end = Clock::now();
    double lease_sec = std::chrono::duration<double>(end - start).count();
    double lease_ops_sec = LEASE_ITERS / lease_sec;
    std::cout << "[2] Model Lease RAII Acquisition / Release:\n";
    std::cout << "    - Iterations: " << LEASE_ITERS << "\n";
    std::cout << "    - Throughput: " << std::fixed << std::setprecision(0) << lease_ops_sec << " ops/sec\n";
    std::cout << "    - Latency per cycle: " << std::setprecision(3) << (lease_sec * 1e6 / LEASE_ITERS) << " us\n";

    // 3. KV Cache Allocation & Bounded Sizing
    ModelConfig qwen_cfg;
    qwen_cfg.num_layers = 28;
    qwen_cfg.num_kv_heads = 2;
    qwen_cfg.head_dim = 128;
    qwen_cfg.context_length = 8192;

    start = Clock::now();
    const int KV_ITERS = 500;
    for (int i = 0; i < KV_ITERS; ++i) {
        KVCache kv;
        kv.allocate(qwen_cfg, 8192, &vk);
    }
    end = Clock::now();
    double kv_sec = std::chrono::duration<double>(end - start).count();
    size_t kv_bytes = 8192 * KVCache::calculate_bytes_per_token(qwen_cfg);
    std::cout << "[3] Bounded KV Cache Allocation (28 layers, 8192 tokens @ FP16 = " << (kv_bytes / (1024*1024)) << " MiB):\n";
    std::cout << "    - Allocation Latency: " << std::setprecision(2) << (kv_sec * 1e3 / KV_ITERS) << " ms per buffer\n";

    // 4. Resource Governor proc check overhead
    ResourceGovernor gov(&vk);
    start = Clock::now();
    const int GOV_ITERS = 1000;
    for (int i = 0; i < GOV_ITERS; ++i) {
        auto snap = gov.get_snapshot();
        (void)snap;
    }
    end = Clock::now();
    double gov_us = std::chrono::duration<double, std::micro>(end - start).count() / GOV_ITERS;
    std::cout << "[4] Resource Governor /proc Monitoring Latency:\n";
    std::cout << "    - Overhead per snapshot: " << std::setprecision(2) << gov_us << " us\n";
}

void benchmark_phase3_tokenizer_context() {
    std::cout << "\n======================================================\n";
    std::cout << " BENCHMARK: Phase 3 BPE Tokenizer & Context Engine\n";
    std::cout << "======================================================\n";

    // Set up synthetic BPE Vocab
    Vocab vocab;
    std::vector<std::string> words = {
        "<|im_start|>", "<|im_end|>", "system", "user", "assistant",
        "The", " quick", " brown", " fox", " jumps", " over", " the", " lazy", " dog",
        " function", " return", " int", " void", " class", " public", " private",
        " const", " auto", " string", " vector", " for", " while", " if", " else",
        " DenseLite", " Apollo", " Linux", " AMD", " Radeon", " Vulkan", " AVX2",
        " ", "\n", ".", ",", ":", ";", "(", ")", "{", "}", "[", "]", "=", "+"
    };
    vocab.tokens = words;
    vocab.scores.resize(words.size(), 0.0f);
    for (size_t id = 0; id < words.size(); ++id) {
        const auto& tok = words[id];
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

    // Prepare test corpus
    std::string small_text = "The quick brown fox jumps over the lazy dog.";
    std::string code_text = "class DenseLite { public: int return vector string DenseLite Apollo Linux AMD Vulkan AVX2; };";
    std::string large_text;
    for (int i = 0; i < 200; ++i) {
        large_text += "The quick brown fox jumps over the lazy dog. DenseLite Apollo Linux AMD Radeon Vulkan AVX2.\n";
    }

    // 1. Encoding Throughput
    const int ENC_ITERS = 1000;
    auto start = Clock::now();
    size_t total_tokens_encoded = 0;
    for (int i = 0; i < ENC_ITERS; ++i) {
        auto tokens = tok.encode(large_text);
        total_tokens_encoded += tokens.size();
    }
    auto end = Clock::now();
    double enc_sec = std::chrono::duration<double>(end - start).count();
    double enc_tokens_per_sec = total_tokens_encoded / enc_sec;

    std::cout << "[1] Native Trie BPE Tokenizer Encoding:\n";
    std::cout << "    - Document Size: " << large_text.size() << " bytes (~" << (total_tokens_encoded / ENC_ITERS) << " tokens)\n";
    std::cout << "    - Total Encoded: " << total_tokens_encoded << " tokens in " << std::setprecision(3) << enc_sec << " s\n";
    std::cout << "    - Encoding Throughput: " << std::fixed << std::setprecision(0) << enc_tokens_per_sec << " tokens/sec\n";

    // 2. Fast Token Counting (without vector allocation)
    start = Clock::now();
    size_t total_counted = 0;
    for (int i = 0; i < ENC_ITERS; ++i) {
        total_counted += tok.count_tokens(large_text);
    }
    end = Clock::now();
    double count_sec = std::chrono::duration<double>(end - start).count();
    double count_tokens_per_sec = total_counted / count_sec;

    std::cout << "[2] Fast Token Counting (Zero Vector Allocation):\n";
    std::cout << "    - Throughput: " << std::fixed << std::setprecision(0) << count_tokens_per_sec << " tokens/sec\n";
    std::cout << "    - Speedup vs Encode: " << std::setprecision(2) << (enc_sec / count_sec) << "x faster\n";

    // 3. Decoding Throughput
    auto sample_tokens = tok.encode(large_text);
    start = Clock::now();
    size_t total_tokens_decoded = 0;
    for (int i = 0; i < ENC_ITERS; ++i) {
        std::string dec = tok.decode(sample_tokens);
        total_tokens_decoded += sample_tokens.size();
    }
    end = Clock::now();
    double dec_sec = std::chrono::duration<double>(end - start).count();
    double dec_tokens_per_sec = total_tokens_decoded / dec_sec;

    std::cout << "[3] Tokenizer Decoding:\n";
    std::cout << "    - Decoding Throughput: " << std::fixed << std::setprecision(0) << dec_tokens_per_sec << " tokens/sec\n";

    // 4. Context Compressor (Deduplication + Sliding Window)
    std::vector<OpenAIMessage> session_history;
    OpenAIMessage sys_msg;
    sys_msg.role = "system";
    sys_msg.content = "You are an expert systems engineer.";
    session_history.push_back(sys_msg);

    for (int i = 0; i < 50; ++i) {
        OpenAIMessage u_msg;
        u_msg.role = "user";
        u_msg.content = "Turn " + std::to_string(i) + ": " + code_text;
        session_history.push_back(u_msg);

        OpenAIMessage a_msg;
        a_msg.role = "assistant";
        a_msg.content = "Response " + std::to_string(i) + ": " + small_text;
        session_history.push_back(a_msg);
    }

    OpenAIMessage final_msg;
    final_msg.role = "user";
    final_msg.content = "Final task: implement context engine benchmark.";
    session_history.push_back(final_msg);

    const int COMPRESS_ITERS = 5000;
    start = Clock::now();
    for (int i = 0; i < COMPRESS_ITERS; ++i) {
        auto compressed = ContextCompressor::sliding_window(session_history, &tok, 1024);
        (void)compressed;
    }
    end = Clock::now();
    double comp_sec = std::chrono::duration<double>(end - start).count();
    std::cout << "[4] Context Compression & Sliding Window (52 turns):\n";
    std::cout << "    - Latency per compression: " << std::setprecision(2) << (comp_sec * 1e6 / COMPRESS_ITERS) << " us\n";
    std::cout << "    - Compression Throughput: " << std::setprecision(0) << (COMPRESS_ITERS / comp_sec) << " passes/sec\n";

    // 5. ContextEngine End-to-End Optimization & Compilation
    TokenizerRegistry reg;
    reg.register_tokenizer("qwen_main", &vocab, 1, 0);
    ContextEngine engine(&reg);

    OpenAIRequest req;
    req.model = "qwen_main";
    req.messages = session_history;

    const int E2E_ITERS = 2000;
    start = Clock::now();
    for (int i = 0; i < E2E_ITERS; ++i) {
        auto res = engine.optimize_and_compile(req, "qwen_main", 4096);
        (void)res;
    }
    end = Clock::now();
    double e2e_sec = std::chrono::duration<double>(end - start).count();

    std::cout << "[5] ContextEngine End-to-End (Budget + Deduplication + Compress + ChatML Compile):\n";
    std::cout << "    - Latency: " << std::setprecision(2) << (e2e_sec * 1e6 / E2E_ITERS) << " us\n";
    std::cout << "    - Throughput: " << std::setprecision(0) << (E2E_ITERS / e2e_sec) << " requests/sec\n";
    std::cout << "======================================================\n\n";
}

void benchmark_phase4a_memory() {
    std::cout << "\n======================================================\n";
    std::cout << " BENCHMARK: Phase 4A Memory Engine (Zvec + SQLite)\n";
    std::cout << "======================================================\n";

    std::string test_db = "/tmp/bench_memory.db";
    std::filesystem::remove(test_db);

    MemoryEngine engine;
    engine.init(test_db);

    // 1. SQLite Canonical Write Throughput
    const int WRITE_ITERS = 5000;
    auto start = Clock::now();
    for (int i = 0; i < WRITE_ITERS; ++i) {
        MemoryEntry e;
        e.id = "mem_" + std::to_string(i);
        e.key = "key_" + std::to_string(i);
        e.value = "Persistent value record testing canonical persistence throughput in SQLite " + std::to_string(i);
        e.category = MemoryCategory::DISCOVERY;
        e.confidence = 1.0f;
        engine.store().put_memory(e);
    }
    auto end = Clock::now();
    double write_sec = std::chrono::duration<double>(end - start).count();
    std::cout << "[1] Canonical SQLite Memory Write Throughput:\n";
    std::cout << "    - Throughput: " << std::fixed << std::setprecision(0) << (WRITE_ITERS / write_sec) << " writes/sec\n";
    std::cout << "    - Latency per insert: " << std::setprecision(2) << (write_sec * 1e6 / WRITE_ITERS) << " us\n";

    // 2. SQLite Canonical Read Throughput
    const int READ_ITERS = 10000;
    start = Clock::now();
    for (int i = 0; i < READ_ITERS; ++i) {
        MemoryEntry out;
        std::string k = "key_" + std::to_string(i % WRITE_ITERS);
        engine.store().get_memory(k, out);
    }
    end = Clock::now();
    double read_sec = std::chrono::duration<double>(end - start).count();
    std::cout << "[2] Canonical SQLite Memory Read Throughput:\n";
    std::cout << "    - Throughput: " << std::fixed << std::setprecision(0) << (READ_ITERS / read_sec) << " reads/sec\n";
    std::cout << "    - Latency per read: " << std::setprecision(2) << (read_sec * 1e6 / READ_ITERS) << " us\n";

    // 3. MemoryRecall Top-K Query
    const int RECALL_ITERS = 2000;
    start = Clock::now();
    for (int i = 0; i < RECALL_ITERS; ++i) {
        auto recalled = engine.recall().recall("key_42", 5);
        (void)recalled;
    }
    end = Clock::now();
    double recall_sec = std::chrono::duration<double>(end - start).count();
    std::cout << "[3] MemoryRecall Top-K Query Search:\n";
    std::cout << "    - Throughput: " << std::fixed << std::setprecision(0) << (RECALL_ITERS / recall_sec) << " queries/sec\n";
    std::cout << "    - Latency per recall: " << std::setprecision(2) << (recall_sec * 1e6 / RECALL_ITERS) << " us\n";

    // 4. Session Compaction & Consolidation
    engine.session().set_session_id("sess_bench");
    for (int i = 0; i < 20; ++i) {
        engine.session().add_message("user", "Turn " + std::to_string(i) + ": Remember framework: Laravel 12");
        engine.session().add_message("assistant", "Confirmed.");
    }
    start = Clock::now();
    auto cres = engine.end_session_and_archive();
    end = Clock::now();
    double cons_us = std::chrono::duration<double, std::micro>(end - start).count();
    std::cout << "[4] Session Compaction & Deterministic Archive:\n";
    std::cout << "    - Archive ID: " << cres.archive_id << "\n";
    std::cout << "    - Compaction Latency: " << std::setprecision(2) << cons_us << " us\n";

    std::filesystem::remove(test_db);
    std::cout << "======================================================\n\n";
}

void benchmark_phase4b_code_intel() {
    std::cout << "\n======================================================\n";
    std::cout << " BENCHMARK: Phase 4B Code Intelligence (Tree-sitter)\n";
    std::cout << "======================================================\n";

    std::string test_db = "/tmp/bench_code_intel.db";
    std::filesystem::remove(test_db);

    CodeIndexer indexer;
    indexer.init(test_db);

    // Prepare realistic C++ source snippet (50 lines)
    std::string cpp_source =
        "#include <iostream>\n"
        "#include <vector>\n"
        "class RouterController {\n"
        "public:\n"
        "    void handle_request(int req_id) {\n"
        "        std::cout << req_id << std::endl;\n"
        "    }\n"
        "    void dispatch_job(const std::string& job) {\n"
        "        std::cout << job << std::endl;\n"
        "    }\n"
        "};\n"
        "int process_data(int a, int b) {\n"
        "    return a + b;\n"
        "}\n";

    // 1. Language Detection Latency
    const int DETECT_ITERS = 100000;
    auto start = Clock::now();
    for (int i = 0; i < DETECT_ITERS; ++i) {
        auto lang = indexer.languages().detect_language("src/controllers/RouterController.cpp");
        (void)lang;
    }
    auto end = Clock::now();
    double detect_sec = std::chrono::duration<double>(end - start).count();
    std::cout << "[1] Language Detection Latency:\n";
    std::cout << "    - Throughput: " << std::fixed << std::setprecision(0) << (DETECT_ITERS / detect_sec) << " lookups/sec\n";
    std::cout << "    - Latency per check: " << std::setprecision(3) << (detect_sec * 1e9 / DETECT_ITERS) << " ns\n";

    // 2. Tree-sitter AST Parsing & Chunking Throughput
    const int PARSE_ITERS = 2000;
    start = Clock::now();
    size_t total_chunks = 0;
    ASTChunker chunker(&indexer.languages());
    for (int i = 0; i < PARSE_ITERS; ++i) {
        auto chunks = chunker.chunk("RouterController.cpp", cpp_source);
        total_chunks += chunks.size();
    }
    end = Clock::now();
    double parse_sec = std::chrono::duration<double>(end - start).count();
    std::cout << "[2] Tree-sitter AST Parsing & Chunking (C++):\n";
    std::cout << "    - Parses per second: " << std::fixed << std::setprecision(0) << (PARSE_ITERS / parse_sec) << " files/sec\n";
    std::cout << "    - Latency per file: " << std::setprecision(2) << (parse_sec * 1e6 / PARSE_ITERS) << " us\n";
    std::cout << "    - Chunks extracted: " << (total_chunks / PARSE_ITERS) << " definitions/file\n";

    // 3. Incremental Change Tracking Throughput
    const int HASH_ITERS = 50000;
    start = Clock::now();
    for (int i = 0; i < HASH_ITERS; ++i) {
        bool changed = indexer.tracker().has_changed("RouterController.cpp", cpp_source);
        (void)changed;
    }
    end = Clock::now();
    double hash_sec = std::chrono::duration<double>(end - start).count();
    double hash_mb_sec = (HASH_ITERS * cpp_source.size()) / (hash_sec * 1024 * 1024);
    std::cout << "[3] Incremental Hash Change Tracking (64-bit FNV-1a):\n";
    std::cout << "    - Throughput: " << std::fixed << std::setprecision(0) << (HASH_ITERS / hash_sec) << " checks/sec ("
              << std::setprecision(2) << hash_mb_sec << " MB/s)\n";

    // 4. End-to-End File Indexing & Symbol Search
    start = Clock::now();
    auto idx_res = indexer.index_file("RouterController.cpp", cpp_source, true);
    auto syms = indexer.query_symbol("RouterController");
    end = Clock::now();
    double idx_us = std::chrono::duration<double, std::micro>(end - start).count();
    std::cout << "[4] Full End-to-End File Indexing & Symbol Query:\n";
    std::cout << "    - Chunks Indexed: " << idx_res.chunk_count << "\n";
    std::cout << "    - Symbols Found: " << syms.size() << "\n";
    std::cout << "    - Total Latency: " << std::setprecision(2) << idx_us << " us\n";

    std::filesystem::remove(test_db);
    std::cout << "======================================================\n\n";
}

int main() {
    benchmark_vulkan_lifecycle();
    benchmark_phase3_tokenizer_context();
    benchmark_phase4a_memory();
    benchmark_phase4b_code_intel();
    return 0;
}

