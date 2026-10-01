# 06: Hybrid Search & Agentic Loop Benchmark

**Date:** 2026-10-01  
**Status:** 🟢 VERIFIED  
**Hardware Platform:** Intel Core i7-6500U @ 2.50GHz, 32 GB RAM  

---

## 1. Multi-Signal Hybrid Search & ResultFusion

DenseLite integrates 4 distinct retrieval channels into a unified candidate pool, ranked deterministically via `ResultFusion`:
$$\text{Score} = w_{\text{exact}} S_{\text{exact}} + w_{\text{lexical}} S_{\text{lexical}} + w_{\text{vector}} S_{\text{vector}} + w_{\text{structural}} S_{\text{structural}}$$

| Retrieval Channel | Underlying Engine | Measured Throughput | Latency per Query | Status |
|---|---|:---:|:---:|:---:|
| **ExactSearch** | Exact symbol / token map | **44,286 queries/sec** | **22.58 µs** | 🟢 PASS |
| **LexicalSearch** | BM25 Term Frequency | **21,808 queries/sec** | **45.86 µs** | 🟢 PASS |
| **VectorSearch** | TurboQuant SIMD Vector Index | **1,105,573 ops/sec** | **904.51 ns** | 🟢 PASS |
| **ResultFusion** | 40 candidates $\to$ Top 5 | **139,633 fusions/sec** | **7.16 µs** | 🟢 PASS |
| **End-to-End SearchEngine** | 4 Channels + Fusion | **13,457 queries/sec** | **74.31 µs** | 🟢 PASS |

---

## 2. Evidence-Based Autonomous Agent Loop

DenseLite acts as the intelligence core, reasoning through multi-turn agentic cycles with self-healing fault recovery:

```
MODEL RESPONSE ──► ResponseAnalyzer (5 States)
                          │
       ┌──────────────────┼──────────────────┬──────────────────┐
       ▼                  ▼                  ▼                  ▼
   TOOL_CALL        MODEL_CONTINUE      MODEL_ERROR          COMPLETE
       │                  │                  │                  │
(Return to Zed)     (Internal Loop)    RecoveryPolicy     CompletionPolicy
                                       (7 Actions)        (Evidence Gate)
```

| Agent Subsystem | Target Capability | Measured Throughput | Latency per Op | Status |
|---|---|:---:|:---:|:---:|
| **ResponseAnalyzer** | 5-State Multi-Schema Detection | **363,238 parses/sec** | **2.75 µs** | 🟢 PASS |
| **RecoveryPolicy** | 7-Action Fault Recovery | **9,204,000 decisions/sec** | **0.11 µs** | 🟢 PASS |
| **CompletionPolicy** | Evidence policy evaluation; effectiveness unmeasured | **61,407,333 evals/sec** | **0.02 µs** | 🟢 PASS |
| **Curator** | Evidence Consolidation & Citations | **123,630 consolidations/s**| **8.09 µs** | 🟢 PASS |

---

## 3. Self-Healing Recovery Policy Verification

| HTTP / Provider Error Code | Primary Action | Fallback Strategy | Status |
|---|---|---|:---:|
| **429 (Rate Limit)** | `SWITCH_KEY` | Rotates to warm backup API key in SQLite pool | 🟢 VERIFIED |
| **413 (Payload Too Large)** | `REDUCE_CONTEXT` | Truncates old history and retries same model | 🟢 VERIFIED |
| **404 (Model Not Found)** | `SWITCH_MODEL` | Auto-routes to alternative model on provider | 🟢 VERIFIED |
| **502 / 500 (Provider Down)**| `SWITCH_PROVIDER` | Fails over (e.g. Groq $\to$ OpenRouter $\to$ Local) | 🟢 VERIFIED |
| **Timeout / Unknown Error** | `FALLBACK_LOCAL` | Drops to local resident AVX2 Llama-3.2-1B / DeepSeek-R1 engine | 🟢 VERIFIED |
| **Unrecoverable Fault** | `FAIL_SESSION` | Emits structured diagnostic JSON error | 🟢 VERIFIED |

---

## 4. Session Tool Registry & Handshake Deduplication

Caches external MCP schemas per chat session on the initial handshake:
- **Deduplication:** Completely eliminates repeated 600 KB JSON schema re-parsing across multi-turn sessions.
- **Handshake Speed:** **552,828 handshakes/sec** ($1.81\text{ µs}$).

---

## 5. Selective Tool Extraction & Conversational Chat Pruning

- **Intent Evaluation:** Evaluates query intent: conversational inputs (`"hi"`, `"how are you?"`) have tool schemas stripped to 0, enabling instant sub-5ms AVX2 inference; coding tasks selectively receive only 1–2 relevant tools.
- **Extraction Latency:** **0.35 µs** (2,876,312 queries/sec).

---

## 6. DecisionEngine & ModernBERT Intent Routing

- **Deterministic Fast Heuristic Tier:** 86.7% accuracy on 15 canonical test cases, 100% memory/web recall flag accuracy, 3.44 µs evaluation latency.
- **ModernBERT Zero-Shot Intent Router:** Embedded ONNX Runtime C++ classifier (`ModernBERT-large-zeroshot-v2.0`) providing sub-10ms classification across coding, reasoning, audio, image, and compressor model domains.
