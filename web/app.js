// DenseLite Control Plane Frontend Controller

const API_BASE = window.location.origin;

document.addEventListener("DOMContentLoaded", () => {
  initTabs();
  initEngineButtons();
  fetchStatus();
  fetchSettings();
  fetchModels();
  fetchLogs();

  setInterval(fetchStatus, 1500);
  setInterval(fetchLogs, 2500);
});

// Tab Navigation
function initTabs() {
  document.querySelectorAll(".nav-item").forEach(btn => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".nav-item").forEach(b => b.classList.remove("active"));
      document.querySelectorAll(".tab-pane").forEach(p => p.classList.remove("active"));
      btn.classList.add("active");
      const tabId = btn.dataset.tab;
      document.getElementById(`pane-${tabId}`).classList.add("active");
    });
  });
}

// Engine Controls
function initEngineButtons() {
  document.getElementById("btn-start").addEventListener("click", () => execEngineAction("/api/engine/start", "Starting DenseLite Engine..."));
  document.getElementById("btn-stop").addEventListener("click", () => execEngineAction("/api/engine/stop", "Stopping DenseLite Engine..."));
  document.getElementById("btn-restart").addEventListener("click", () => execEngineAction("/api/engine/restart", "Restarting DenseLite Engine..."));
}

async function execEngineAction(endpoint, msg) {
  showToast(msg, "info");
  try {
    const res = await fetch(API_BASE + endpoint, { method: "POST" });
    const data = await res.json();
    if (data.success) {
      showToast("Engine state updated successfully", "success");
      setTimeout(fetchStatus, 500);
    } else {
      showToast(data.message || "Failed to update engine state", "error");
    }
  } catch (err) {
    showToast("Network error communicating with control plane", "error");
  }
}

// Status & Telemetry Polling
async function fetchStatus() {
  try {
    const res = await fetch(API_BASE + "/api/status");
    if (!res.ok) return;
    const st = await res.json();

    const isRun = st.running;
    const dot = document.getElementById("status-dot");
    const label = document.getElementById("status-label");
    const mini = document.getElementById("metrics-mini");

    if (isRun) {
      dot.className = "status-dot active";
      label.textContent = "RUNNING";
      label.style.color = "#34d399";
      mini.textContent = `PID: ${st.pid} | Port: ${st.port}`;
    } else {
      dot.className = "status-dot";
      label.textContent = "STOPPED";
      label.style.color = "#ef4444";
      mini.textContent = `PID: -- | Port: ${st.port}`;
    }

    document.getElementById("btn-start").disabled = isRun;
    document.getElementById("btn-stop").disabled = !isRun;
    document.getElementById("btn-restart").disabled = !isRun;

    document.getElementById("tel-uptime").textContent = isRun ? formatUptime(st.uptime_seconds) : "0s";
  } catch (e) {}
}

function formatUptime(sec) {
  if (!sec) return "0s";
  const m = Math.floor(sec / 60);
  const s = sec % 60;
  if (m === 0) return `${s}s`;
  const h = Math.floor(m / 60);
  return h > 0 ? `${h}h ${m % 60}m` : `${m}m ${s}s`;
}

// Settings Loading & Saving
async function fetchSettings() {
  try {
    const res = await fetch(API_BASE + "/api/settings");
    if (!res.ok) return;
    const cfg = await res.json();

    // Inference
    if (cfg.inference) {
      document.getElementById("inf-needle-mode").value = cfg.inference.needle3_mode || "hybrid";
      document.getElementById("inf-tool-dedup").checked = cfg.inference.enable_tool_dedup !== false;
      document.getElementById("inf-context-injection").checked = cfg.inference.enable_context_injection !== false;
      document.getElementById("inf-context-window").value = cfg.inference.context_window || 65536;
      document.getElementById("val-context-window").textContent = cfg.inference.context_window || 65536;
      document.getElementById("inf-temperature").value = cfg.inference.default_temperature || 0.7;
      document.getElementById("inf-top-p").value = cfg.inference.default_top_p || 0.9;
    }

    // Storage
    if (cfg.storage) {
      document.getElementById("st-models-dir").value = cfg.storage.models_dir || "";
      document.getElementById("st-data-dir").value = cfg.storage.data_dir || "";
      document.getElementById("st-kv-dir").value = cfg.storage.kv_cache_dir || "";
      document.getElementById("st-logs-dir").value = cfg.storage.logs_dir || "";
    }

    // Server & Resource
    if (cfg.server) {
      document.getElementById("srv-host").value = cfg.server.host || "0.0.0.0";
      document.getElementById("srv-port").value = cfg.server.port || 9501;
      document.getElementById("srv-threads").value = cfg.server.threads || 4;
      document.getElementById("srv-max-payload").value = cfg.server.max_payload_mb || 32;
    }
    if (cfg.resource) {
      const ram = Math.round((cfg.resource.ram_budget_percent || 0.45) * 100);
      document.getElementById("res-ram-budget").value = ram;
      document.getElementById("val-ram-budget").textContent = ram + "%";
      document.getElementById("res-enable-gpu").checked = cfg.resource.enable_gpu === true;
      document.getElementById("res-vram-budget").value = cfg.resource.vram_budget_mb || 2048;
    }
  } catch (e) {
    showToast("Failed to fetch settings", "error");
  }
}

