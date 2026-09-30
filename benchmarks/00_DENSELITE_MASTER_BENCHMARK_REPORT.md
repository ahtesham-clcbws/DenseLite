# 00: DenseLite v4.0.0 — Master System Benchmark Report

**Project:** DenseLite (Pure C++ Native Intelligence Engine)  
**Version:** v4.0.0  
**Date:** 2026-09-30  
**Hardware Platform:** Intel(R) Core(TM) i7-6500U CPU @ 2.50GHz (2 Cores, 4 Threads), 32 GB RAM  
**GPU Hardware / VRAM Gate:** AMD Radeon R7 M350 / Intel HD Graphics 520 (Vulkan 1.3)  
**Operating System:** Linux 6.13.5-zen1-1-zen (x86_64)  
**Compiler:** GCC 15.2.1 with flags `-O3 -mavx2 -mfma -fopenmp -std=c++20`  
**Test Suite Verification:** 17/17 CTest Suites Passing (100% Pass Rate in ~24.7s)  

> Evidence scope: historical measurements describe the dated implementation, not the current working tree. See [current validation](10_CURRENT_VALIDATION.md) for fresh logs and remaining gaps.

---

## Executive Summary

DenseLite v4.0.0 is an edge-optimized, native C++ multi-model orchestration and intelligence engine. Designed specifically for resource-constrained hardware (dual-core edge laptops), it enforces a strict ResourcePolicy CPU allocation (50% of hardware threads for compute; not a total CPU utilization cap) and a RAM ceiling derived from detected host RAM through ResourcePolicy while providing high-performance transformer inference, structural code understanding, persistent memory, and agentic self-healing recovery.

With v4.0.0, DenseLite integrates Session Tool Registry deduplication, Dynamic 64K context scaling, and Disk-Backed Persistent Session KV Caching with delta prefix evaluation. Furthermore, the engine is fortified with **Phase 1-4 Hardening**, which includes:
- **Strict Hardware Governance & VRAM Gate:** Enforcement of an ResourcePolicy VRAM ceiling, deterministic CPU/GPU admission gates, and strict RAM-bound KV caching.
- **ModernBERT Strict Routing:** Zero-shot intent classification explicitly overriding legacy heuristic fallbacks.
- **Unification of TurboQuant:** Direct routing of VectorSearch and MemoryStore queries through SIMD-accelerated exhaustive scans.
- **Core Semantic Fixes:** NLI evaluation downgraded from hard heuristic decisions, proper JSON exception bubbling, and strict session/workspace DB isolation.

---

## High-Level Benchmark Scorecard

