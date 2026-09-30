# 01: Hardware & Environment Audit

**Date:** 2026-09-26  
**Status:** HISTORICAL MEASUREMENT — revalidation required
**Target Environment:** Edge Laptop (Dual-Core Ultra-Low Voltage)  

> Evidence scope: historical measurements describe the dated implementation, not the current working tree. See [current validation](10_CURRENT_VALIDATION.md) for fresh logs and remaining gaps.

---

Current run detected **31,371 MiB** physical RAM and a **15,685 MiB** default policy ceiling; see [fresh evidence](10_CURRENT_VALIDATION.md). The figures below are historical.

## 1. Physical Hardware Specifications

```text
Processor:              Intel(R) Core(TM) i7-6500U CPU @ 2.50GHz
Architecture:           x86_64 (64-bit)
Physical Cores:         2
Hardware Threads:       4 (2 Threads / Core via SMT)
Base Frequency:         2.50 GHz
Max Turbo Frequency:    3.10 GHz
L1d Cache:              64 KiB (2 x 32 KiB)
L1i Cache:              64 KiB (2 x 32 KiB)
L2 Cache:               512 KiB (2 x 256 KiB)
L3 Cache:               4096 KiB (4 MiB shared)
System RAM:             31.2 GiB (32 GB DDR3/DDR4 dual-channel)
Historical configured RAM ceiling: 15,974 MiB (50% of the recorded 31.2 GiB host RAM).
Current default: ResourcePolicy caps allocations at 50% of detected host RAM (~15.6 GiB for the recorded 31.2 GiB host); lower settings are allowed.
```

---

## 2. CPU SIMD Instruction Set Architecture

```text
AVX (Advanced Vector Extensions):       Supported (256-bit SIMD registers YMM0–YMM15)
AVX2:                                   Supported (256-bit integer and floating-point SIMD)
FMA (Fused Multiply-Add):               Supported (Single-cycle a*b + c)
F16C:                                   Supported (Half-precision FP16 conversion)
SSE / SSE2 / SSE3 / SSSE3 / SSE4.1/4.2: Supported
BMI1 / BMI2:                            Supported (Bit manipulation intrinsics)
```

---

## 3. GPU Hardware & Vulkan 1.3 Driver Limits

DenseLite utilizes Vulkan 1.3 for GPU resource management, physical limits validation, and VRAM admission gating, with active transformer forward pass execution carried out on the AVX2+FMA SIMD CPU runtime.

```text
Selected Device:        AMD Radeon R7 M350 (RADV OLAND)
Driver:                 Mesa 25.0.0-arch1.1 (radv)
API Version:            Vulkan 1.3.303
Total Dedicated VRAM:   2,048 MiB
85% Safety Ceiling:     1,740 MiB (Strict hardware gate)
Host Display Reserve:   307 MiB (15% reserved for X11/Wayland display server)
Fallback GPU:           Intel(R) HD Graphics 520 (SKL GT2)
Placement Invariant:    GPU_PREFERRED with deterministic CPU_RAM fallback
```

---

## 4. Operating System & Toolchain

```text
OS Distribution:        Arch Linux / CachyOS Linux
Kernel Version:         6.13.5-zen1-1-zen (Preemptive Low-Latency Kernel)
Compiler:               GCC 15.2.1 20250208
C++ Standard:           ISO C++20 (`-std=c++20`)
Optimization Flags:     -O3 -mavx2 -mfma -fopenmp -march=native -fno-finite-math-only
Linker Flags:           -Wl,--as-needed -lvulkan -lsqlite3 -lssl -lcrypto
POSIX Subsystems:       Direct mmap with MAP_SHARED / MAP_PRIVATE, non-blocking /proc polling
```

---

## 5. Hardware Constraints & Resource Rules

1. **CPU Throttling Rule:** DenseLite operates under a strict 50% logical-thread ceiling to prevent OS starvation. For this 4-thread reference machine, this is realized as a strict maximum of half of hardware threads during forward passes or vector searches, ensuring at least 2 hardware threads remain available for the operating system, editor (Zed), and window compositor.
2. **RAM Safety Ceiling:** Host allocations are checked against the ResourcePolicy RAM ceiling before expanding context or admitting on-demand models.
3. **Display Server Protection:** The ResourcePolicy VRAM limit cap ensures desktop sessions never experience stutter or GPU resets.