async function saveInferenceSettings() {
  const payload = {
    inference: {
      needle3_mode: document.getElementById("inf-needle-mode").value,
      enable_tool_dedup: document.getElementById("inf-tool-dedup").checked,
      enable_context_injection: document.getElementById("inf-context-injection").checked,
      context_window: parseInt(document.getElementById("inf-context-window").value),
      default_temperature: parseFloat(document.getElementById("inf-temperature").value),
      default_top_p: parseFloat(document.getElementById("inf-top-p").value)
    }
  };
  saveSettingsPayload(payload, "Inference settings saved successfully");
}

async function saveStorageSettings() {
  const payload = {
    storage: {
      models_dir: document.getElementById("st-models-dir").value,
      data_dir: document.getElementById("st-data-dir").value,
      kv_cache_dir: document.getElementById("st-kv-dir").value,
      logs_dir: document.getElementById("st-logs-dir").value
    }
  };
  saveSettingsPayload(payload, "Storage paths updated successfully");
}

async function saveServerSettings() {
  const payload = {
    server: {
      host: document.getElementById("srv-host").value,
      port: parseInt(document.getElementById("srv-port").value),
      threads: parseInt(document.getElementById("srv-threads").value),
      max_payload_mb: parseInt(document.getElementById("srv-max-payload").value)
    },
    resource: {
      ram_budget_percent: parseInt(document.getElementById("res-ram-budget").value) / 100.0,
      enable_gpu: document.getElementById("res-enable-gpu").checked,
      vram_budget_mb: parseInt(document.getElementById("res-vram-budget").value)
    }
  };
  saveSettingsPayload(payload, "Server configuration saved successfully");
}

async function saveSettingsPayload(payload, successMsg) {
  try {
    const res = await fetch(API_BASE + "/api/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    const d = await res.json();
    if (d.success) showToast(successMsg, "success");
    else showToast(d.error || "Failed to save settings", "error");
  } catch (e) {
    showToast("Error saving settings", "error");
  }
}

// Models & Roles Management
let globalModels = [];

async function fetchModels() {
  try {
    const res = await fetch(API_BASE + "/api/models");
    if (!res.ok) return;
    const d = await res.json();
    globalModels = d.models || [];
    renderModelsTable(globalModels);
    renderRolesCards(d.roles || []);
  } catch (e) {}
}

function renderModelsTable(models) {
  const tbody = document.getElementById("models-table-body");
  if (!models || models.length === 0) {
    tbody.innerHTML = `<tr><td colspan="7" class="text-center" style="color:var(--text-dim);padding:24px;">No custom GGUF models registered. (Using default .env fallback)</td></tr>`;
    return;
  }
  tbody.innerHTML = models.map(m => `
    <tr>
      <td><strong>${escapeHtml(m.model_id)}</strong></td>
      <td><span class="badge" style="background:rgba(14,165,233,0.15);color:#38bdf8;">${escapeHtml(m.architecture)}</span></td>
      <td>${escapeHtml(m.param_size_str)}</td>
      <td>${escapeHtml(m.quant_type)}</td>
      <td>${m.context_length}</td>
      <td><span class="badge badge-pass">PASS (&le; 1.85B)</span></td>
      <td class="font-mono" style="font-size:0.75rem;color:var(--text-dim);">${escapeHtml(m.file_path)}</td>
    </tr>
  `).join("");
}

function renderRolesCards(roles) {
  const container = document.getElementById("roles-container");
  if (!roles || roles.length === 0) {
    container.innerHTML = `<div style="color:var(--text-dim);grid-column:1/-1;">No roles configured.</div>`;
    return;
  }

  container.innerHTML = roles.map(r => {
    const modelOptions = globalModels.map(m => `
      <option value="${escapeHtml(m.model_id)}" ${m.model_id === r.model_id ? "selected" : ""}>
        ${escapeHtml(m.model_id)} (${escapeHtml(m.param_size_str)})
      </option>
    `).join("");

    return `
      <div class="role-card">
        <div class="role-header">
          <span class="role-title">${escapeHtml(r.role)}</span>
          <label class="switch">
            <input type="checkbox" ${r.is_active ? "checked" : ""} onchange="toggleRole('${escapeHtml(r.role)}', this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="form-group" style="margin:0;">
          <select class="form-control role-bind-select font-mono" onchange="bindRole('${escapeHtml(r.role)}', this.value)">
            ${modelOptions.length > 0 ? modelOptions : `<option value="${escapeHtml(r.model_id)}">${escapeHtml(r.model_id)}</option>`}
          </select>
        </div>
      </div>
    `;
  }).join("");
}

async function toggleRole(role, activate) {
  try {
    const res = await fetch(API_BASE + "/api/roles/toggle", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ role, activate })
    });
    const d = await res.json();
    if (d.success) showToast(`Role '${role}' ${activate ? "activated" : "deactivated"}`, "success");
    else showToast("Failed to toggle role", "error");
  } catch (e) {
    showToast("Error updating role", "error");
  }
}

