# DenseLite Benchmark: Frozen Hardware & Environment Configuration

**Phase:** Phase 0 Baseline Audit  
**Date:** 2026-09-25  
**DenseLite Git SHA:** `b78ab122eca24169893259f93633555731295484`

---

## 1. System Hardware

| Property | Value |
|---|---|
| **CPU Model** | Intel(R) Core(TM) i7-6500U CPU @ 2.50GHz |
| **Physical Cores** | 2 |
| **Logical Threads** | 4 (SMT Enabled) |
| **Total RAM** | 30.8 GiB (DDR3/DDR4, 32 GB physical) |
| **Swap** | 46.0 GiB |
| **SIMD Extensions** | AVX2, FMA3, F16C, BMI1, BMI2, SSE4.2 (Verified via CPUID) |
| **Max Process Ceiling** | 2 OpenMP worker threads, 14,117 MB RAM (Hard ceiling) |

---

## 2. Operating System & Toolchain

| Tool / Layer | Version / Value |
|---|---|
| **Operating System** | CachyOS Linux (Arch Linux derivative) |
| **Kernel** | `7.2.6-1-cachyos` (x86_64, SMP PREEMPT_DYNAMIC) |
| **C/C++ Compiler** | GCC / G++ `16.2.1 20260810` |
| **CMake** | `4.4.3` |
| **CMAKE_CXX_FLAGS** | `-O3 -mavx2 -mfma -mf16c -Wall -Wextra -Wpedantic` |
| **Linkage** | Static (`librocksdb.a`, `libarrow.a`, `libzvec.a`, `libantlr4-runtime.a`), Dynamic (`libssl.so.3`, `libcrypto.so.3`, `libsqlite3.so`, OpenMP) |

---

## 3. Model Inventory & Cryptographic Hashes

All models reside in `/mnt/apollo/Apollo4/DenseLite/models/` and are mapped sequentially into memory via POSIX `mmap()`:

| Model ID | File Name | Size | SHA256 Checksum |
|---|---|---|---|
| **nomic** | `nomic-embed-text-v1.5.Q8_0.gguf` | 140 MB | `3e24342164b3d94991ba9692fdc0dd08e3fd7362e0aacc396a9a5c54a544c3b7` |
| **qwen_main** | `Qwen2.5-1.5B-Instruct-abliterated.Q8_0.gguf` | 1.6 GB | `081d107d44fa7ffef717c0ebdcb689b80ff07ddb27157cc73aced01b63e5a92b` |
| **qwen_coder** | `Qwen2.5-Coder-1.5B-Instruct-abliterated-Q8_0.gguf` | 1.8 GB | `15492089c47f1c36f3a548c067ba20264e377d54dd559292bfaed39eb33bf0d4` |
| **smollm2** | `SmolLM2-360M-Instruct-Q8_0.gguf` | 369 MB | `c004ac34cce45e03e4452bca472535bc371fcb0d07ae560bba2c57ce4a284b7b` |
| **needle** | `needle3/` (Directory) | — | Non-GGUF stub classifier |
