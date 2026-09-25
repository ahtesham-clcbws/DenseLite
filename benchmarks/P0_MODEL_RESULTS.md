# DenseLite Benchmark: Per-Model Verification Results

**Phase:** Phase 0 Baseline Audit  
**Date:** 2026-09-25  
**Hardware:** Intel Core i7-6500U (2 cores, 4 threads, AVX2 enabled), 32 GB RAM

---

## 1. Summary of Resident Models

DenseLite boots 4 resident models sequentially via `mmap()`:

```
[Loader] Loading resident model: needle... (Stub loaded)
[Loader] Loading resident model: smollm2... (369 MB mapped)
[Loader] Loading resident model: nomic... (140 MB mapped)
[Loader] Loading resident model: qwen_main... (1.6 GB mapped)
[Loader] Loading resident model: qwen_coder... (1.8 GB mapped)
[Loader] All resident models loaded successfully in sequence.
```

Total idle memory footprint with all 4 models mapped into process address space: **3,964 MB RSS (~3.96 GB)**.

---

## 2. Individual Model Execution Audit

### Model 1: `qwen_coder` (Qwen2.5-Coder-1.5B-Instruct Q8_0)
- **Status:** ✅ **OPERATIONAL (PASS)**
- **Inference Kernel:** Custom AVX2 + FMA `matvec_q8` OpenMP 2-thread kernel.
- **Tied Word Embeddings:** `tie_word_embeddings = true`. Projects through `token_embd.weight` (resolved).
- **Prompt Tested:** `"def add(a, b):"`
- **Response:** Generated coherent Python docstring, implementation, and test call (`result = add(5, 3)`).
- **Tokens Generated:** ~104 tokens in 23.9s (~4.35 tokens/sec).
- **SSE Stream:** Valid chunks followed by `data: [DONE]`.

### Model 2: `qwen_main` (Qwen2.5-1.5B-Instruct Q8_0)
- **Status:** ✅ **OPERATIONAL (PASS)**
- **Inference Kernel:** Custom AVX2 + FMA `matvec_q8` OpenMP 2-thread kernel.
- **Tied Word Embeddings:** `tie_word_embeddings = true`. Projects through `token_embd.weight` (resolved).
- **Prompt Tested:** `"Say hi in 5 words"`
- **Response:** `"Hi there! How can I help you today?"` (8 tokens generated in 3.08s, ~2.6 tokens/sec).
- **SSE Stream:** Valid chunks followed by `data: [DONE]`.

### Model 3: `smollm2` (SmolLM2-360M-Instruct Q8_0)
- **Status:** ⚠️ **BLOCKED BY HARDCODED DIMS (SIGSEGV)**
- **Audit Discovery:** In `src/infer.cpp`, transformer dimensions are statically hardcoded to Qwen2.5-1.5B (`int mlp_hidden_dim = 8960;`).
- **Root Cause:** SmolLM2-360M uses intermediate size `2560`. Running `matvec_q8` on down-projection with `mlp_hidden_dim = 8960` causes out-of-bounds memory read and SIGSEGV.
- **Target Resolution:** Handled in Phase 1 (Native Transformer Runtime) via GGUF metadata dynamic dimension parsing (`intermediate_size`).

### Model 4: `nomic` (nomic-embed-text-v1.5 Q8_0)
- **Status:** ⚠️ **PASSIVE / HEURISTIC FALLBACK**
- **Audit Discovery:** Context manager applies semantic filtering stub (`ContextManager::optimize_context`), but embedding forward pass is not yet wired to a dedicated BERT transformer path. Heuristic truncation currently operates safely.
- **Target Resolution:** Phase 4A / Phase 5.

### Model 5: `needle` (Intent Router)
- **Status:** ⚠️ **HEURISTIC KEYWORD FALLBACK**
- **Audit Discovery:** `NeedleRouter::analyze_request` checks for model pointer; if unavailable, falls back cleanly to keyword classifier (`coding` vs `text`).
- **Target Resolution:** Phase 2 / Phase 6.
