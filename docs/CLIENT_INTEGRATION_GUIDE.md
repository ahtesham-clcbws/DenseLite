# DenseLite Client Integration Guide

DenseLite exposes a standard **OpenAI-compatible HTTP Gateway** on `http://localhost:9501/v1`. Any client, IDE, or tool supporting custom OpenAI endpoints can seamlessly connect to DenseLite.

---

## Gateway Endpoints & Available Models

* **Base URL:** `http://localhost:9501/v1`
* **Chat Endpoint:** `POST http://localhost:9501/v1/chat/completions`
* **Models Catalog:** `GET http://localhost:9501/v1/models`
* **Health Check:** `GET http://localhost:9501/health`
* **API Key:** `Bearer <api_secret_key>` (or any string if authentication is disabled in Settings)

### Supported Model Identifiers & Roles
| Model ID / Role | Execution Engine | Best Use Case |
|---|---|---|
| `denselite` | **ModernBERT Auto-Router (Recommended)** | Zero-shot intent classification into general, coder, or cloud |
| `general` | Local C++ AVX2 (Llama-3.2-1B-Instruct) | General chat, summarization, conversational reasoning |
| `coder` | Local C++ AVX2 (DeepSeek-R1-Distill-1.5B) | Advanced coding, syntax reasoning, and refactoring |
| `compressor` | Local C++ AVX2 (SmolLM2-360M) | Context compression and memory consolidation |
| `router` | Embedded ONNX Runtime (ModernBERT-Large) | Sub-10ms intent classification and capability routing |
| `embedding` | Local C++ AVX2 (Nomic-Embed-Text-v2-MoE) | Dense semantic vectors & hybrid retrieval |
| `openai/gpt-oss-20b` | Groq Cloud | Ultra-fast cloud text & reasoning fallback |
| `gemini-2.5-flash` | Google Gemini Cloud | Complex cloud reasoning with auto-fallback |

---

## 1. Zed Editor Setup

### Method A: Via Zed UI
1. Navigate to: **`User` → `AI` → `General` → `LLM Providers` → `Add OpenAI-Compatible Provider`**.
2. Fill in the fields:
   * **Provider Name:** `DenseLite`
   * **API URL:** `http://localhost:9501/v1`
   * **API Key:** `denselite`
3. Click **`+ Add Model`**:
   * **Model Name:** `denselite` *(or `coder` / `general`)*
   * **Max Completion Tokens:** `4096`
   * **Max Output Tokens:** `4096`
   * **Max Tokens (Context):** `32768`
   * **Toggles:**
     * [x] **Supports tools**
     * [x] **Supports /chat/completions**

### Method B: Via `settings.json`
Add the following snippet directly to `~/.config/zed/settings.json`:

```json
{
  "language_models": {
    "openai_compatible": {
      "DenseLite": {
        "api_url": "http://localhost:9501/v1",
        "available_models": [
          {
            "name": "denselite",
            "max_tokens": 32768,
            "max_output_tokens": 4096
          },
          {
            "name": "coder",
            "max_tokens": 32768,
            "max_output_tokens": 4096
          },
          {
            "name": "general",
            "max_tokens": 32768,
            "max_output_tokens": 4096
          },
          {
            "name": "openai/gpt-oss-20b",
            "max_tokens": 131072,
            "max_output_tokens": 4096
          }
        ]
      }
    }
  }
}
```

---

## 2. OpenCode Setup (OpenCode v2)

> **Important Note for OpenCode v2 Users:**
> When running OpenCode Desktop or the `opencode-cli serve --service` daemon, adding custom providers through the UI modal triggers:
> `Request failed: Custom providers are unavailable on this server`
> OpenCode v2 requires custom OpenAI-compatible providers to be defined declaratively in your configuration file (`opencode.json` / `opencode.jsonc`), not via the UI modal.

### Configuration (`~/.config/opencode/opencode.json` or `./opencode.json`)
OpenCode v2 automatically searches and merges configuration from `~/.config/opencode/opencode.json` (global) and `./opencode.json` (project workspace).

Add the following provider specification:

```json
{
  "$schema": "https://opencode.ai/config.json",
  "model": "denselite/denselite",
  "providers": {
    "denselite": {
      "name": "DenseLite Local Brain",
      "package": "@opencode/ai/providers/openai-compatible",
      "settings": {
        "baseURL": "http://localhost:9501/v1"
      },
      "models": {
        "denselite": {
          "name": "DenseLite ModernBERT Router"
        },
        "coder": {
          "name": "DeepSeek-R1 Distill Qwen 1.5B (Local Coder)"
        },
        "general": {
          "name": "Llama 3.2 1B Instruct Abliterated (General Reasoner)"
        },
        "openai/gpt-oss-20b": {
          "name": "Groq GPT OSS 20B (Cloud)"
        }
      }
    }
  }
}
```

### Selecting Models in OpenCode
- **In Chat:** Type `/models` and select `denselite/denselite` or any configured model.
- **In Config:** Set `"model": "denselite/denselite"` or `"denselite/coder"` as your default.

### MCP Tools & 600 KB Schema Acceleration (v3.3.0)
DenseLite v3.3.0 includes native **Session Tool Registry** and **Persistent Session KV Cache**:
- **Zero-Timeout Chat:** When OpenCode attaches massive MCP tool collections (e.g., 600 KB `laravel-boost` schema), DenseLite caches the tools on the first handshake. General conversational messages (`"hi"`, `"how are you?"`) bypass schema injection completely, executing in sub-5ms instead of timing out.
- **Instant Turn 2+ Responses:** The persistent KV cache matches prompt prefixes with previous turns and performs **delta prefill only**, skipping redundant history evaluations.
- **Disk Resumption:** Sessions are checkpointed to `denselite_kv_cache/*.kv` at 2.1 GB/s, allowing conversations to resume across server reboots.

---

## 3. VS Code Setup (Continue / Cline / Roo Code)

For **Continue** (`~/.continue/config.json`):

```json
{
  "models": [
    {
      "title": "DenseLite Auto",
      "provider": "openai",
      "model": "denselite",
      "apiBase": "http://localhost:9501/v1",
      "apiKey": "denselite"
    },
    {
      "title": "DenseLite Coder",
      "provider": "openai",
      "model": "coder",
      "apiBase": "http://localhost:9501/v1",
      "apiKey": "denselite"
    }
  ],
  "tabAutocompleteModel": {
    "title": "DenseLite Autocomplete",
    "provider": "openai",
    "model": "coder",
    "apiBase": "http://localhost:9501/v1",
    "apiKey": "denselite"
  }
}
```

---

## 4. Quick Verification via CLI

Test your live connection with a one-liner:

```bash
# 1. Health check
curl -s http://localhost:9501/health

# 2. Local AVX2 inference
curl -s -X POST http://localhost:9501/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model":"denselite","messages":[{"role":"user","content":"Say Hello!"}]}'
```
