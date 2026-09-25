# DenseLite Benchmark: Inference Performance & Resource Metrics

**Phase:** Phase 0 Baseline Audit  
**Date:** 2026-09-25  
**Hardware:** Intel(R) Core(TM) i7-6500U (2 cores, 4 threads @ 2.50GHz), 32 GB RAM

---

## 1. Measured Baseline Metrics

| Metric | Measured Value | Constraint Ceiling | Evaluation |
|---|---|---|---|
| **Startup Time (cold boot to listening)** | ~850 ms | < 2,000 ms | ✅ Excellent (POSIX `mmap` lazy load) |
| **Idle Resident RAM (all 4 models loaded)** | 3,964 MB (~3.96 GB) | < 14,117 MB | ✅ Well within 14 GB ceiling |
| **Peak RAM (during active token generation)** | 5,070 MB (~5.07 GB) | < 14,117 MB | ✅ Safe headroom (~9 GB remaining) |
| **Active OpenMP Threads** | 2 threads | 2 threads (Strict) | ✅ Strict hardware limit enforced |
| **First Token Latency (TTFT)** | ~400 ms | < 1,500 ms | ✅ Low prefill latency on short prompts |
| **Throughput (Qwen2.5-Coder-1.5B)** | 4.35 tokens/sec | > 3.0 tokens/sec | ✅ Smooth streaming on 2 physical cores |
| **Throughput (Qwen2.5-Main-1.5B)** | ~2.60–3.20 tokens/sec | > 2.5 tokens/sec | ✅ Consistent CPU AVX2 throughput |
| **Throughput (SmolLM2-360M)** | N/A (Blocked by P1 hardcoded dims) | — | ⚠️ Documented in P0_REGRESSIONS.md |

---

## 2. Resource Consumption Analysis

1. **Memory Allocation:**
   - Sequential model mapping via `mmap()` ensures physical pages are faulted in only when touched.
   - KV Cache allocated dynamically per request (`std::vector<float>` in `init_inference_state`).
   - Peak RSS reached ~5.07 GB when KV cache and working buffers for context are expanded.
2. **CPU Utilization:**
   - OpenMP parallel for loops in `avx2_math.hpp` strictly bind to 2 threads.
   - Total process CPU stabilizes at ~150–160% (out of 400% across 4 hyperthreads, i.e., ~40% total system load).
   - Machine remains completely responsive; no thermal throttling observed during continuous generation.
