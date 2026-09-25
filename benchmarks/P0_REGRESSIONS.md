# DenseLite Benchmark: Known Limitations & Regression Test Cases

**Phase:** Phase 0 Baseline Audit  
**Date:** 2026-09-25  

---

## 1. Active Regressions & Limitations (Queue for P1–P3)

### REG-001: SmolLM2 SIGSEGV via Hardcoded MLP Dimension
- **Component:** `src/infer.cpp:136`
- **Behavior:** In `src/infer.cpp`, `int mlp_hidden_dim = 8960;` is hardcoded to Qwen2.5-1.5B dimensions. SmolLM2-360M has an intermediate dimension of 2560. When SmolLM2 is invoked, `matvec_q8` reads out-of-bounds memory on `ffn_down.weight`, causing an immediate segmentation fault.
- **Regression Test:**
  ```bash
  curl -s -X POST http://127.0.0.1:9501/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{"model":"smollm2","messages":[{"role":"user","content":"Hello"}]}'
  ```
- **Resolution Plan (Phase 1):** Read `intermediate_size` dynamically from GGUF metadata instead of hardcoding.

---

### REG-002: Malformed JSON Returns HTTP 200 Instead of HTTP 400
- **Component:** `src/RequestAnalyzer.cpp` & `src/server.cpp`
- **Behavior:** When invalid JSON is posted to `/v1/chat/completions`, `RequestAnalyzer::parse_openai_request` catches the `json::parse_error` and returns a default-constructed `OpenAIRequest`. The engine proceeds to run inference on an empty prompt, generating tokens until max context or EOS, returning HTTP 200.
- **Regression Test:**
  ```bash
  curl -s -w "%{http_code}" -X POST http://127.0.0.1:9501/v1/chat/completions \
    -H "Content-Type: application/json" -d "invalid json {"
  # Expected: 400 Bad Request
  # Actual: 200 OK
  ```
- **Resolution Plan (Phase 3):** Implement strict request validation returning HTTP 400 immediately on parse errors.

---

### REG-003: Empty Messages Array Triggers Token Generation
- **Component:** `src/DenseLiteEngine.cpp`
- **Behavior:** When `messages: []` is received, prompt compilation yields an empty string, causing the model to generate random text for 256 tokens (~70s latency).
- **Regression Test:**
  ```bash
  curl -s -w "%{http_code}" -X POST http://127.0.0.1:9501/v1/chat/completions \
    -H "Content-Type: application/json" -d '{"model":"denselite","messages":[]}'
  # Expected: 400 Bad Request
  # Actual: 200 OK (Empty generation)
  ```
- **Resolution Plan (Phase 3):** Validate `!req.messages.empty()` before entering inference pipeline.

---

### REG-004: Context Truncation is Line/Char Heuristic Rather Than Token Budgeted
- **Component:** `src/ContextManager.cpp`
- **Behavior:** Context management currently calculates length via `chars / 4` estimation and performs crude message trimming.
- **Regression Test:** High token count inputs may exceed context limits or cause uneven truncation.
- **Resolution Plan (Phase 3):** Implement exact BPE token counting and dynamic context budgeting.

---

## 2. Resolved Regressions (Verified in Phase 0)

| ID | Issue | Resolution | Status |
|---|---|---|---|
| **FIX-001** | Tied word embeddings (`tie_word_embeddings = true`) in Qwen2.5 caused null pointer dereference and SIGSEGV when looking for `output.weight`. | Added fallback to `token_embd.weight` in `src/infer.cpp`. | ✅ Resolved |
| **FIX-002** | OpenSSL dynamic linkage missing in CMake, causing build errors on system SSL symbols. | Linked OpenSSL (`libssl`, `libcrypto`) in `CMakeLists.txt`. | ✅ Resolved |
| **FIX-003** | Zed client default request model `"denselite"` routed to dead Groq cloud model. | Updated `DenseLiteEngine.cpp` to map `"denselite"` to resident `qwen_coder` / `qwen_main`. | ✅ Resolved |
| **FIX-004** | OpenRouter endpoint missing `/api` path prefix, resulting in HTTP 404 HTML responses. | Added `/api` path routing in `ModelEngine.cpp`. | ✅ Resolved |
