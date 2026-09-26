# DenseLite Benchmark: Phase 5 Multi-Signal Search & Deterministic Result Fusion

**Phase:** Phase 5 Unified Search & Deterministic Reranking  
**Date:** 2026-09-26  
**Hardware:** Intel(R) Core(TM) i7-6500U (2 cores, 4 threads @ 2.50GHz), 32 GB RAM  
**OS:** Linux (CachyOS / Niri)  

---

## 1. Executive Summary

Phase 5 introduces a unified, multi-channel retrieval engine across Code, Memory, and Text. Instead of expensive, non-deterministic neural rerankers (like ColBERT or Cross-Encoders) that waste tokens and saturate laptop CPU cores, Phase 5 implements a deterministic multi-signal fusion model:

$$\text{score} = w_1 s_{\text{semantic}} + w_2 s_{\text{lexical}} + w_3 s_{\text{structural}} + w_4 s_{\text{symbol}} + w_5 s_{\text{recency}} + w_6 s_{\text{task}}$$

Where:
- $w_1 = 0.25$ (Semantic Similarity / Cosine Distance)
- $w_2 = 0.25$ (Lexical BM25 / Term Matching)
- $w_3 = 0.15$ (Structural AST Hierarchy: Class/Struct > Method/Function)
- $w_4 = 0.20$ (Exact / Case-Insensitive Symbol Match)
- $w_5 = 0.05$ (Recency / Timestamp)
- $w_6 = 0.10$ (Active Task Context Relevance)

All measurements were taken on host hardware using `./build/benchmark_engine`.

---

## 2. Empirical Benchmark Results

| Channel / Component | Benchmark Metric | Measured Result | Target Threshold | Evaluation |
|---|---|---|---|---|
| **ExactSearch** | Exact / Case-Insensitive Symbol & Substring Lookup | **42,726 queries/sec** (23.40 µs/query) | > 5,000 queries/sec | ✅ PASS (8.5x target) |
| **LexicalSearch** | Tokenized BM25 Term Matching & Error Tracing | **28,522 queries/sec** (35.06 µs/query) | > 5,000 queries/sec | ✅ PASS (5.7x target) |
| **VectorSearch** | 512-dim Dense Vector Cosine Similarity | **1,339,750 ops/sec** (746.41 ns/op) | > 500,000 ops/sec | ✅ PASS (2.7x target) |
| **ResultFusion** | Multi-Signal Deduplication & Weighted Scoring (40 candidates $\to$ top 5) | **182,884 fusions/sec** (5.47 µs/fusion) | > 20,000 fusions/sec | ✅ PASS (9.1x target) |
| **SearchEngine Pipeline** | End-to-End Multi-Signal Search over Code & Memory | **13,425 queries/sec** (74.49 µs/query) | > 2,000 queries/sec | ✅ PASS (6.7x target) |

---

## 3. Invariant Protections & Architectural Guarantees

1. **Deterministic Scoring (Zero LLM / Neural Reranker Overhead):**
   - Scoring and ranking consume zero model context tokens, zero forward passes, and zero GPU memory, leaving 100% of compute resources dedicated to primary generation.
2. **Multi-Signal Deduplication:**
   - Overlapping spans and duplicate symbol hits across different retrieval channels are merged into a single hit, preserving the highest individual signal scores ($\max$).
3. **Stale Index Invalidation:**
   - Incorporates `CodeChangeTracker` FNV-1a hash checks so that modified files trigger incremental re-indexing, preventing stale code references from polluting the retrieved prompt context.
4. **ContextEngine Retrieval Wiring:**
   - Search evidence is directly consumed by `ContextEngine::optimize_and_compile()`, formatting structural chunks under sectional budget caps while preserving the system prompt invariant and generation reserve.
