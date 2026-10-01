# 05: Persistent Memory & AST Code Intelligence Benchmark

**Date:** 2026-10-01  
**Status:** 🟢 VERIFIED  
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
| **Top-K Memory Recall** | Tiered Cache + Lexical | **12,689 recalls/sec** | **78.81 µs** | 🟢 PASS |
| **Deterministic Compaction**| Session Archive | **214 compactions/sec** | **4.66 ms** | 🟢 PASS |

---

## 2. Tree-sitter Code Intelligence & AST Parsing

Rather than blindly slicing files by line counts, DenseLite indexes repositories into structurally coherent AST chunks (`FUNCTION`, `CLASS`, `METHOD`) using Tree-sitter parsers:

| Code Intelligence Operation | Target Workload | Measured Throughput | Latency per Unit | Status |
|---|---|:---:|:---:|:---:|
| **Language Detection** | File path extension map | **10,674,778 checks/s** | **93.68 ns** | 🟢 PASS |
| **Tree-sitter AST File Parse** | C++ Source Translation Unit | **7,237 files/sec** | **138.18 µs** | 🟢 PASS |
| **AST Symbol Extraction** | Classes, Methods, Funcs | **3,227,889 chunks/sec** | **309.80 ns** | 🟢 PASS |
| **Incremental Hash Tracking**| 64-bit FNV-1a byte hash | **15,151,230 checks/s (4,421.5 MB/s)** | **66.00 ns** | 🟢 PASS |
| **End-to-End File Indexing** | Parse + Chunks + Symbol Map | **751 files/sec** | **1.33 ms/file** | 🟢 PASS |

---

## 3. TurboQuant IVF SIMD Vector Index Benchmark

DenseLite implements an 8x memory-compressed SIMD vector index utilizing a 4-bit Lloyd-Max quantizer combined with spherical k-means coarse centroids:
- **Index Dimensions:** 768-dimensional embedding vectors (mimicking transformer geometry).
- **IVF Clustering:** True spherical k-means centroid training on indexed distribution prior to assignment.
- **AVX2 In-Kernel Filtering:** Bitmask filtering evaluates candidates with zero unpack overhead.

| Vector Benchmark Metric | Measured Result | Target Baseline | Status |
|---|:---:|:---:|:---:|
| **Recall@1** | **82.5%** | $> 75.0\%$ | 🟢 PASS |
| **Recall@10** | **100.0%** | $> 95.0\%$ | 🟢 PASS |
| **Mean Reciprocal Rank (MRR)** | **0.9083** | $> 0.850$ | 🟢 PASS |
| **Mean Absolute Cosine Loss ($\Delta \cos$)** | **0.0028** | $< 0.010$ | 🟢 PASS |
| **Throughput (50K vectors scan)** | **38.63 ms** | $< 50$ ms | 🟢 PASS |
| **RAM Footprint (50K vectors)** | **36 MB** (vs 292 MB FP32) | 8x Compression | 🟢 PASS |

*Open Capability Milestone:* Real-world Nomic embedding generation and retrieval accuracy across natural language corpora remains an open milestone.

---

## 4. Automated Memory Consolidation & Fact Supersession

- **Automatic Extraction:** Evaluates conversation turns across sessions to extract structured facts (`key: value`).
- **Contradiction Resolution & Supersession:** Automatically invalidates superseded facts (e.g. `primary_backend: Laravel 11` $\to$ `Laravel 12`) using vector-driven semantic memory fetching and tombstoning without manual identifier wiring.
