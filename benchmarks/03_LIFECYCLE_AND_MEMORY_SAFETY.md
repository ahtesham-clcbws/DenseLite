# 03: Model Lifecycle & Memory Safety Benchmark

**Date:** 2026-10-01  
**Status:** 🟢 VERIFIED  
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
| **ResourcePolicy VRAM limit Gate Enforcement** | Strict Gate | **1,740 MiB limit enforced** | 🟢 PASS |
| **Out-of-VRAM Fallback** | Deterministic | Graceful CPU/RAM allocation | 🟢 PASS |

---

## 2. RAII ModelLease Acquisition & Release Throughput

Model unmapping during active inference is prevented through reference-counted RAII handles (`ModelLease`). Models cannot be evicted or unmapped while `active_users > 0`.

| Metric | Iteration Count | Measured Throughput | Latency per Cycle | Status |
|---|:---:|:---:|:---:|:---:|
| **RAII Lease Acquire / Release** | 100,000 cycles | **5,396,617 ops/sec** | **0.185 µs** | 🟢 PASS |
| **Concurrency Contention Overhead** | Multi-threaded | Atomic spin-wait $< 12$ ns | Zero deadlock | 🟢 PASS |

---

## 3. Bounded Memory Allocations & Resident Pool Fit

KV cache buffers, scratch workspaces, and activation tensors are statically bounded prior to inference:

```text
KV Cache Buffer (28 Layers, 8192 Context @ FP16):
- Theoretical Size:     224.00 MiB
- Actual Allocation:    224.00 MiB (0.00 ms allocation time)
- Deallocation Time:    < 0.01 ms
- Memory Leaks:         Bounded drift within strict CI limits
```

### Resident Pool Capacity Fit (`test_resident_pool_fit`)
- **Simulated Resident Weights:** 1,100.0 MiB buffer (mimicking a resident 1.5B Q4_0 model).
- **Concurrent Context Streams:** 4 streams $\times$ 4K context (56.0 MiB per stream = 224.0 MiB total).
- **Process Memory Metrics:** Baseline RSS: 1,326.7 MiB, Peak VmHWM: **1,552.7 MiB** (well within the $\le 2,048.0$ MiB bound).
- *Open Capability Milestone:* Concurrent multi-model loaded residency under `ModelManager` remains an open milestone.

---

## 4. Bounded Resource Drift Assertions

Soak and stress testing enforces empirical bounds to prevent resource exhaustion:
- **RSS Memory Drift:** $\Delta\text{RSS} \le 150$ MB upper bound across extended test runs.
- **File Descriptors:** $\Delta\text{FD} \le 2$ upper bound.
- **Thread Count Stability:** $\Delta\text{Threads} \le 1$ drift upper bound.

---

## 5. Model State Machine Verification

```
COLD ──(Demand Load)──► LOADING ──(Success)──► HOT
                          │                      │
                          │ (Fail)               │ (Idle Timeout)
                          ▼                      ▼
                        FAILED                 EVICTED
```