| Subsystem | Core Metric | Measured Throughput / Latency | Target Baseline | Result |
|---|---|:---:|:---:|:---:|
| **AVX2 Math Core** | `dot_product_q8_fp32` | rel_err $< 1.1 \times 10^{-6}$ | rel_err $< 1.0 \times 10^{-5}$ | 🟢 PASS |
| **Model Lifecycle** | RAII `ModelLease` Acquire/Release | **5,396,617 ops/sec** (0.185 µs) | $> 500,000$ ops/sec | 🟢 PASS |
| **Vulkan Hardware Gate** | ResourcePolicy VRAM ceiling Enforcement | **ResourcePolicy VRAM Max** (307 MiB Display Reserve) | Strict ResourcePolicy Gate | 🟢 PASS |
| **BPE Tokenizer** | Native Trie BPE Encoding | **1,426,758 tokens/sec** | $> 500,000$ tokens/sec | 🟢 PASS |
| **Fast Token Counting** | Zero-Allocation BPE Count | **1,527,724 tokens/sec** | $> 1,000,000$ tokens/sec | 🟢 PASS |
| **Context Compilation** | End-to-End Budget + ChatML | **1,771 requests/sec** (564.51 µs) | $> 200$ requests/sec | 🟢 PASS |
| **Persistent Memory** | Canonical SQLite Reads | **121,528 reads/sec** (8.23 µs) | $> 10,000$ reads/sec | 🟢 PASS |
| **Semantic Recall** | Top-K Memory Recall | **12,689 recalls/sec** (78.81 µs) | $> 1,000$ recalls/sec | 🟢 PASS |
| **Code Intelligence** | Tree-sitter AST File Parsing | **7,237 files/sec** (138.18 µs) | $> 500$ files/sec | 🟢 PASS |
| **Delta Change Tracking** | 64-bit FNV-1a Hash Stream | **15,151,230 checks/s (4,421.5 MB/s)** | $> 500$ MB/s | 🟢 PASS |
| **Vector Search** | FP32 pairwise cosine helper, 512 dimensions; not index search | **1,418,551 ops/sec** (704.9 ns) | $> 200,000$ ops/sec | 🟢 PASS |
| **Result Fusion** | 40-Candidate Score Merging | **203,358 fusions/sec** (4.92 µs) | $> 10,000$ fusions/sec | 🟢 PASS |
| **Response Analyzer** | 5-State Multi-Schema Parser | **470,336 parses/sec** (2.13 µs) | $> 50,000$ parses/sec | 🟢 PASS |
| **Fault Recovery** | 7-Action Self-Healing Engine | **13,011,777 decisions/sec** (0.08 µs) | $> 500,000$ decisions/sec | 🟢 PASS |
| **Completion Gate** | Evidence policy evaluation (effectiveness unmeasured) | **105,620,712 evals/sec** (0.01 µs) | $> 10,000,000$ evals/sec | 🟢 PASS |
| **Thread Throttling** | OpenMP $\le 2$ Thread Cap | **1,217,537 enforcements/sec** (0.82 µs) | Strict 2 Threads | 🟢 PASS |
| **Memory Eviction** | 6-Stage Progressive Cascade | **35,200 assessments/sec** (28.41 µs) | $> 5,000$ assessments/sec | 🟢 PASS |
| **Speech-to-Text** | Synthetic audio chunk processing | **29,444 chunks/sec** (synthetic chunk processing)| $> 1,000$ chunks/sec | ⚠️ SYNTHETIC |
| **Image Generation** | Diffusion Step Simulation | **14,847 passes/sec** (67.35 µs) | $> 1,000$ passes/sec | ⚠️ SYNTHETIC |
| **Cloud LLM Routing** | HTTPS Multi-Provider Failover | **52.6 ms Latency / 100% Failover Resilient** | $< 100$ ms Gateway Overhead | 🟢 PASS |
| **Dynamic Context Sizing**| 64K Context Headroom Scaling | **40,385 evals/sec (24.76 µs)** | 64K Tokens Allocated | 🟢 PASS |
| **Session Tool Registry** | Handshake Schema Deduplication | **434,723 handshakes/sec (2.30 µs)** | 0-Schema Chat Pruning | 🟢 PASS |
| **Tool Payload Pruning** | Selective Tool Schema Extraction | **2,219,108 queries/sec (0.45 µs)** | Sub-Microsecond Pruning | 🟢 PASS |
| **KV Prefix Delta Match** | Common Token Sequence Matching | **1,676,250 matches/sec (0.60 µs)** | Delta Prefill Speedup | 🟢 PASS |
| **Disk-Backed KV Flush** | Binary Serialization Throughput | **2,243.4 MB/s (140.2 saves/sec)** | Non-Blocking Persistence | 🟢 PASS |

---

## Benchmark Index

Reports 00–09 retain historical metrics. [Current working-tree validation and evidence](10_CURRENT_VALIDATION.md) record the new run separately:
- [01_HARDWARE_AND_ENVIRONMENT_AUDIT.md](01_HARDWARE_AND_ENVIRONMENT_AUDIT.md): Low-level CPU, SIMD, RAM, Vulkan GPU, and OS execution environment.
- [02_INFERENCE_AND_RUNTIME_BENCHMARK.md](02_INFERENCE_AND_RUNTIME_BENCHMARK.md): Pure C++ AVX2 forward pass, multi-model throughput, and TTFT.
- [03_LIFECYCLE_AND_MEMORY_SAFETY.md](03_LIFECYCLE_AND_MEMORY_SAFETY.md): RAII lease throughput, Vulkan ResourcePolicy VRAM ceiling, and bounded KV cache.
- [04_BPE_TOKENIZER_AND_CONTEXT_BENCHMARK.md](04_BPE_TOKENIZER_AND_CONTEXT_BENCHMARK.md): Trie BPE encoding/decoding, generation reserve invariants, and ChatML compilation.
- [05_PERSISTENT_MEMORY_AND_AST_CODE_INTEL.md](05_PERSISTENT_MEMORY_AND_AST_CODE_INTEL.md): SQLite canonical storage, TurboQuant SIMD exhaustive recall, Tree-sitter AST indexing, and FNV-1a delta tracking.
- [06_HYBRID_SEARCH_AND_AGENTIC_LOOP.md](06_HYBRID_SEARCH_AND_AGENTIC_LOOP.md): 4-channel retrieval, ResultFusion, 5-state response parsing, and 7-action self-healing.
- [07_RESOURCE_GOVERNANCE_AND_MULTIMODAL.md](07_RESOURCE_GOVERNANCE_AND_MULTIMODAL.md): 2-core CPU throttling, 6-stage progressive eviction, Whisper STT, and Stable Diffusion.
- [08_FINAL_REALITY_AUDIT_MATRIX.md](08_FINAL_REALITY_AUDIT_MATRIX.md): Comprehensive verification matrix proving core features delivered, alongside documented synthetic multimodal paths and unbenchmarked 64K perplexity limitations.
- [09_SESSION_KV_AND_TOOL_REGISTRY_BENCHMARK.md](09_SESSION_KV_AND_TOOL_REGISTRY_BENCHMARK.md): Session Tool Registry, Dynamic 64K Context Sizing, and Disk-Backed Persistent Session KV Cache.
