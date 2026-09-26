# Contributing to DenseLite

Thank you for your interest in contributing to DenseLite! This document provides
guidelines and information to make the contribution process smooth for everyone.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Architecture Constraints](#architecture-constraints)
- [Code Standards](#code-standards)
- [Submitting Changes](#submitting-changes)
- [Reporting Bugs](#reporting-bugs)
- [Feature Requests](#feature-requests)

## Code of Conduct

This project adheres to the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md).
By participating, you are expected to uphold this code. Please report unacceptable
behavior via [GitHub Issues](https://github.com/ahtesham-clcbws/DenseLite/issues).

## Getting Started

1. **Fork** the repository on GitHub.
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/<your-username>/DenseLite.git
   cd DenseLite
   ```
3. **Copy** the environment template:
   ```bash
   cp env.example .env
   ```
4. **Download** the required models (see `.env` for HuggingFace URLs).
5. **Build** the project:
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc) DenseLite
   ```

## Development Environment

### Prerequisites

| Requirement | Minimum Version |
|---|---|
| C++ Compiler | GCC 10+ or Clang 14+ (C++20 required) |
| CMake | 3.15+ |
| CPU | x86_64 with AVX2, FMA, and F16C support |
| RAM | 8 GB minimum (32 GB recommended for all models) |
| OS | Linux (tested on Arch/CachyOS) |
| SQLite3 | System library (`libsqlite3-dev`) |
| OpenMP | Required for parallel inference |

### Build Flags

DenseLite compiles with aggressive optimizations by default:

```
-O3 -mavx2 -mfma -mf16c -Wall -Wextra -Wpedantic
```

These are **not optional** — the AVX2 math kernels in `avx2_math.hpp` require
these instruction sets. Builds on non-AVX2 hardware will fail at link time or
crash at runtime.

### Running Tests

```bash
./test_e2e.sh
```

This will build, start the server, run integration tests, and shut down
automatically. Ensure no other process is bound to port `9501`.

## Architecture Constraints

DenseLite follows a strict architectural contract. **Read this before writing any
code:**

### The Golden Rules

1. **DenseLite controls intelligence. Zed controls execution.**
   DenseLite never executes tools, reads the filesystem, or manages workspace
   permissions. It only directs the IDE on what to do.

2. **Zero external runtime dependencies.**
   No Ollama, no llama.cpp, no MNN. The inference engine is 100% hand-rolled
   AVX2 C++ targeting Qwen 2.5.

3. **`server.cpp` is a thin gateway.**
   It must remain under 100 lines. All intelligence lives in `DenseLiteEngine`
   and its pipeline stages.

4. **Deterministic self-healing.**
   Error recovery uses SQLite-backed provider/model lookups — never LLM-based
   guessing.

### Pipeline Stages

Every request flows through these stages in order:

```
HTTP Gateway → RequestAnalyzer → NeedleRouter → MemoryEngine (Recall)
    → SearchEngine (Multi-Signal) → ContextEngine (BPE + ChatML)
    → ModelEngine → AgentLoop (ResponseAnalyzer + Recovery + Completion)
    → Curator → Formatter
```

Each stage is a separate class. Do not merge stages or add cross-cutting concerns.

### File Density Limits

| Range | Status |
|---|---|
| < 200 lines | ✅ Ideal — target for 90% of files |
| 200–500 lines | ⚠️ Standard — max for complex core logic |
| 500–1000 lines | 🔴 Warning — refactor required |
| > 1000 lines | 🚨 Critical — immediate modular breakdown |

## Code Standards

### General

- **C++20** standard (`-std=c++20`).
- **No raw `new`/`delete`** — use RAII, `std::unique_ptr`, or mmap.
- **No `std::regex`** — use `nlohmann/json` (already vendored in
  `dependencies/json.hpp`) or simple string operations.
- **Parameterized SQLite queries only** — no string-concatenated SQL.
- **All tensor pointers must be 32-byte aligned** for AVX2 safety.

### Naming Conventions

| Entity | Convention | Example |
|---|---|---|
| Classes | PascalCase | `ModelEngine` |
| Methods | snake_case | `get_fallback_model()` |
| Constants | UPPER_SNAKE | `QWEN_EOS_TOKEN` |
| Files (classes) | PascalCase | `DenseLiteEngine.cpp` |
| Files (systems) | snake_case | `sqlite_router.cpp` |
| Enum values | UPPER_SNAKE | `SessionStatus::COMPLETED` |

### Commit Messages

Use conventional commits:

```
feat(engine): add retry loop to recovery pipeline
fix(parser): handle array-type content in OpenAI messages
refactor(server): extract model loading into ModelLoader
docs: update architecture contract for V3.1
```

## Submitting Changes

### Pull Request Process

1. Create a **feature branch** from `main`:
   ```bash
   git checkout -b feat/my-feature
   ```
2. Make your changes, keeping commits atomic and well-described.
3. Ensure the E2E test suite passes:
   ```bash
   ./test_e2e.sh
   ```
4. Push to your fork and open a **Pull Request** against `main`.
5. In the PR description, include:
   - **What** the change does
   - **Why** it's needed
   - **How** it was tested
   - Any **architectural trade-offs** considered

### Review Criteria

PRs are evaluated against three vectors:

- **Performance** — Does it add latency to the inference path?
- **Maintainability** — Does it respect file density limits and SRP?
- **Minimalist Complexity** — Does it solve the problem without over-engineering?

### What We Won't Accept

- Changes that add external runtime dependencies (Ollama, llama.cpp, etc.)
- God files (> 500 lines without justification)
- Hardcoded file paths or API keys
- LLM-based error recovery (must be deterministic)

## Reporting Bugs

Open a [GitHub Issue](https://github.com/ahtesham-clcbws/DenseLite/issues) with:

1. **Environment:** OS, compiler version, CPU model (check AVX2 support)
2. **Steps to reproduce:** Exact commands or API requests
3. **Expected behavior** vs. **actual behavior**
4. **Logs:** Relevant `[Engine]`, `[ModelEngine]`, or `[GGUF]` console output
5. **Model files:** Which GGUF models were loaded (sizes, quantization)

## Feature Requests

Feature requests are welcome! Open a GitHub Issue with the `enhancement` label.
Include:

- **Use case:** What problem does this solve?
- **Proposed solution:** How should it work?
- **Alternatives considered:** What else was evaluated?
- **Scope impact:** Which pipeline stages would be affected?

---

Thank you for contributing to DenseLite! 🚀
