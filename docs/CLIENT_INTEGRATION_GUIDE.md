# DenseLite Client Integration Guide

DenseLite exposes a standard **OpenAI-compatible HTTP Gateway** on `http://localhost:9501/v1`. Any client, IDE, or tool supporting custom OpenAI endpoints can seamlessly connect to DenseLite.

---

## Gateway Endpoints & Available Models

* **Base URL:** `http://localhost:9501/v1`
* **Chat Endpoint:** `http://localhost:9501/v1/chat/completions`
* **API Key:** Any string (e.g. `denselite` or leave blank if optional)

### Supported Model Identifiers
| Model ID | Execution Engine | Best Use Case |
|---|---|---|
| `denselite` | **Auto-Router (Recommended)** | Automatically selects coder, reasoner, or cloud |
| `qwen_coder` | Local C++ AVX2 (1.5B Q8_0) | Fast code generation and refactoring |
| `qwen_main` | Local C++ AVX2 (1.5B Q8_0) | Conversational text, summarization, general chat |
| `openai/gpt-oss-20b`| Groq Cloud (52 ms latency) | Ultra-fast cloud text & reasoning |
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
   * **Model Name:** `denselite` *(or `qwen_coder`)*
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
            "name": "qwen_coder",
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

## 2. OpenCode Setup

### Method A: Via OpenCode UI
1. Open OpenCode Settings and select **Custom Providers** (`Add Provider`).
2. Configure provider fields:
   * **Provider ID:** `denselite`
   * **Display name:** `DenseLite Local Brain`
   * **Base URL:** `http://localhost:9501/v1`
   * **API key:** `denselite` *(or leave blank)*
3. Under **Models**, add your target models:
   * `model-id`: `denselite` | **Display Name:** `DenseLite Auto Router`
   * `model-id`: `qwen_coder` | **Display Name:** `Qwen 2.5 Coder 1.5B (Local)`
   * `model-id`: `openai/gpt-oss-20b` | **Display Name:** `Groq GPT OSS 20B (Cloud)`

### Method B: Via `config.json`
In your OpenCode settings configuration (`~/.config/opencode/config.json`):

```json
{
  "providers": {
    "denselite": {
      "id": "denselite",
      "name": "DenseLite Local Brain",
      "baseUrl": "http://localhost:9501/v1",
      "apiKey": "denselite",
      "models": [
        {
          "id": "denselite",
          "name": "DenseLite Auto Router"
        },
        {
          "id": "qwen_coder",
          "name": "Qwen 2.5 Coder 1.5B (Local)"
        },
        {
          "id": "openai/gpt-oss-20b",
          "name": "Groq GPT OSS 20B (Cloud)"
        }
      ]
    }
  }
}
```

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
      "model": "qwen_coder",
      "apiBase": "http://localhost:9501/v1",
      "apiKey": "denselite"
    }
  ],
  "tabAutocompleteModel": {
    "title": "DenseLite Autocomplete",
    "provider": "openai",
    "model": "qwen_coder",
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
