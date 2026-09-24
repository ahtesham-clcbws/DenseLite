# DenseLite

![Version](https://img.shields.io/badge/version-v3.0-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![C++](https://img.shields.io/badge/language-C++-blue.svg)
![AVX2](https://img.shields.io/badge/SIMD-AVX2%20%2B%20FMA-orange.svg)

**DenseLite** is a hyper-optimized, C++ based multi-model orchestration gateway designed to dynamically load, route, and execute large language models, vector embeddings, speech recognition, and image generation locally. It acts as an incredibly fast, edge-optimized "local brain".

> **DenseLite controls intelligence. Zed controls execution.**
> **DenseLite may reason about tools, but DenseLite never executes tools.**

DenseLite operates as a pure **Agentic Inference Engine**. It does not execute bash commands, it does not read the filesystem, and it does not manage workspace permissions. Zed (the IDE) acts as the external harness that manages the environment, tools, and execution. DenseLite acts as the brain that directs Zed on what to do.

With V3.0, DenseLite has evolved from a smart proxy into an embedded RAG-powered, self-healing local intelligence core.

---

## Features

- **Dynamic Hot-Loading**: Models are swapped in and out of RAM on-demand to protect hardware limits.
- **Architecture Agnostic GGUF Parsing**: Capable of dynamically detecting and parsing `.context_length`, `.block_count`, and other metadata for *any* standard GGUF architecture out-of-the-box (Qwen, Llama, Nomic, Mistral, etc.).
- **Multi-Modal Support**: Integrated pipelines for text (Qwen / SmolLM), speech-to-text (Whisper), image generation (Stable Diffusion 1.5), and vector embeddings (Nomic).
- **Semantic Intent Routing**: Employs a specialized "Needle 3" router to intelligently classify intent and route requests using structured JSON assessments.
- **Deterministic Self-Healing**: Provider error recovery uses SQLite-backed model/key lookups — never LLM-based guessing.
- **Embedded Vector Search**: Alibaba Zvec provides sub-millisecond semantic retrieval over conversation history.
- **Custom AVX2 Inference Engine**: A 100% hand-rolled Transformer engine (`infer.cpp`) with raw Intel intrinsics for Q8_0 dequantization, RoPE, and SwiGLU.
- **Hardware Enforced**: Built-in strict hardware limit enforcement (50% CPU, 45% RAM) to prevent OOM on edge hardware.
- **No Path Hardcoding**: Auto-resolves execution environments dynamically.

---

## How It Works

When Zed sends a request to DenseLite, it flows through a deterministic pipeline of specialized C++ modules. Each module has a single responsibility and a clean interface boundary.

### Request Lifecycle

```mermaid
flowchart TD
    A["🖥️ Zed IDE sends POST /v1/chat/completions"] --> B["📥 server.cpp\n(HTTP Gateway)"]
    B --> C["🔍 RequestAnalyzer\nParse JSON → OpenAIRequest"]
    C --> D["🧠 NeedleRouter\nClassify intent via local LLM"]
    D --> E{"Intent Type?"}

    E -->|"reasoning"| F["📐 High-complexity path"]
    E -->|"coding"| F
    E -->|"text"| G["💬 Standard path"]
    E -->|"image"| H["🎨 Image generation path"]

    F --> I["🗜️ ContextManager\nZvec semantic filter + Nomic Embed"]
    G --> I
    H --> I

    I --> J["⚡ ModelEngine"]
    J --> K{"Cloud or Local?"}

    K -->|"Cloud API available"| L["☁️ CloudAdapter\nGroq / OpenRouter / Gemini"]
    K -->|"Offline / all keys exhausted"| M["🔧 LocalInference\nAVX2 infer.cpp\nQwen 2.5 1.5B"]

    L --> N["📊 ResponseAnalyzer"]
    M --> N

    N --> O{"Response Type?"}

    O -->|"COMPLETE"| P["✅ CompletionPolicy\nVerify task satisfaction"]
    O -->|"TOOL_CALL"| Q["🔨 Format tool request\nReturn to Zed for execution"]
    O -->|"INCOMPLETE"| R["🔄 Re-enter pipeline\n(multi-turn loop)"]
    O -->|"MODEL_ERROR"| S["🚨 ProviderErrorAnalyzer"]

    P --> T["📝 Curator\nConsolidate multi-turn results"]
    T --> U["📤 Formatter\nSSE stream → Zed"]

    Q --> V["Zed executes tool\nReturns result via HTTP"]
    V --> C

    S --> W["🔀 RecoveryPolicy"]
    W --> X{"Recovery Action?"}

    X -->|"SWITCH_PROVIDER"| Y["SQLiteRouter\nGet next provider + key"]
    X -->|"SWITCH_MODEL"| Z["SQLiteRouter\nGet fallback model"]
    X -->|"FALLBACK_LOCAL"| M
    X -->|"FAIL_SESSION"| AA["❌ Return error to Zed"]

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
    A["Cloud API\nreturns error"] --> B{"HTTP Status?"}

    B -->|"429"| C["Rate Limit\n5-min cooldown on key"]
    B -->|"404"| D["Model Not Found\nSwitch to fallback model"]
    B -->|"5xx"| E["Server Error\n1-min cooldown + retry"]
    B -->|"DNS Fail"| F["Network Down\nFallback to local AVX2"]

    C --> G["SQLiteRouter\nRound-robin next key"]
    D --> H["SQLiteRouter\nget_fallback_model()"]
    E --> G
    F --> I["LocalInference\ninfer.cpp"]

    G --> J["Retry with\nnew credentials"]
    H --> J
    I --> K["Generate locally\nQwen 2.5 1.5B"]

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
    B --> C["HardwareManager\nEnforce 50% CPU / 45% RAM"]
    C --> D["Load resident models\nsequentially into RAM"]

    D --> E["needle3.cact\n(Intent Router)"]
    D --> F["SmolLM2-360M\n(Context Compression)"]
    D --> G["Nomic Embed\n(Vector Embeddings)"]
    D --> H["Qwen 2.5 1.5B\n(Main Local Brain)"]
    D --> I["Qwen 2.5 Coder\n(Code Specialist)"]

    E --> J["GGUF Parser\nmmap + 32-byte align"]
    F --> J
    G --> J
    H --> J
    I --> J

    J --> K["SQLiteRouter\nInit DB + seed models"]
    K --> L["HTTP Server\nListening on :9501"]

    style A fill:#4a9eff,color:#fff
    style L fill:#00b894,color:#fff
```

---

## Quick Start


### 1. Setup Environment

Clone the repository and copy the example environment file:
```bash
git clone https://github.com/ahtesham-clcbws/DenseLite.git
cd DenseLite
cp env.example .env
```

### 2. Download Models

All required models should be placed inside a `models/` directory at the root of the project.
You can find the direct HuggingFace download links for each supported model inside the `env.example` file. Simply download the `.gguf`, `.safetensors`, and `.bin` files and place them according to the `MODEL_*_FILE` paths defined in the environment.

### 3. Build from Source

DenseLite uses CMake and heavily depends on AVX2/FMA optimizations for its internal inference engine and Zvec vector database.

```bash
mkdir build
cd build
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..
make -j4 DenseLite
```

### 4. Run the Gateway

Execute the compiled binary from the `build` directory:
```bash
./DenseLite
```
The API server will spin up on `http://localhost:9501`.

### 5. Run E2E Tests

```bash
./test_e2e.sh
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

### Directory Structure

```
DenseLite/
├── CMakeLists.txt              # Master build script
├── .env                        # Runtime configuration (NOT committed)
├── env.example                 # Environment template
│
├── src/                        # The Intelligent Core
│   ├── server.cpp              # The Gateway — thin HTTP listener
│   ├── DenseLiteEngine.*       # The Orchestrator — owns InferenceSession
│   ├── RequestAnalyzer.*       # The Payload Parser — JSON → OpenAIMessage
│   ├── NeedleRouter.*          # The Semantic Brain — intent classification
│   ├── sqlite_router.*         # The Deterministic Registry — keys & cooldowns
│   ├── ProviderErrorAnalyzer.* # The Error Interceptor — 400/404/429 handling
│   ├── RecoveryPolicy.*        # The Recovery Decider — retry strategy
│   ├── ContextManager.*        # The Memory Slicer — Zvec + Nomic embeddings
│   ├── ModelEngine.*           # The Inference Abstraction — cloud vs local
│   ├── ResponseAnalyzer.*      # The Output Classifier — TOOL_CALL/COMPLETE/ERROR
│   ├── CompletionPolicy.*      # The Completion Verifier — task satisfaction check
│   ├── Curator.*               # The Consolidator — multi-turn merge
│   ├── Formatter.*             # The Protocol Serializer — OpenAI JSON/SSE
│   ├── ProviderSync.*          # The Model Syncer — provider catalog refresh
│   ├── HardwareManager.hpp     # The Resource Governor — CPU/RAM budgets
│   │
│   ├── infer.cpp / .hpp        # The Transformer Core — GQA generation loop
│   ├── avx2_math.hpp           # The Math Kernels — raw AVX2 intrinsics
│   ├── gguf_parser.cpp / .hpp  # The Weight Loader — mmap GGUF parser
│   ├── model.hpp               # The Data Structures — Tensor, DenseModel
│   └── httplib.h               # The Network Layer — single-header HTTP
│
├── dependencies/
│   ├── json.hpp                # nlohmann/json (vendored)
│   └── zvec/                   # Alibaba Zvec vector database (submodule)
│
└── models/                     # Local model weights (not committed)
    ├── Qwen2.5-1.5B-*.gguf
    ├── Qwen2.5-Coder-1.5B-*.gguf
    ├── SmolLM2-360M-*.gguf
    ├── nomic-embed-text-*.gguf
    ├── ggml-base.en.bin
    └── needle3/
```

---

### Conceptual Architecture

```mermaid
graph TD
    %% ZED LAYER
    subgraph Zed[Zed IDE]
        Z1[Workspace]
        Z2[Files]
        Z3[Terminal]
        Z4[MCP / Skills]
        Z5[Tool Execution]
    end

    Zed -- "OpenAI-compatible API" --> Engine

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

        RespAnalyze -->|TOOL_CALL| Zed
        RespAnalyze -->|INCOMPLETE| Session
        RespAnalyze -->|MODEL_ERROR| Recovery[ProviderErrorAnalyzer]
        Recovery --> RecPolicy[RecoveryPolicy]
        RecPolicy --> SqlRouter[SQLiteRouter]
        SqlRouter --> ModEng

        RespAnalyze -->|COMPLETE| CompPolicy[CompletionPolicy]
        CompPolicy -->|Not Acceptable| Session
        CompPolicy -->|Acceptable| Curate[Curator]

        Curate --> Format[Formatter]
        Format --> Zed
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
> **User:** Hits "Generate Script" in Zed.
> **Problem:** The primary Groq API key hits a rate limit (HTTP 429).
> **Resolution:** `ProviderErrorAnalyzer` catches the 429, logs a 60-second cooldown in `sqlite_router`, and instantly maps to the next best available model (e.g., OpenRouter Llama-3). The user experiences a sub-second delay.

#### Scenario 3: The Tool Continuation Loop
> **User:** "Find all TODO comments and fix them."
> **Resolution:** DenseLite's `ResponseAnalyzer` identifies a `TOOL_CALL` request → formats JSON → passes to Zed → Zed executes `grep` and edits → returns "Success" via HTTP POST → DenseLite resumes the `InferenceSession` → loops until `COMPLETE`.

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
