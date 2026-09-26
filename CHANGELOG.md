# Changelog

All notable changes to the DenseLite project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.2.1] - 2026-09-26

### Added
- **Phase 2 Model Lifecycle & Role Manager**: Introduced `ModelRegistry` cataloging model roles (`ROUTER`, `EMBEDDING`, `FORMATTER`, `GENERAL_REASONER`, `CODER`, `SPEECH_TO_TEXT`, `IMAGE_GENERATOR`).
- **GPU-Preferred Unified Admission Control**: Automatically detects Vulkan compute GPUs (e.g. AMD Radeon R7 M350 / 2048 MiB) and enforces an 85% VRAM safety ceiling (1740 MiB) with 15% (~308 MiB) reserved for X11/Wayland display servers.
- **Dynamic Device Limits**: Queries `VkPhysicalDeviceLimits` at runtime for buffer alignments (`minStorageBufferOffsetAlignment: 4 bytes`) and buffer ranges without hardcoded magic numbers.
- **Bounded KV Cache (`kv_cache.{hpp,cpp}`)**: Enforces $\text{KV Bytes} \le \text{Effective Context Tokens} \times \text{KV Bytes Per Token}$ with GPU-preferred admission and safe Host RAM fallback.
- **RAII ModelLease & Eviction Guards (`model_lease.{hpp,cpp}`, `model_pool.{hpp,cpp}`)**: Reference-counted model leases (`active_users`) strictly block memory unmapping or model eviction during active inference.
- **Safe ModelLoader (`ModelLoader.hpp`, `model_loader.cpp`)**: Replaced all legacy `exit(1)` aborts with structured diagnostic error strings.
- **Continuous Resource Governor (`resource_governor.{hpp,cpp}`)**: Continuous monitoring of host RAM and GPU VRAM via Linux `/proc/meminfo` and `/proc/self/statm`.
- **Phase 2 Automated Tests (`tests/model_lifecycle_tests.cpp`)**: Validated Vulkan discovery, 85% safety gate, bounded KV cache, lease RAII, eviction guards, and error handling.

## [3.1.0] - 2026-09-26

### Added
- **Phase 1 Native Transformer Runtime**: 100% zero-dependency model-driven C++ AVX2 forward pass in `infer.cpp`.
- **Dynamic ModelConfig & RopeConfig (`model.hpp`)**: Eliminated hardcoded dimensions (`mlp_hidden_dim = 8960` removed); dynamically parses GGUF metadata for `intermediate_dim`, `head_dim`, and RoPE base frequency ($100{,}000$ for SmolLM2, $1{,}000{,}000$ for Qwen2.5).
- **SmolLM2-360M Compatibility**: Resolved legacy SIGSEGV crashes; SmolLM2, Qwen Main, and Qwen Coder run natively through identical code paths.
- **Phase 1 Test Suite**: Added `tests/math_correctness.cpp`, `tests/model_config_tests.cpp`, and `tests/golden_inference_tests.cpp` with 100% automated pass rate.

## [3.0.0] - 2026-09-25

### Added
- **Interactive Setup Wizard**: `start.sh` now operates as a complete, single-file entrypoint that dynamically asks the user for their preferred model tiers (Qwen, SmolLM2, Nomic) and automatically generates the `.env` configuration file.
- **Dynamic Resource Assessment**: `start.sh` automatically detects available system RAM to constrain model selections and calculates optimal CPU threading for execution.
- **Automated Weight Downloading**: `start.sh` dynamically downloads the required GGUF weights directly from HuggingFace without requiring the user to manually run `wget`.
- **System Warnings & Thresholds**: Added data size warnings and strict minimum/recommended hardware requirements to `README.md` (e.g., explicit recommendations for NVMe SSDs over SATA).

### Changed
- **Abliterated Models by Default**: Switched the default Qwen 2.5 (1.5B/0.5B) and Qwen 2.5 Coder models to their uncensored "abliterated" Q8_0 GGUF equivalents for maximum agentic flexibility.
- **Alibaba Zvec Integration**: `zvec` is now officially treated as a fully bundled C++ dependency rather than an external disjointed git submodule. Stripped internal nested `.git` references from third-party vendor directories (e.g., `RaBitQ-Library`) to ensure clean compilation.
- **GCC 13+ Compatibility (RocksDB/Zvec)**: Manually patched upstream missing `#include <cstdint>` headers in `checkpoint.h` and `wal_file.h` to fix standard library `uint64_t`/`uint32_t` definition compilation errors on modern Linux kernels.

### Fixed
- **Needle 3 Internalization**: Embedded Needle 3 resources internally (via Git LFS) directly into the repository, eliminating the need for users to fetch it from an external source.
- **Core Engine Compilation Errors**: Fixed a missing JSON nested object closing brace within `RequestAnalyzer.cpp`'s `messages` parsing loop block.
- **Header Declarations**: Added missing `#include "RequestAnalyzer.hpp"` header to `DenseLiteEngine.hpp` to resolve missing `OpenAIRequest` struct definitions during the final compilation phase.
