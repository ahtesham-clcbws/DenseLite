# DenseLite Benchmark: Phase 2 Model Lifecycle & Hardware Safety

**Phase:** Phase 2 Lifecycle, Role Management & Hardware Enforcement  
**Date:** 2026-09-26  
**Hardware:** Intel(R) Core(TM) i7-6500U (2 cores, 4 threads @ 2.50GHz), 32 GB Host RAM, AMD Radeon R7 M350 (2048 MiB Dedicated VRAM)  
**OS:** Linux (CachyOS / Niri)  

---

## 1. Executive Summary

Phase 2 replaces static global model loading with dynamic lifecycle management, Vulkan hardware discovery, RAII lease ref-counting with eviction guards, and strict memory budgeting. All metrics were measured directly on the host using `./build/benchmark_engine`.

---

## 2. Empirical Benchmark Results

| Component | Benchmark Metric | Measured Result | Target Threshold | Evaluation |
|---|---|---|---|---|
| **Vulkan Detection** | Device Identification & Limit Query | AMD Radeon R7 M350 (RADV OLAND) | Valid Vulkan 1.0+ Device | ✅ PASS |
| **VRAM Safety Gate** | 85% Ceiling Enforcement | 1740 MiB (15% / 307 MiB reserved) | Strict $\le 85\%$ VRAM | ✅ PASS |
| **Model Lease RAII** | Acquisition / Release Throughput | 3,681,845 ops/sec | > 500,000 ops/sec | ✅ PASS (0.272 µs/cycle) |
| **Eviction Guard** | Concurrency Safety & Mutex Contention | 100% block on active lease unload | Zero dangling pointers | ✅ PASS |
| **KV Cache Sizing** | Bounded Allocation (8192 tokens) | 224 MiB @ FP16 (28 layers, 2 KV heads) | Bounded, non-growing | ✅ PASS (0.00 ms alloc) |
| **Resource Governor** | `/proc` Snapshot & RSS Sampling | 73.47 µs per snapshot | < 500 µs | ✅ PASS |
| **Thread Enforcement** | OpenMP Physical Core Clamp | 2 physical threads | Max 2 threads (50% CPU) | ✅ PASS |

---

## 3. Placement & Safety Architecture

```
1. RESIDENCY & PLACEMENT HIERARCHY

GPU PREFERRED — Always attempt GPU first
├── Nomic Embed
├── Qwen Main
├── KV Cache
├── Activation / Scratch Buffers
├── Qwen Coder
├── SmolLM2
├── Whisper
└── SD / Image Models

CPU_RAM — Fallback when GPU admission fails
└── Any model, cache, or workload rejected by GPU budget/device limits

HOT
├── Needle → CPU_RAM
├── Nomic Embed → GPU_PREFERRED
└── Qwen Main → GPU_PREFERRED

LEASED / ON-DEMAND
├── Qwen Coder → GPU_PREFERRED (Evicts Qwen Main if needed)
├── SmolLM2 → GPU_PREFERRED
├── Whisper → GPU_PREFERRED
└── SD / Image Models → GPU_PREFERRED
```

1. **Vulkan Hardware Interrogation:** Dynamic query of `VkPhysicalDeviceLimits` exposes `minStorageBufferOffsetAlignment = 4` bytes and `maxStorageBufferRange = 4095 MB`.
2. **Eviction Guard Invariant:** When `active_users > 0`, `ModelPool::remove_model()` immediately rejects unmapping requests, eliminating use-after-free and SIGSEGV panics during concurrent inference.
3. **Bounded KV Cache Invariant:** $\text{KV Bytes} \le \text{Effective Context Tokens} \times \text{KV Bytes Per Token}$. Pre-calculated and bounded to prevent host or device OOM crashes.
