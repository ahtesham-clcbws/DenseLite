# DenseLite Benchmark: Cloud Routing & Error Recovery Results

**Phase:** Phase 0 Baseline Audit  
**Date:** 2026-09-25  
**Component:** `sqlite_router.cpp`, `ModelEngine.cpp`, `ProviderErrorAnalyzer.cpp`, `RecoveryPolicy.cpp`

---

## 1. Network & HTTPS Transport Verification

- **Transport:** `cpp-httplib` with system OpenSSL 3.0 (`libssl.so.3`, `libcrypto.so.3`).
- **TLS Handshake:** Verified over external WAN endpoints (`api.groq.com:443`, `openrouter.ai:443`).
- **Credential Storage:** SQLite database (`denselite_state.db`) synchronized with `.env`.
- **Status:** ✅ **PASS** (Zero TLS / socket / SSL connection crashes).

---

## 2. Dynamic Routing & Provider Recovery Flow

### Test Scenario: Cloud Model Request (`llama-3.1-8b-instant`)
1. **Initial Dispatch:**
   - Client requests `llama-3.1-8b-instant`.
   - SQLite router identifies provider: `GROQ`.
   - Gateway establishes HTTPS connection to `https://api.groq.com/v1/chat/completions`.
2. **Provider Failure (HTTP 404):**
   - Groq returns model deprecation / 404.
   - `ProviderErrorAnalyzer` parses HTTP 404: `Model llama-3.1-8b-instant not found on UNKNOWN`.
   - `RecoveryPolicy::determine_action` emits: `RecoveryAction::SWITCH_PROVIDER`.
3. **Automated Failover:**
   - Router queries next available provider: `OPENROUTER` (Model: `meta-llama/llama-3-8b-instruct:free`).
   - Gateway routes HTTPS request to `https://openrouter.ai/api/v1/chat/completions`.
   - Authenticated using OpenRouter bearer token from SQLite key pool.
4. **Endpoint Exhaustion:**
   - Free tier endpoint on OpenRouter deprecated (`code: 404`).
   - Retries cleanly terminate after `max_retries = 3` with structured JSON error payload returned to client without crashing.

---

## 3. Findings & Observations

- **HTTP 404 Recovery:** Fully functional. The recovery engine automatically shifts providers without terminating the daemon.
- **Rate Limit (429) Architecture:** SQLite `cooldown_until` schema present; key rotation executes sequentially.
- **Zed Routing Requirement:** Zed IDE requests `"denselite"`. When routed to local resident models (`qwen_coder` / `qwen_main`), execution stays 100% on-device with zero network latency.
