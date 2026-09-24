# Security Policy

## Supported Versions

| Version | Supported |
|---|---|
| 3.0.x (current) | ✅ Active |
| 2.x | ❌ End of life |
| 1.x | ❌ End of life |

Only the latest release on the `main` branch receives security updates.

## Reporting a Vulnerability

**Do NOT open a public GitHub Issue for security vulnerabilities.**

Instead, please report vulnerabilities privately:

1. **Email:** Send a detailed report to the maintainer via GitHub's private
   vulnerability reporting feature on the
   [DenseLite repository](https://github.com/ahtesham-clcbws/DenseLite/security).
2. **Include:**
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact assessment
   - Suggested fix (if any)

### Response Timeline

| Stage | Timeframe |
|---|---|
| Acknowledgment | Within 48 hours |
| Initial assessment | Within 1 week |
| Patch release | Within 2 weeks (critical) / 4 weeks (high) |
| Public disclosure | After patch is released |

## Security Model & Threat Surface

### What DenseLite Handles

DenseLite is a **local inference gateway** that:

- Listens on `localhost:9501` (HTTP, no TLS)
- Stores API keys in-memory (loaded from `.env` at boot)
- Manages provider credentials via SQLite (`denselite_state.db`)
- Executes local model inference using mmap'd GGUF files
- Proxies requests to cloud LLM providers (Groq, OpenRouter, Gemini, etc.)

### Threat Model

| Threat | Mitigation | Status |
|---|---|---|
| API key exposure in version control | `.env` excluded via `.gitignore`; use `env.example` as template | ⚠️ Requires user discipline |
| Network-accessible API | Binds to `0.0.0.0:9501` — should be `127.0.0.1` in production | ⚠️ Configurable |
| SQL injection in SQLite queries | All queries use parameterized statements (`sqlite3_bind_*`) | ✅ Mitigated |
| Memory corruption via malformed GGUF | 32-byte alignment validation on all tensor offsets; magic byte check | ✅ Mitigated |
| Prompt injection via cloud relay | DenseLite does not execute tools — it only suggests actions to the IDE | ✅ By design |
| Denial of service via large payloads | No payload size limit enforced | ⚠️ Future improvement |
| KV cache memory exhaustion | `HardwareManager` enforces 45% RAM ceiling | ✅ Mitigated |

### API Key Handling Best Practices

1. **Never commit `.env`** — only commit `env.example` with placeholder values.
2. **Rotate keys regularly** — especially after any accidental exposure.
3. **Use separate key sets** for development and production.
4. **Monitor provider dashboards** for unexpected usage spikes.
5. **Prefer per-key cooldowns** — DenseLite's `SQLiteRouter` supports multiple
   keys per provider with automatic cooldown rotation.

### Local Model Security

- GGUF files are loaded via `mmap()` with `PROT_READ` + `MAP_SHARED` — the model
  weights are never modified in memory.
- Tensor data offsets are validated against 32-byte alignment boundaries before
  any AVX2 operations. Misaligned offsets cause a clean abort, not a segfault.
- The GGUF parser validates magic bytes (`0x46554747`) before processing any
  metadata.

### Network Security

DenseLite does **not** implement TLS. It is designed to run as a local service
behind a reverse proxy or within a trusted network. If you expose DenseLite to
an untrusted network:

1. Place it behind an **nginx/caddy reverse proxy** with TLS termination.
2. Restrict access via firewall rules to `127.0.0.1` only.
3. Do **not** expose port `9501` to the public internet.

## Hardening Checklist

- [ ] `.env` is in `.gitignore` and never committed
- [ ] All API keys are rotated after any exposure
- [ ] DenseLite binds to `127.0.0.1`, not `0.0.0.0`
- [ ] Firewall blocks external access to port `9501`
- [ ] GGUF model files are from trusted sources (HuggingFace official repos)
- [ ] `denselite_state.db` file permissions are restricted (`chmod 600`)

## Acknowledgments

We appreciate responsible disclosure. Contributors who report valid
vulnerabilities will be credited in the release notes (with permission).
