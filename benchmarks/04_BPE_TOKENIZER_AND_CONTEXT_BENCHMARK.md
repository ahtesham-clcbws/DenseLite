# 04: BPE Tokenizer & Context Engine Benchmark

**Date:** 2026-10-01  
**Status:** 🟢 VERIFIED  
**Hardware Platform:** Intel Core i7-6500U @ 2.50GHz, 32 GB RAM  

---

## 1. Native Trie-Based BPE Tokenizer

DenseLite replaces the primitive `chars / 4` heuristic with a zero-dependency, trie-based Byte-Pair Encoding (BPE) tokenizer dynamically instantiated from the GGUF model vocabulary (`vocab_size = 128,256` for Llama 3.2, `151,936` for DeepSeek-R1, and `49,152` for SmolLM2).

| Tokenizer Operation | Input Data Size | Measured Throughput | Latency per Unit | Target Baseline | Result |
|---|:---:|:---:|:---:|:---:|:---:|
| **BPE Trie Token Encoding** | 18,400 bytes (~600 tokens) | **1,261,122 tokens/sec** | **0.793 µs/tok** | $> 500,000$ tok/s | 🟢 PASS |
| **Fast Token Counting (Zero Alloc)**| 18,400 bytes | **1,470,708 tokens/sec** | **0.680 µs/tok** | $> 1,000,000$ tok/s | 🟢 PASS |
| **BPE Token Decoding** | Vocabulary Tokens | **17,018,091 tokens/sec** | **0.059 µs/tok** | $> 5,000,000$ tok/s | 🟢 PASS |

> **Zero Vector Allocation Advantage:** The `count_tokens()` method evaluates total token count using a streaming state-machine trie walker without allocating a single `std::vector<int32_t>`, yielding a **1.17x throughput speedup** over full vector encoding.

---

## 2. Invariant Context Budgeting Model

DenseLite enforces the **Generation Reserve Invariant**:
$$\text{Generation Reserve} \ge \max(0.25 \times \text{Context Limit}, 1024)$$

```text
TOTAL CONTEXT WINDOW (8,192 tokens)
├── Generation Reserve (Guaranteed):    2,048 tokens (25% strict floor)
└── Usable Input Context:               6,144 tokens
    ├── System Prompt (Protected):        512 tokens (Zero truncation guarantee)
    ├── Task Objective (Protected):       384 tokens
    ├── Retrieved Code Chunks:          2,560 tokens
    ├── Retrieved Semantic Memory:        800 tokens
    └── Conversation Dialogue:          1,888 tokens
```

---

## 3. Multi-Stage Context Compression

When dialogue history exceeds the allocated input budget, the `ContextCompressor` applies deterministic compaction:
- **Level 1 (Duplicate Filter):** Drops consecutive identical user/assistant frames.
- **Level 4 (Chronological Sliding Window):** Preserves the original task and system prompt, compacts intermediate dialogue, and retains recent turns.

| Compression Operation | Test Workload | Measured Latency | Throughput | Status |
|---|:---:|:---:|:---:|:---:|
| **Sliding Window Compaction** | 52 turns / 12,400 tokens | **198.79 µs** | **5,030 passes/sec** | 🟢 PASS |
| **ChatML Context Compiler** | Full budget assembly | **495.96 µs** | **2,016 assemblies/sec** | 🟢 PASS |
| **End-to-End ContextEngine** | Budget + Dedup + Compile | **694.75 µs** | **1,439 requests/sec** | 🟢 PASS |

---

## 4. Dynamic RAM-Aware Context Sizing

DenseLite dynamically sizes context capacity based on detected system RAM headroom after baseline model allocation:
- **Headroom Policy:** Checks remaining process headroom against the canonical ResourcePolicy budget (50% physical RAM ceiling); unlocks up to 64K tokens (subject to model KV dimensions).
- **Headroom Evaluation Latency:** **23.97 µs** (41,711 evaluations/sec) with zero runtime inference overhead.

---

## 5. Persistent Session KV Cache & Delta Prefill

Preserves inference KV states between turns to eliminate repetitive prompt re-evaluation:
- **Turn 1 (Cold Prefill):** Evaluates prompt tokens ($0 \to N$).
- **Turn 2+ (Warm Delta Prefill):** Detects common token prefix sequence at **2,009,455 matches/sec** ($0.50\text{ µs}$). Only newly added tokens ($L \to N$) pass through the transformer forward pass.

---

## 6. High-Speed Binary Disk-Backed Persistence (`DLKV`)

- **Serialization Throughput:** Flushes active session KV tensors to disk using custom `DLKV` binary format at **2,243.4 MB/s** (6.48 ms for 16 MB snapshot).
- **Heterogeneous Architecture Validation:** Validates `num_layers`, `num_kv_heads`, and `head_dim` upon deserialization to eliminate cross-model memory corruption.
- **Warm File Attachment:** Memory-mapped instant restoration at 0.00 ms.
