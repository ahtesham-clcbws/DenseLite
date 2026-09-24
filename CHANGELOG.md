# Changelog

All notable changes to the DenseLite project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
