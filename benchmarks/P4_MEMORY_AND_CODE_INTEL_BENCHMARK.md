# DenseLite Benchmark: Phase 4 Vector/State Memory Store & Tree-sitter Code Intelligence

**Phase:** Phase 4A (Memory Store & Recall) & Phase 4B (Code Intelligence & AST Parser)  
**Date:** 2026-09-26  
**Hardware:** Intel(R) Core(TM) i7-6500U (2 cores, 4 threads @ 2.50GHz), 32 GB RAM  
**OS:** Linux (CachyOS / Niri)  

---

## 1. Executive Summary

Phase 4 delivers two foundational engines:
1. **Phase 4A (Vector & State Memory Store):** Multi-tier memory architecture featuring thread-safe in-RAM working memory, session turn management, persistent rule caching, SQLite canonical storage, lexical similarity recall, and deterministic factual compaction (Amendment 5).
2. **Phase 4B (Code Intelligence & AST Parser):** Vendored Tree-sitter runtime supporting C++, Python, PHP, and JavaScript; structural AST chunking (functions, classes, methods); 64-bit FNV-1a delta hash change tracking; and SQLite symbol indexing (`code_symbols`).

All empirical measurements were obtained directly on host hardware using `./build/benchmark_engine`.

---

## 2. Empirical Benchmark Results

### Phase 4A: Memory Store & Recall

| Component | Benchmark Metric | Measured Result | Target Threshold | Evaluation |
|---|---|---|---|---|
| **SQLite Canonical Write** | Parameterized `INSERT OR REPLACE` (1,000 ops) | 12,309 writes/sec (81.24 µs/op) | > 2,000 writes/sec | ✅ PASS (6.1x faster than target) |
| **SQLite Canonical Read** | Primary Key Point Lookup (2,000 ops) | 53,958 reads/sec (18.53 µs/op) | > 10,000 reads/sec | ✅ PASS (5.4x faster than target) |
| **Memory Recall Top-K** | In-Memory Lexical Scoring & Ranking | 10,804 queries/sec (92.56 µs/query) | > 1,000 queries/sec | ✅ PASS (10.8x faster than target) |
| **Session Compaction** | Turn Archival & Deterministic Fact Extraction | 2,596.80 µs | < 10,000 µs | ✅ PASS (Sub-3ms archival) |

### Phase 4B: Code Intelligence & AST Parsing

| Component | Benchmark Metric | Measured Result | Target Threshold | Evaluation |
|---|---|---|---|---|
| **Language Detection** | Extension to Tree-sitter Grammar Lookup | 14,417,486 lookups/sec (69.36 ns/op) | > 1,000,000 lookups/sec | ✅ PASS |
| **AST Parsing & Chunking** | Multi-function C++ Source Parsing | 7,190 files/sec (139.08 µs/file) | > 500 files/sec | ✅ PASS (14.3x faster than target) |
| **FNV-1a Hash Delta Check** | 64-bit Hash Computation & Comparison | 21,206,146 checks/sec (6,188.47 MB/s) | > 1,000,000 checks/sec | ✅ PASS (Instant skip for unchanged files) |
| **End-to-End File Indexing** | Full Parse + Chunk + Symbol SQLite Index | 1,119.94 µs | < 5,000 µs | ✅ PASS (1.12ms total latency) |

---

## 3. Invariant Protections & Architectural Guarantees

1. **Multi-Tier Separation:**
   - **Working Memory:** Zero-allocation in-RAM tracking of current objective, active task, active model, and constraints.
   - **Session Memory:** Chronological turn history with tool invocations and results.
   - **Persistent Memory:** High-speed in-RAM cache for project rules, architecture patterns, and conventions.
   - **Canonical SQLite Store:** Persistent backing store with thread-safe connection pooling and `SQLITE_OPEN_FULLMUTEX`.
2. **Deterministic Fact Extraction (Amendment 5):**
   - Extracts structured facts (`[fact]`, `[decision]`, `[constraint]`, `KEY: VALUE`) without invoking LLM tokens during memory consolidation.
3. **AST-Driven Structural Integrity:**
   - Tokenization breaks code at logical AST boundaries (functions, classes, structs) rather than naive character or line offsets, preserving syntactic integrity for embedding and retrieval.
4. **Delta Change Tracking:**
   - 64-bit FNV-1a hashing guarantees unmodified files bypass AST parsing entirely, reducing repository indexing time by > 99% during incremental edits.
