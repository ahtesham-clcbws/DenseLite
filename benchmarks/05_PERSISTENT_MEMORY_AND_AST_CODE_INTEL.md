# 05: Persistent Memory & AST Code Intelligence Benchmark

**Date:** 2026-09-26  
**Status:** 🟢 FROZEN & EMPIRICALLY VERIFIED  
**Hardware Platform:** Intel Core i7-6500U @ 2.50GHz, 32 GB RAM  

---

## 1. Persistent State Memory Store (SQLite + Tiered Cache)

DenseLite replaces volatile in-RAM maps with a two-tier persistent memory engine:
- **Tier 1 (Canonical Store):** ACID SQLite database (`denselite_state.db`) for long-term memory, conversation state, tool results, and rate limits.
- **Tier 2 (In-RAM Cache):** Ring-buffered working memory for sub-microsecond lexical and semantic recall.

| Memory Operation | Storage Engine | Measured Throughput | Latency per Op | Status |
|---|:---:|:---:|:---:|:---:|
| **Canonical Record Insert** | SQLite WAL | **7,626 writes/sec** | **131.13 µs** | 🟢 PASS |
| **Canonical Record Select** | SQLite Index | **44,391 reads/sec** | **22.53 µs** | 🟢 PASS |
| **Top-K Memory Recall** | Tiered Cache + Lexical | **9,292 recalls/sec** | **107.62 µs** | 🟢 PASS |
| **Deterministic Compaction**| Session Archive | **214 compactions/sec** | **4.66 ms** | 🟢 PASS |

---

## 2. Tree-sitter Code Intelligence & AST Parsing

Rather than blindly slicing files by line counts, DenseLite indexes repositories into structurally coherent AST chunks (`FUNCTION`, `CLASS`, `METHOD`) using Tree-sitter parsers:

| Code Intelligence Operation | Target Workload | Measured Throughput | Latency per Unit | Status |
|---|:---:|:---:|:---:|:---:|
| **Language Detection** | File path extension map | **10,674,778 checks/s** | **93.68 ns** | 🟢 PASS |
| **Tree-sitter AST File Parse** | C++ Source Translation Unit | **5,620 files/sec** | **177.93 µs** | 🟢 PASS |
| **AST Symbol Extraction** | Classes, Methods, Funcs | **3,227,889 chunks/sec** | **309.80 ns** | 🟢 PASS |
| **Incremental Hash Tracking**| 64-bit FNV-1a byte hash | **8,994,077 checks/s (2,624 MB/s)** | **111.18 ns** | 🟢 PASS |
| **End-to-End File Indexing** | Parse + Chunks + Symbol Map | **751 files/sec** | **1.33 ms/file** | 🟢 PASS |

---

## 3. Structural Chunk Schema Verification

```text
StructuralChunk {
    file_path:     "src/model_manager.cpp",
    symbol:        "ModelManager::acquire",
    parent_symbol: "ModelManager",
    type:          "METHOD",
    start_line:    42,
    end_line:      68,
    source_hash:   0x8f7a2c19e5d43b2a,
    content:       "[C++ Source Lines 42-68]"
}
```

- **Delta Change Detection:** Modifying a single function body only invalidates and reindexes that specific AST chunk; untouched functions within the file are bypassed with 0 vector operations.