async function bindRole(role, modelId) {
  try {
    const res = await fetch(API_BASE + "/api/roles/bind", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ role, model_id: modelId })
    });
    const d = await res.json();
    if (d.success) showToast(`Bound role '${role}' to '${modelId}'`, "success");
    else showToast("Failed to bind role", "error");
  } catch (e) {
    showToast("Error binding role", "error");
  }
}

// Model Inspector & Registration Modal
function openRegisterModal() {
  document.getElementById("modal-register").classList.add("active");
}

function closeRegisterModal() {
  document.getElementById("modal-register").classList.remove("active");
  document.getElementById("inspect-result").classList.add("hidden");
}

async function inspectGguf() {
  const path = document.getElementById("reg-filepath").value.trim();
  if (!path) return showToast("Please enter a file path", "error");

  const box = document.getElementById("inspect-result");
  box.textContent = "Inspecting GGUF header...";
  box.classList.remove("hidden");

  try {
    const res = await fetch(API_BASE + "/api/models/inspect", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ path })
    });
    const d = await res.json();
    if (!d.valid) {
      box.textContent = `[FAIL] Invalid GGUF: ${d.error || "File error"}`;
      box.style.borderColor = "var(--accent-red)";
      return;
    }

    box.style.borderColor = d.pass_edge_budget ? "var(--accent-green)" : "var(--accent-amber)";
    box.textContent = `Architecture: ${d.architecture} | Parameters: ${d.param_size_str}\nQuantization: ${d.quant_type} | Context: ${d.context_length}\nEdge Budget Check: ${d.pass_edge_budget ? "PASS (<= 1.85B)" : "FAIL (> 1.85B)"}\nCompatible Roles: ${(d.compatible_roles || []).join(", ")}`;

    if (!document.getElementById("reg-model-id").value) {
      const parts = path.split("/").pop().split(".")[0];
      document.getElementById("reg-model-id").value = parts.toLowerCase().replace(/[^a-z0-9_]/g, "_");
    }
  } catch (e) {
    box.textContent = "Inspection request failed.";
  }
}

async function submitRegisterModel() {
  const path = document.getElementById("reg-filepath").value.trim();
  const id = document.getElementById("reg-model-id").value.trim();
  const checkedRoles = Array.from(document.querySelectorAll("input[name='reg-roles']:checked")).map(cb => cb.value);

  if (!path || !id) return showToast("File path and Model ID are required", "error");

  try {
    const res = await fetch(API_BASE + "/api/models/register", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ path, id, roles: checkedRoles })
    });
    const d = await res.json();
    if (d.success) {
      showToast(`Model '${id}' registered successfully!`, "success");
      closeRegisterModal();
      fetchModels();
    } else {
      showToast(d.error || "Failed to register model", "error");
    }
  } catch (e) {
    showToast("Error registering model", "error");
  }
}

// Real-time Console Log Viewer
let lastLogText = "";

async function fetchLogs() {
  try {
    const res = await fetch(API_BASE + "/api/logs/tail");
    if (!res.ok) return;
    const d = await res.json();
    const box = document.getElementById("console-output");
    if (!d.lines || d.lines.length === 0) return;

    const newText = d.lines.join("");
    if (newText !== lastLogText) {
      lastLogText = newText;
      box.innerHTML = d.lines.map(line => {
        let cls = "log-entry log-info";
        if (line.includes("[ERROR]") || line.includes("FAIL")) cls = "log-entry log-err";
        else if (line.includes("[WARN]")) cls = "log-entry log-warn";
        return `<div class="${cls}">${escapeHtml(line)}</div>`;
      }).join("");

      if (document.getElementById("log-autoscroll").checked) {
        box.scrollTop = box.scrollHeight;
      }
    }
  } catch (e) {}
}

function clearLogs() {
  document.getElementById("console-output").innerHTML = "";
  lastLogText = "";
}

// UI Utilities
function showToast(message, type = "info") {
  const container = document.getElementById("toast-container");
  const t = document.createElement("div");
  t.className = `toast toast-${type}`;
  t.textContent = message;
  container.appendChild(t);
  setTimeout(() => {
    t.style.opacity = "0";
    setTimeout(() => t.remove(), 250);
  }, 3500);
}

function escapeHtml(str) {
  if (!str) return "";
  return String(str).replace(/[&<>"']/g, m => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
  })[m]);
}
