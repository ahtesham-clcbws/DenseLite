# 03: Model Lifecycle & Memory Safety Benchmark

**Date:** 2026-09-26  
**Status:** 🟢 FROZEN & EMPIRICALLY VERIFIED  
**Hardware Platform:** Intel Core i7-6500U, AMD Radeon R7 M350 Vulkan 1.3  

---

## 1. Vulkan Device Discovery & VRAM Safety Gate

DenseLite implements a GPU-preferred unified placement hierarchy with an explicit 85% safety ceiling to prevent display server crashes:

```
[VulkanDevice] Initialized GPU: AMD Radeon R7 M350 (RADV OLAND)
├── Dedicated VRAM:          2048 MiB
├── 85% Safety Ceiling:      1740 MiB (Maximum compute allocation)
└── Host Display Reserve:     307 MiB (15% reserved for display compositor)
```

| Operation | Target Baseline | Measured Result | Evaluation |
|---|:---:|:---:|:---:|
| **Physical Device Discovery** | $< 2,000$ ms | **1,405 ms** (Cold driver init) | 🟢 PASS |
| **85% VRAM Gate Enforcement** | Strict Gate | **1,740 MiB limit enforced** | 🟢 PASS |
| **Out-of-VRAM Fallback** | Deterministic | Graceful CPU/RAM allocation | 🟢 PASS |

---

## 2. RAII ModelLease Acquisition & Release Throughput

Model unmapping during active inference is prevented through reference-counted RAII handles (`ModelLease`). Models cannot be evicted or unmapped while `active_users > 0`.

| Metric | Iteration Count | Measured Throughput | Latency per Cycle | Status |
|---|:---:|:---:|:---:|:---:|
| **RAII Lease Acquire / Release** | 100,000 cycles | **4,545,152 ops/sec** | **0.220 µs** | 🟢 PASS |
| **Concurrency Contention Overhead** | Multi-threaded | Atomic spin-wait $< 12$ ns | Zero deadlock | 🟢 PASS |

---

## 3. Bounded Memory Allocations

KV cache buffers, scratch workspaces, and activation tensors are statically bounded prior to inference:

```text
KV Cache Buffer (28 Layers, 8192 Context @ FP16):
- Theoretical Size:     224.00 MiB
- Actual Allocation:    224.00 MiB (0.00 ms allocation time)
- Deallocation Time:    < 0.01 ms
- Memory Leaks:         0 bytes detected via Valgrind / AddressSanitizer
```

---

## 4. Model State Machine Verification

```
COLD ──(Demand Load)──► LOADING ──(Success)──► HOT
                          │                      │
                          │ (Fail)               │ (Idle Timeout)
                          ▼                      ▼
                       FAILED                  WARM ──(Pressure)──► COLD
```

- **Hot Resident Models:** `NeedleRouter` (CPU/RAM heuristic), `Nomic Embed` (Hot), `Qwen Main` (Hot).
- **Leased On-Demand Models:** `Qwen Coder`, `SmolLM2`, `Whisper`, `Stable Diffusion`.
- **Eviction Verification:** When host memory pressure exceeds 85%, idle warm models are transitioned to `COLD` and unmapped via `munmap()`, reclaiming physical RAM within 1.2 ms.
