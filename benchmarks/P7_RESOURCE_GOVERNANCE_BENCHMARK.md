# DenseLite Benchmark: Phase 7 2-Core Resource Governance & CPU/RAM Throttling

**Phase:** Phase 7 2-Core Resource Governance & CPU/RAM Throttling  
**Date:** 2026-09-26  
**Hardware:** Intel(R) Core(TM) i7-6500U (2 cores, 4 threads @ 2.50GHz), 32 GB RAM  
**OS:** Linux (CachyOS / Niri)  

---

## 1. Executive Summary

Phase 7 hardens every subsystem in DenseLite with proactive resource governance, strictly tailored for dual-core, memory-constrained edge hardware. Key architectural deliverables:
1. **Dynamic Thread Throttling (50% CPU Budget):** Caps OpenMP inference and execution loops to strictly $\le 2$ threads (`get_max_allowed_threads()`), ensuring the host desktop/IDE environment never freezes or experiences thread contention.
2. **Proactive 6-Stage Eviction Cascade:** Replaces reactive crashes with a deterministic 6-tier eviction cascade:
   - **Stage 1 (DISCARD_SCRATCH):** Free transient scratch buffers when available RAM drops below 25% or 3072 MiB.
   - **Stage 2 (SHRINK_CONTEXT):** Cap context compiler to 4096 tokens when available RAM drops below 20% or 2560 MiB.
   - **Stage 3 (EVICT_RETRIEVAL):** Evict candidate search cache when available RAM drops below 16% or 2048 MiB.
   - **Stage 4 (UNLOAD_WARM):** Unload idle leased models (`active_users == 0`) when available RAM drops below 12% or 1536 MiB.
   - **Stage 5 (REJECT_OPTIONAL):** Deny optional speculative model loads when available RAM drops below 8% or 1024 MiB.
   - **Stage 6 (ROUTE_CLOUD):** Force cloud provider fallback when available RAM drops below 5% or 512 MiB, or process RSS exceeds 14 GiB ceiling.
3. **Component Cost Accounting:** Zero-overhead atomic memory tracking across inference context, KV cache, vector store, and model weights ($< 20$ ns update overhead).

All measurements were taken on host hardware using `./build/benchmark_engine`.

---

## 2. Empirical Benchmark Results

| Channel / Subsystem | Benchmark Metric | Measured Result | Target Threshold | Evaluation |
|---|---|---|---|---|
| **Thread Throttling** | OpenMP Dynamic Enforcement (Capped at 50% CPU / 2 Threads) | **152,236 enforcements/sec** (6.57 µs/op) | > 10,000 enforcements/sec | ✅ PASS (15.2x target) |
| **Eviction Cascade** | 6-Stage Proactive Cascade Assessment | **29,022 assessments/sec** (34.46 µs/op) | > 5,000 assessments/sec | ✅ PASS (5.8x target) |
| **Memory Headroom** | Host RAM Admission Check | **82,455 evaluations/sec** (12.13 µs/op) | > 10,000 evaluations/sec | ✅ PASS (8.2x target) |
| **Component Accounting** | Atomic Tracking Overhead (Context + KV + Zvec + Weights) | **52,812,679 updates/sec** (18.94 ns/op) | > 1,000,000 updates/sec | ✅ PASS (52.8x target) |
| **Snapshot Query** | Continuous Linux `/proc/meminfo` + `statm` Polling | **9,636 queries/sec** (103.77 µs/op) | > 2,000 queries/sec | ✅ PASS (4.8x target) |

---

## 3. Invariant Protections & Architectural Guarantees

1. **Host Stability Guarantee (2-Core Cap):**
   - By querying physical hardware concurrency and enforcing `hw_concurrency / 2`, DenseLite guarantees that native matrix multiplications and token searches never monopolize 100% of the laptop CPU, preventing system lockups.
2. **Deterministic OOM Prevention (Stage 6 Cloud Fallback):**
   - Rather than allowing Linux `oom-killer` to terminate the process when host memory is exhausted, `ResourceGovernor::should_route_to_cloud()` intercepts requests and transparently delegates generation to cheap cloud providers.
3. **Eviction-Protected RAII Leases:**
   - Active model leases with `active_users > 0` cannot be evicted under any eviction stage, guaranteeing that currently executing inferences are never interrupted mid-generation.
4. **Context Cache Compaction:**
   - When Stage 2 is triggered, `DenseLiteEngine::execute_pipeline` automatically clamps prompt compilation from 8192 to 4096 tokens, shedding transient prompt buffers while preserving the system invariant and generation reserve.
