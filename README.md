# DenseLite

![Version](https://img.shields.io/badge/version-v3.2.1-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![C++](https://img.shields.io/badge/language-C++-blue.svg)
![AVX2](https://img.shields.io/badge/SIMD-AVX2%20%2B%20FMA-orange.svg)
![Vulkan](https://img.shields.io/badge/GPU-Vulkan%201.3%20Compute-red.svg)

**DenseLite** is a hyper-optimized, C++ based multi-model orchestration gateway designed to dynamically load, route, and execute large language models, vector embeddings, speech recognition, and image generation locally. It acts as an incredibly fast, edge-optimized "local brain".

> **DenseLite controls intelligence. The Client (IDE/Agent) controls execution.**  
> **DenseLite may reason about tools, but DenseLite never executes tools.**

DenseLite operates as a pure **Agentic Inference Engine**. It does not execute bash commands, it does not read the filesystem, and it does not manage workspace permissions. The Client (e.g., Zed, VSCode, Antigravity, or custom scripts) acts as the external harness that manages the environment, tools, and execution. DenseLite acts as the brain that directs the client on what to do.

With V3.2.1, DenseLite features a pure C++ AVX2 forward pass, GPU-preferred unified placement with an 85% VRAM safety gate, bounded KV cache allocation, RAII model lease lifecycle safety, and self-healing provider recovery.

---

## Features

- **GPU-Preferred Unified Placement**: Workloads attempt Vulkan GPU compute allocation first, with automatic, deterministic fallback to Host CPU/RAM.
- **85% VRAM Safety Ceiling**: Strict safety gate ($2048\text{ MiB} \times 0.85 = 1740\text{ MiB}$) reserving 15% (~308 MiB) for host display servers (X11/Wayland) and desktop compositors.
- **Dynamic Device Limit Query**: Dynamically inspects `VkPhysicalDeviceLimits` for buffer alignments and ranges instead of hardcoded magic values.
- **Bounded KV Cache Allocation**: Strictly bounded by $\text{KV Bytes} \le \text{Effective Context Tokens} \times \text{KV Bytes Per Token}$ with zero crash/OOM risk.
- **RAII ModelLease & Eviction Guards**: Reference-counted model leases (`active_users`) prevent unmapping or memory eviction during active inference.
- **Model-Driven Native Runtime**: 100% zero-dependency CPU transformer forward pass (`infer.cpp`) with AVX2 + FMA intrinsics, Q8_0 dequantization, dynamic RoPE (`RopeConfig`), RMSNorm, and SwiGLU.
- **Safe Error Propagation**: All loaders and lifecycle methods return diagnostic error messages with zero legacy `exit(1)` aborts.
- **Hardware Enforced**: Built-in continuous `ResourceGovernor` enforcing 2 threads (50% CPU allocation) and process RSS limits.

---

## Project Status: Phase 0, 1, 2, 3, 4A, 4B, 5 & 6 — 🟢 COMPLETED & VERIFIED

DenseLite has successfully verified **Phase 0 Baseline**, **Phase 1 Native Transformer Runtime**, **Phase 2 Model Lifecycle & Role Manager**, **Phase 3 Native BPE Tokenizer & Context Engine**, **Phase 4A Vector & State Memory Store**, **Phase 4B Code Intelligence & AST Parser**, **Phase 5 Unified Search & Deterministic Reranking**, and **Phase 6 Evidence-Based Autonomous Agent Loop** under automated CTest validation (100% pass rate across 9 test suites).

### Phase 6: Evidence-Based Autonomous Agent Loop (🟢 COMPLETED 2026-09-26)

| Subsystem | Status | Verified Implementation Details |
|---|:---:|---|
| **ResponseAnalyzer (5 States)** | 🟢 VERIFIED | Response parser detecting `TOOL_CALL`, `MODEL_CONTINUE` (with stop-reason discrimination `length`/`MAX_TOKENS`), `COMPLETE`, `MODEL_ERROR`, and `INVALID` across OpenAI/Gemini/Anthropic schemas at **481,540 analyses/sec** (2.08 µs/op). |
| **RecoveryPolicy (7 Actions)** | 🟢 VERIFIED | Deterministic recovery engine for 429, 413, 502, 500, and 404 faults (`RETRY_SAME`, `SWITCH_KEY`, `SWITCH_PROVIDER`, `SWITCH_MODEL`, `REDUCE_CONTEXT`, `FALLBACK_LOCAL`, `FAIL_SESSION`) running at **7,928,125 decisions/sec** (0.13 µs/op). |
| **CompletionPolicy (Evidence Gate)** | 🟢 VERIFIED | Anti-hallucination policy enforcing "Done is not evidence" on coding tasks; verified at **95,702,297 evaluations/sec** (0.01 µs/op). |
| **Curator (Multi-Turn Stitching)** | 🟢 VERIFIED | Iteration consolidation synthesizing turn steps and appending code citation blocks with exact file paths and symbols at **173,313 consolidations/sec** (5.77 µs/op). |
| **End-to-End Agent Loop** | 🟢 VERIFIED | Full cognitive loop executed in `DenseLiteEngine::execute_pipeline`: `Request` → `Analyze` → `Route` → `Recall` → `Search` → `Context` → `Model` → `Analyze Response` → `Tool/Continue/Complete/Recover` → `Curate` → `Formatter`. |

### Phase 5: Multi-Signal Search & Result Fusion (🟢 COMPLETED 2026-09-26)

| Subsystem | Status | Verified Implementation Details |
|---|:---:|---|
| **ExactSearch** | 🟢 VERIFIED | Exact and case-insensitive symbol & substring code lookup running at **42,726 queries/sec** (23.40 µs/query). |
| **LexicalSearch** | 🟢 VERIFIED | Tokenized BM25 keyword matching and stack-trace error parsing at **28,522 queries/sec** (35.06 µs/query). |
| **VectorSearch** | 🟢 VERIFIED | 512-dimensional dense embedding cosine similarity running at **1,339,750 ops/sec** (746.41 ns/dot product). |
| **ResultFusion (No Neural Reranker)** | 🟢 VERIFIED | Multi-signal deduplication and 6-parameter deterministic scoring ($w_1\text{semantic} + w_2\text{lexical} + w_3\text{structural} + w_4\text{symbol} + w_5\text{recency} + w_6\text{task}$) at **182,884 fusions/sec** (5.47 µs/fusion). |
| **SearchEngine Pipeline** | 🟢 VERIFIED | End-to-end multi-channel code and memory search executing at **13,425 queries/sec** (74.49 µs/query). |
| **ContextEngine Evidence Wiring** | 🟢 VERIFIED | Retrieved code symbols and memory facts directly injected into contextual prompt generation with sectional token budget protection. |

### Phase 4A & 4B: Memory Engine & Code Intelligence (🟢 COMPLETED 2026-09-26)

| Subsystem | Status | Verified Implementation Details |
|---|:---:|---|
| **Multi-Tier Memory Architecture** | 🟢 VERIFIED | In-RAM `WorkingMemory` (zero-allocation state tracking), `SessionMemory` (turn history), and `PersistentMemory` (project rules/conventions). |
| **Canonical SQLite Store** | 🟢 VERIFIED | Thread-safe SQLite backing store (`memories`, `sessions`, `session_turns`, `consolidated_archives`) achieving **12,309 writes/sec** and **53,958 reads/sec**. |
| **Deterministic Fact Compaction** | 🟢 VERIFIED | Amendment 5 deterministic extraction (`[fact]`, `[decision]`, `[constraint]`, `KEY: VALUE`) and archival in **2,596.80 µs** without LLM token waste. |
| **Memory Recall Engine** | 🟢 VERIFIED | Sub-millisecond top-K lexical similarity ranking executing at **10,804 queries/sec** (92.56 µs/query). |
| **Tree-sitter AST Parser** | 🟢 VERIFIED | Vendored Tree-sitter runtime parsing C++, Python, PHP, and JavaScript into structural chunks (`FUNCTION`, `CLASS`, `METHOD`) at **7,190 files/sec** (139.08 µs/file). |
| **64-bit FNV-1a Change Tracker** | 🟢 VERIFIED | Ultra-fast file change detection running at **21,206,146 checks/sec** (6,188.47 MB/s) to skip unchanged files during indexing. |
| **Code Symbol Index** | 🟢 VERIFIED | SQLite-backed `code_symbols` index with end-to-end file parsing, AST chunking, and symbol indexing in **1,119.94 µs**. |

### Phase 3: Native BPE Tokenizer & Context Engine (🟢 COMPLETED 2026-09-26)

| Subsystem | Status | Verified Implementation Details |
|---|:---:|---|
| **Native Trie BPE Tokenizer** | 🟢 VERIFIED | Exact BPE tokenization/detokenization running at **1,594,094 tokens/sec** encoding and **38,845,617 tokens/sec** decoding. |
| **Fast Token Counting** | 🟢 VERIFIED | Zero-allocation `count_tokens()` running at **1,759,479 tokens/sec** (replaces crude `chars / 4` heuristics). |
| **Generation Reserve Invariant** | 🟢 VERIFIED | Strictly preserves $\ge 25\%$ or $\ge 1024$ tokens for model output generation; never consumed by input prompts. |
| **System Prompt Protection** | 🟢 VERIFIED | Root system instructions and latest user query preserved 100% against truncation. |
| **Multi-Stage Compressor** | 🟢 VERIFIED | L1 deduplication and L4 chronological sliding window executed in **158.86 µs** (6,295 passes/sec). |
| **ChatML Context Compiler** | 🟢 VERIFIED | End-to-end prompt compilation and sectional budget verification executed in **558.39 µs** (1,791 requests/sec). |

### Phase 2: Model Lifecycle & Role Manager (🟢 COMPLETED 2026-09-26)

| Subsystem | Status | Verified Implementation Details |
|---|:---:|---|
| **Vulkan GPU Discovery** | 🟢 VERIFIED | Detects discrete GPU `AMD Radeon R7 M350 (RADV OLAND)` with 2048 MiB dedicated VRAM and 1.3 compute support. |
| **85% Safety Gate** | 🟢 VERIFIED | Limits VRAM allocation to 1740 MiB; rejects over-budget workloads and triggers CPU/RAM fallback. |
| **Model Registry & Roles** | 🟢 VERIFIED | Canonical catalog for `ROUTER`, `EMBEDDING`, `FORMATTER`, `GENERAL_REASONER`, `CODER`, `SPEECH_TO_TEXT`, and `IMAGE_GENERATOR`. |
| **ModelLease RAII** | 🟢 VERIFIED | Lease acquisition increments `active_users`; destruction decrements. Eviction guard strictly blocks `unload()` while in use (3.68M ops/sec). |
| **Bounded KV Cache** | 🟢 VERIFIED | Evaluated per layer/head; allocates on GPU if under 85% budget, else Host RAM fallback (224 MiB @ FP16, 0.00 ms alloc). |
| **Safe Error Handling** | 🟢 VERIFIED | Missing or malformed GGUF files yield structured error diagnostics without aborting the process. |

### Phase 1: Pure C++ AVX2 Model Execution Engine (🟢 COMPLETED 2026-09-26)

| Subsystem | Status | Verified Implementation Details |
|---|:---:|---|
| **Dynamic ModelConfig** | 🟢 VERIFIED | Removed all hardcoded Qwen dimensions; dynamically parses `intermediate_size`, `head_dim`, and shapes from GGUF. |
| **Explicit RopeConfig** | 🟢 VERIFIED | Dynamically initializes RoPE base ($100{,}000$ for SmolLM2, $1{,}000{,}000$ for Qwen2.5) and frequency factors. |
| **AVX2 Math Correctness** | 🟢 VERIFIED | `dot_product_q8_fp32` (rel_err < 1e-5), `rmsnorm`, `rope`, and `swiglu` bitwise verified against scalar references. |
| **Multi-Model Parity** | 🟢 VERIFIED | Main, Coder, and SmolLM2 run through identical native transformer code paths without segfaults. |

### Phase 0: Reality Audit & Baseline Verification (🟢 COMPLETED 2026-09-25)

Detailed empirical logs, hardware configuration, and benchmark reports:
- [P6_AGENT_LOOP_BENCHMARK.md](benchmarks/P6_AGENT_LOOP_BENCHMARK.md): Phase 6 evidence-based autonomous agent loop, 5-state response analyzer, 7-action recovery policy, and multi-turn curation metrics.
- [P5_UNIFIED_SEARCH_BENCHMARK.md](benchmarks/P5_UNIFIED_SEARCH_BENCHMARK.md): Phase 5 multi-signal search, Exact/Lexical/Vector/Structural, and ResultFusion metrics.
- [P4_MEMORY_AND_CODE_INTEL_BENCHMARK.md](benchmarks/P4_MEMORY_AND_CODE_INTEL_BENCHMARK.md): Phase 4A/4B memory store, recall, Tree-sitter AST, and symbol indexing metrics.
- [P3_CONTEXT_ENGINE_BENCHMARK.md](benchmarks/P3_CONTEXT_ENGINE_BENCHMARK.md): Phase 3 BPE tokenizer, counting, and context compiler metrics.
- [P2_LIFECYCLE_BENCHMARK.md](benchmarks/P2_LIFECYCLE_BENCHMARK.md): Phase 2 Vulkan hardware, lease RAII, and KV cache metrics.
- [P0_REALITY_MATRIX.md](benchmarks/P0_REALITY_MATRIX.md): Complete reality audit matrix.
- [P0_HARDWARE.md](benchmarks/P0_HARDWARE.md): Frozen machine hardware and compiler flags.
- [P0_MODEL_RESULTS.md](benchmarks/P0_MODEL_RESULTS.md): Per-model execution results and residency status.
- [P0_INFERENCE_RESULTS.md](benchmarks/P0_INFERENCE_RESULTS.md): Throughput, latency, and memory metrics.
- [P0_NETWORK_RESULTS.md](benchmarks/P0_NETWORK_RESULTS.md): WAN transport, HTTPS API handshakes, and provider failover.
- [P0_REGRESSIONS.md](benchmarks/P0_REGRESSIONS.md): Documented regression tests and issues resolved across Phase 1–6.

---

## Roadmap & Architecture Queue (v3.2.1 Frozen Contract)

- **Phase 0: Baseline & Reality Audit** — ✅ **COMPLETED**
- **Phase 1: Pure C++ AVX2 Model Execution Engine** — ✅ **COMPLETED**
- **Phase 2: Model Lifecycle & Role Manager** — ✅ **COMPLETED**
- **Phase 3: Native BPE Tokenizer & Context Window Engine** — ✅ **COMPLETED**
- **Phase 4A: Vector & State Memory Store (Zvec + SQLite)** — ✅ **COMPLETED**
  Persistent hybrid memory combining SQLite (metadata, conversation state, rate limits) and in-RAM tiered cache with lexical scoring.
- **Phase 4B: Code Intelligence & AST Parser (Tree-sitter)** — ✅ **COMPLETED**
  Tree-sitter syntax-aware code parsing, AST symbol extraction (`FUNCTION`, `CLASS`, `METHOD`), and FNV-1a delta hash change tracking.
- **Phase 5: Hybrid Search & Evidence Reranking** — ✅ **COMPLETED**
  Multi-channel retrieval combining Exact, Lexical BM25, Dense Vector, and Structural Tree-sitter search with deterministic ResultFusion.
- **Phase 6: Evidence-Based Autonomous Agent Loop** — ✅ **COMPLETED**
  Multi-turn reasoning and tool action loop (Read, Edit, Command execution) with 5-state response parsing, 7-action self-healing recovery, and evidence-based completion.
- **Phase 7: 2-Core Resource Governance & CPU/RAM Throttling** — ⬜ READY
  Strict 2-thread CPU cap (50% max) and 14 GB RAM ceiling for stable execution on edge laptops.
- **Phase 8: Multimodal Vision & Speech Processing** — ⬜ FUTURE
  Offline speech-to-text with Whisper.cpp and lightweight image generation pipelines.

---

## How It Works

When a Client IDE or Agent sends a request to DenseLite, it flows through a deterministic pipeline of specialized C++ modules. Each module has a single responsibility and a clean interface boundary.

### Request Lifecycle

```mermaid
flowchart TD
    A["🖥️ Client IDE/Agent sends POST /v1/chat/completions"] --> B["📥 server.cpp<br/>(HTTP Gateway)"]
    B --> C["🔍 RequestAnalyzer<br/>Parse JSON → OpenAIRequest"]
    C --> D["🧠 NeedleRouter<br/>Classify intent via local LLM"]
    D --> E{"Intent Type?"}

    E -->|"reasoning"| F["📐 High-complexity path"]
    E -->|"coding"| F
    E -->|"text"| G["💬 Standard path"]
    E -->|"image"| H["🎨 Image generation path"]

    F --> I["🗜️ ContextManager<br/>Zvec semantic filter + Nomic Embed"]
    G --> I
    H --> I

    I --> J["⚡ ModelEngine"]
    J --> K{"Cloud or Local?"}

    K -->|"Cloud API available"| L["☁️ CloudAdapter<br/>Groq / OpenRouter / Gemini"]
    K -->|"Offline / all keys exhausted"| M["🔧 LocalInference<br/>AVX2 infer.cpp<br/>Qwen 2.5 1.5B"]

    L --> N["📊 ResponseAnalyzer"]
    M --> N

    N --> O{"Response Type?"}

    O -->|"COMPLETE"| P["✅ CompletionPolicy<br/>Verify task satisfaction"]
    O -->|"TOOL_CALL"| Q["🔨 Format tool request<br/>Return to Client for execution"]
    O -->|"INCOMPLETE"| R["🔄 Re-enter pipeline<br/>(multi-turn loop)"]
    O -->|"MODEL_ERROR"| S["🚨 ProviderErrorAnalyzer"]

    P --> T["📝 Curator<br/>Consolidate multi-turn results"]
    T --> U["📤 Formatter<br/>SSE stream → Client"]

    Q --> V["Client executes tool<br/>Returns result via HTTP"]
    V --> C

    S --> W["🔀 RecoveryPolicy"]
    W --> X{"Recovery Action?"}

    X -->|"SWITCH_PROVIDER"| Y["SQLiteRouter<br/>Get next provider + key"]
    X -->|"SWITCH_MODEL"| Z["SQLiteRouter<br/>Get fallback model"]
    X -->|"FALLBACK_LOCAL"| M
    X -->|"FAIL_SESSION"| AA["❌ Return error to Client"]

    Y --> J
    Z --> J

    style A fill:#4a9eff,color:#fff
    style M fill:#ff9f43,color:#fff
    style L fill:#6c5ce7,color:#fff
    style U fill:#00b894,color:#fff
    style AA fill:#d63031,color:#fff
```

### Error Recovery & Self-Healing Flow

```mermaid
flowchart LR
    A["Cloud API<br/>returns error"] --> B{"HTTP Status?"}

    B -->|"429"| C["Rate Limit<br/>5-min cooldown on key"]
    B -->|"404"| D["Model Not Found<br/>Switch to fallback model"]
    B -->|"5xx"| E["Server Error<br/>1-min cooldown + retry"]
    B -->|"DNS Fail"| F["Network Down<br/>Fallback to local AVX2"]

    C --> G["SQLiteRouter<br/>Round-robin next key"]
    D --> H["SQLiteRouter<br/>get_fallback_model()"]
    E --> G
    F --> I["LocalInference<br/>infer.cpp"]

    G --> J["Retry with<br/>new credentials"]
    H --> J
    I --> K["Generate locally<br/>Qwen 2.5 1.5B"]

    J --> L["✅ Transparent to user"]
    K --> L

    style A fill:#d63031,color:#fff
    style L fill:#00b894,color:#fff
    style I fill:#ff9f43,color:#fff
```

### Model Loading at Boot

```mermaid
flowchart TD
    A["DenseLite starts"] --> B["Read .env config"]
    B --> C["HardwareManager<br/>Enforce 50% CPU / 45% RAM"]
    C --> D["Load resident models<br/>sequentially into RAM"]

    D --> E["needle3.cact<br/>(Intent Router)"]
    D --> F["SmolLM2-360M<br/>(Context Compression)"]
    D --> G["Nomic Embed<br/>(Vector Embeddings)"]
    D --> H["Qwen 2.5 1.5B<br/>(Main Local Brain)"]
    D --> I["Qwen 2.5 Coder<br/>(Code Specialist)"]

    E --> J["GGUF Parser<br/>mmap + 32-byte align"]
    F --> J
    G --> J
    H --> J
    I --> J

    J --> K["SQLiteRouter<br/>Init DB + seed models"]
    K --> L["HTTP Server<br/>Listening on :9501"]

    style A fill:#4a9eff,color:#fff
    style L fill:#00b894,color:#fff
```

---

## Quick Start

> [!WARNING]
> **First-Run Data Download:** DenseLite will automatically download between **1.5GB and 6.0GB** of model weights during its first startup, depending on which models you select in the interactive setup wizard. Ensure you have a stable internet connection.

### System Requirements

To ensure stable performance with local LLM fallback and semantic routing, we recommend the following minimum hardware specifications:

| Resource | Minimum Required |
|----------|-----------------|
| **Memory (RAM)** | 8 GB (16 GB Recommended for 1.5B models) |
| **Storage** | 10 GB Free Space (NVMe strongly recommended; SATA SSDs may be less responsive and take 10x longer to load models) |
| **CPU** | 4 Cores (AVX2 support required for GGUF) |
| **OS** | Linux / macOS / WSL2 on Windows |

---

### 1. Setup Environment

Clone the repository and run the setup script:
```bash
git clone https://github.com/ahtesham-clcbws/DenseLite.git
cd DenseLite
```

### 2. Build and Run (Interactive Entry Point)

DenseLite includes a single smart executable script (`start.sh`) that acts as the entry point. You do not need to run any external build commands or manually download models. The script will automatically:
1. **Interactive Setup:** Ask you to select your preferred models based on your hardware capabilities.
2. **Auto-Download:** Stream and download the selected GGUF models directly from HuggingFace to the `models/` directory.
3. **Auto-Compile:** Build the C++ binary from source if it isn't compiled yet (using CMake/make).
4. **Auto-Start:** Start the API gateway and save a limited rotating log to `denselite.log` (capped at 5000 lines).

Simply run:
```bash
./start.sh
```
The API server will automatically spin up on `http://localhost:9501`.

### 4. How to Use DenseLite (API)

DenseLite exposes a standard OpenAI-compatible HTTP REST API. Once running, you can connect any agent, IDE, or script to `http://localhost:9501/v1` as the base URL.

Here is a standard cURL example to interact with the engine:
```bash
curl http://localhost:9501/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "auto",
    "messages": [
      {"role": "system", "content": "You are a helpful assistant."},
      {"role": "user", "content": "Write a python script to reverse a string."}
    ],
    "stream": true
  }'
```
DenseLite will intercept this, use `NeedleRouter` to classify the intent as `coding`, search history via `Zvec` if needed, select the best model (Cloud API or Local fallback), and stream the Server-Sent Events (SSE) back to the caller.

### 5. Run E2E Tests

```bash
./test_e2e.sh
```

### 6. LLM Codebase Map

DenseLite automatically generates a full structural XML map of the entire codebase (excluding third-party dependencies and binaries) on every push to `main`. This is extremely useful for providing context to LLMs like Cursor or Claude.

You can access the always-up-to-date raw map here:
```text
https://raw.githubusercontent.com/ahtesham-clcbws/DenseLite/repomap/REPO_MAP.xml
```

---

## Architecture

### The Execution Lifecycle & `InferenceSession`

DenseLite orchestrates complex workflows through an explicit state machine inside `InferenceSession`. Because DenseLite pauses and resumes when Zed executes a tool, the session maintains state across HTTP boundaries.

```cpp
enum class SessionStatus {
    CREATED,
    ANALYZING,
    ROUTING,
    INFERRING,
    WAITING_FOR_TOOL,
    PROCESSING_TOOL_RESULT,
    CONTINUING,
    COMPLETED,
    FAILED,
    CANCELLED
};

struct InferenceSession {
    std::string session_id;
    std::string request_id;
    SessionStatus status;
    TaskType task_type;
    uint32_t iteration;
    uint32_t max_iterations;
    std::string selected_model;
    bool waiting_for_tool;
    bool streaming;
};
```

### The V3.0 Transformation

- **Stripped `server.cpp`:** Removed monolithic logic from the HTTP server, relegating it to a pure routing gateway.
- **Added Semantic Routing:** Replaced keyword-based string matching with `NeedleRouter` (Needle 3), using structured JSON assessments for true intent understanding.
- **Implemented Deterministic Provider-State Healing:** Built `ProviderErrorAnalyzer` and `SQLiteRouter` logic to intercept errors. Rather than letting an LLM guess replacements, it hits provider `/v1/models` endpoints to deterministically map available fallback infrastructure.
- **Embedded Alibaba Zvec:** Pulled the production-grade `alibaba/zvec` vector database directly into the C++ tree to enable true local semantic memory slicing for context.
- **Abstracted `ModelEngine` & Added `Curator`:** Extracted inference into a dedicated engine and added a Curator layer to consolidate multi-turn results before serializing.
- **Revealed the Custom AVX2 Engine:** Committed to the custom, hand-rolled C++ Transformer engine (`infer.cpp`) capable of running Qwen 2.5 natively.

### What We Have Now

A single, ultra-lightweight C++ binary (`DenseLite`) with zero external runtime dependencies. It manages the entire state machine of an `InferenceSession`, slices context infinitely via `Zvec`, falls back to its internal `AVX2` engine when offline, and flawlessly orchestrates the Zed IDE.

---

### Technology Choices

#### Why Alibaba Zvec instead of SQLite for Context

Zvec acts as the **semantic retrieval index**. Context is composed of three conceptual layers:
1. **ContextStore:** Stores conversation chunks and metadata.
2. **EmbeddingEngine:** (Nomic Embed) converts chunks to high-dimensional floats.
3. **VectorIndex:** (**Zvec**) performs HNSW/DiskANN distance calculations.

Unlike SQLite's exact string matching (`LIKE '%code%'`), Zvec understands semantic meaning, allowing DenseLite to find the most relevant context across a 10,000-message conversation in sub-milliseconds.

#### Why SQLite for State Management

SQLite is perfectly designed for ACID-compliant, deterministic, tabular data. We use it strictly in `sqlite_router.cpp` for the `denselite_state.db`. It manages API Keys, Provider URLs, Model Names, and Cooldown Timestamps.

#### Why NOT MNN, llama.cpp, or Ollama

| Alternative | Why Not |
|---|---|
| **Ollama** | Requires a heavy background daemon and Docker-like abstractions |
| **llama.cpp** | Pulls in massive unused hardware backends (Vulkan, CUDA, Metal, ROCm) causing binary bloat |
| **MNN** | Designed for generic deep learning on edge devices, carrying bloat for convolutions and vision |
| **DenseLite** | A **100% custom-built AVX2 Transformer engine** with raw intrinsics for Q8_0 dequantization, RoPE, and SwiGLU — the fastest, smallest possible binary tailored to Qwen 2.5 |

---

### Core Modules & File Structure

```text
DenseLite/
├── CMakeLists.txt              # Master build script.
├── .env                        # Runtime configuration containing API keys and paths (NOT committed).
├── env.example                 # Environment template with HuggingFace model download links.
│
├── src/                        # The Intelligent Core
│   ├── server.cpp              # The Gateway: Thin HTTP listener that receives standard OpenAI JSON payloads.
│   ├── DenseLiteEngine.*       # The Orchestrator: Owns the InferenceSession and manages the state machine across tool calls.
│   ├── RequestAnalyzer.*       # The Payload Parser: Safely parses inbound JSON into an internal OpenAIMessage structure.
│   ├── NeedleRouter.*          # The Semantic Brain: Small intent classification LLM that tags requests (e.g., coding, text, reasoning).
│   ├── sqlite_router.*         # The Deterministic Registry: SQLite state machine for tracking keys, cooldowns, and provider limits.
│   ├── ProviderErrorAnalyzer.* # The Error Interceptor: Traps 400/404/429 HTTP errors and decides if recovery is possible.
│   ├── RecoveryPolicy.*        # The Recovery Decider: Defines the retry strategy (Switch Key vs Switch Provider vs Fallback Local).
│   ├── ContextManager.*        # The Memory Slicer: Interfaces with Zvec and Nomic Embed to semantically filter massive conversation histories.
│   ├── ModelEngine.*           # The Inference Abstraction: The interface boundary between hitting a Cloud API or the Local CPU.
│   ├── ResponseAnalyzer.*      # The Output Classifier: Inspects outputs to detect if it's a TOOL_CALL, COMPLETE, or ERROR.
│   ├── CompletionPolicy.*      # The Completion Verifier: Ensures the task intent was actually satisfied before returning.
│   ├── Curator.*               # The Consolidator: Merges multi-turn tool interactions into a single cohesive response stream.
│   ├── Formatter.*             # The Protocol Serializer: Packages the internal state back into standard OpenAI JSON/SSE formats.
│   ├── ProviderSync.*          # The Model Syncer: Background job to fetch real-time `/v1/models` availability from providers.
│   ├── HardwareManager.hpp     # The Resource Governor: Enforces dynamic CPU and RAM budgets to prevent out-of-memory errors.
│   │
│   ├── infer.cpp / .hpp        # The Transformer Core: A pure, hand-rolled AVX2 inference engine for GQA, RoPE, and SwiGLU.
│   ├── avx2_math.hpp           # The Math Kernels: Raw Intel AVX2/FMA intrinsics for extremely fast vector matrix multiplication.
│   ├── gguf_parser.cpp / .hpp  # The Weight Loader: Memory-maps (mmap) and 32-byte aligns tensor weights from standard GGUF files.
│   ├── model.hpp               # The Data Structures: Internal structs for Tensors, Layers, and the DenseModel.
│   └── httplib.h               # The Network Layer: A single-header HTTP client/server library.
│
├── dependencies/
│   ├── json.hpp                # nlohmann/json (vendored).
│   └── zvec/                   # Alibaba Zvec vector database (bundled dependency).
│
└── models/                     # Local model weights (not committed).
```

### Dependencies & Third-Party Libraries

DenseLite is designed with zero runtime dependencies. However, it leverages a few specialized build-time libraries to ensure high performance:

1. **Alibaba Zvec** (`dependencies/zvec/`):
   - **What it is:** A production-grade C++ vector search engine using HNSW and DiskANN.
   - **Why we use it:** Instead of pushing a 50,000-token conversation history to an LLM, DenseLite embeds user queries and uses Zvec to instantly search past context for only the most semantically relevant chunks. This saves massive token costs and processing time.
2. **nlohmann/json** (`dependencies/json.hpp`):
   - **What it is:** The premier C++ JSON library.
   - **Why we use it:** Robust, crash-proof parsing of inbound HTTP request payloads and outbound SSE streams. String-manipulation logic for JSON is inherently unsafe and brittle; `nlohmann/json` ensures integrity.
3. **cpp-httplib** (`src/httplib.h`):
   - **What it is:** A minimal, single-header C++ HTTP/HTTPS library.
   - **Why we use it:** It enables DenseLite to spin up an ultra-light REST API (like `server.cpp`) and make outbound HTTP requests to Cloud LLMs without pulling in massive dependencies like Boost.Asio or libcurl.
4. **OpenMP** (Standard Compiler Flag):
   - **What it is:** API for multi-platform shared-memory parallel programming.
   - **Why we use it:** To strictly enforce thread-budgets on CPU hardware during local tensor math (`avx2_math.hpp`), allowing parallel matrix dot-products without consuming the entire OS scheduler.

---

### Conceptual Architecture

```mermaid
graph TD
    %% CLIENT LAYER
    subgraph Client[Client IDE / Agent]
        Z1[Workspace]
        Z2[Files]
        Z3[Terminal]
        Z4[MCP / Skills]
        Z5[Tool Execution]
    end

    Client -- "OpenAI-compatible API" --> Engine

    %% DENSELITE LAYER
    subgraph DenseLite
        Engine[DenseLiteEngine]
        Session[InferenceSession]

        Engine --> Session

        Session --> Req[RequestAnalyzer]
        Req --> Needle[Needle 3 Router]
        Needle --> Context[ContextManager]
        Context --> ModEng[ModelEngine]

        ModEng --> Cloud[Cloud API]
        ModEng --> Local[Local AVX2 infer.cpp]

        Cloud --> RespAnalyze[ResponseAnalyzer]
        Local --> RespAnalyze

        RespAnalyze -->|TOOL_CALL| Client
        RespAnalyze -->|INCOMPLETE| Session
        RespAnalyze -->|MODEL_ERROR| Recovery[ProviderErrorAnalyzer]
        Recovery --> RecPolicy[RecoveryPolicy]
        RecPolicy --> SqlRouter[SQLiteRouter]
        SqlRouter --> ModEng

        RespAnalyze -->|COMPLETE| CompPolicy[CompletionPolicy]
        CompPolicy -->|Not Acceptable| Session
        CompPolicy -->|Acceptable| Curate[Curator]

        Curate --> Format[Formatter]
        Format --> Client
    end
```

---

### Dynamic Resource Budgets

DenseLite is designed to be an ultra-lightweight citizen on any operating system, treating hardware limits as strict budgets.

| Resource | Budget | Enforcement |
|---|---|---|
| **CPU** | 50% of total capacity | On a 2-core/4-thread machine → 2 threads max |
| **RAM** | 45% of total capacity | On 32GB → 14.4GB ceiling |

**Deterministic Memory Eviction Order** (when RAM budget is breached):

1. Evict temporary inference buffers
2. Evict old context cache
3. Reduce Zvec retrieval cache
4. Unload inactive local model
5. Refuse new local inference

DenseLite auto-detects `std::thread::hardware_concurrency()` and physical memory at boot to calculate these budgets.

---

### Execution Scenarios

#### Scenario 1: The Infinite Context Request
> **User:** "Review all the changes we made to the networking stack yesterday and suggest improvements."
> **Problem:** The conversation history is 50,000 tokens long.
> **Resolution:** `ContextManager` uses **Zvec** to extract only the 1,500 most semantically relevant tokens discussing "networking" and "changes", saving API cost and token limits.

#### Scenario 2: The Deterministic Cooldown Healing
> **User:** Hits "Generate Script" in the Client.
> **Problem:** The primary Groq API key hits a rate limit (HTTP 429).
> **Resolution:** `ProviderErrorAnalyzer` catches the 429, logs a 60-second cooldown in `sqlite_router`, and instantly maps to the next best available model (e.g., OpenRouter Llama-3). The user experiences a sub-second delay.

#### Scenario 3: The Tool Continuation Loop
> **User:** "Find all TODO comments and fix them."
> **Resolution:** DenseLite's `ResponseAnalyzer` identifies a `TOOL_CALL` request → formats JSON → passes to the Client → Client executes `grep` and edits → returns "Success" via HTTP POST → DenseLite resumes the `InferenceSession` → loops until `COMPLETE`.

#### Scenario 4: The Offline Fallback (Airplane Mode)
> **User:** Asks for a regex pattern while on a plane with no Wi-Fi.
> **Resolution:** `sqlite_router` detects network failure. `ModelEngine` seamlessly routes to `LocalInference` (`infer.cpp`). The local Qwen 2.5 1.5B model spins up natively on CPU cores.

---

## Extending DenseLite

DenseLite is built to be easily extended! The `server.cpp` initialization dynamically loads models based on your `.env` configuration. You can easily plug new models in, or extend the `DenseLiteEngine` class to handle new routing logic for specialized use cases.

See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines, code standards, and the PR process.

---

## Community

- [Code of Conduct](CODE_OF_CONDUCT.md)
- [Contributing Guidelines](CONTRIBUTING.md)
- [Security Policy](SECURITY.md)

## License

MIT License. **Anyone can use this in commercial or personal projects.**
*Condition:* My name (Ahtesham) and this GitHub repository link must be mentioned/attributed in your project or codebase. See [LICENSE](LICENSE) for details.
