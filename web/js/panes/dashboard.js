// Dashboard Overview Pane Controller
// Provides live system telemetry, resident model status, and quick control actions

import { apiGet, apiPost } from "../api.js";
import { showToast } from "../toast.js";

let dashboardTimer = null;

export function initDashboard() {
  refreshDashboard();
  if (!dashboardTimer) {
    dashboardTimer = setInterval(refreshDashboard, 3000);
  }
}

export async function refreshDashboard() {
  try {
    const [statusData, modelsData, settingsData] = await Promise.all([
      apiGet("/api/status").catch(() => null),
      apiGet("/api/models").catch(() => null),
      apiGet("/api/settings").catch(() => null)
    ]);

    if (statusData) renderStatus(statusData);
    if (modelsData) renderModelsOverview(modelsData);
    if (settingsData) renderSettingsOverview(settingsData);
  } catch (err) {
    console.error("Dashboard refresh error:", err);
  }
}

function renderStatus(s) {
  const badge = document.getElementById("dash-status-badge");
  const pidEl = document.getElementById("dash-pid");
  const portEl = document.getElementById("dash-port");
  const uptimeEl = document.getElementById("dash-uptime");

  const isRunning = Boolean(s && s.running);
  if (badge) {
    badge.className = `status-pill ${isRunning ? "pill-running" : "pill-stopped"}`;
    badge.textContent = isRunning ? "ACTIVE / RUNNING" : "STOPPED";
  }
  if (pidEl) pidEl.textContent = isRunning && s.pid > 0 ? s.pid : "--";
  if (portEl) portEl.textContent = (s && s.port) || 9501;
  if (uptimeEl) uptimeEl.textContent = formatUptime((s && s.uptime_seconds) || 0);
}

function renderModelsOverview(data) {
  const container = document.getElementById("dash-models-matrix");
  if (!container) return;

  const roles = (data && data.roles) || [];
  const models = (data && data.models) || [];
  if (roles.length === 0) {
    container.innerHTML = `<div style="color:var(--text-dim);grid-column:1/-1;padding:12px">No model roles configured.</div>`;
    return;
  }

  container.innerHTML = roles.map(r => {
    const m = models.find(mod => mod.model_id === r.model_id);
    const arch = m ? m.architecture : "unassigned";
    const size = m ? m.param_size_str : "--";
    const quant = m ? m.quant_type : "";

    return `
      <div class="dash-model-card">
        <div class="dash-model-role">
          <span class="role-badge">${escapeHtml(r.role)}</span>
          <span class="active-dot ${r.is_active ? 'dot-on' : 'dot-off'}"></span>
        </div>
        <div class="dash-model-id font-mono">${escapeHtml(r.model_id || "None")}</div>
        <div class="dash-model-meta font-mono">${escapeHtml(arch)} · ${escapeHtml(size)} ${escapeHtml(quant)}</div>
      </div>
    `;
  }).join("");
}

function renderSettingsOverview(cfg) {
  if (!cfg) return;
  const needleMode = document.getElementById("dash-needle-mode");
  const ramBudget = document.getElementById("dash-ram-budget");
  const ctxWindow = document.getElementById("dash-ctx-window");
  const modelsDir = document.getElementById("dash-models-dir");

  if (needleMode && cfg.inference) needleMode.textContent = cfg.inference.needle3_mode || "hybrid";
  if (ramBudget && cfg.resource) ramBudget.textContent = (cfg.resource.ram_budget_percent || 45) + "%";
  if (ctxWindow && cfg.inference) ctxWindow.textContent = (cfg.inference.context_window || 65536).toLocaleString() + " tokens";
  if (modelsDir && cfg.storage) modelsDir.textContent = cfg.storage.models_dir || "~/.denselite/models";
}

export async function pingEngine() {
  try {
    const t0 = performance.now();
    const res = await apiGet("/api/status");
    const latency = Math.round(performance.now() - t0);
    const isRun = res && res.running;
    showToast(`DenseLite Engine: ${isRun ? "RUNNING (PID " + res.pid + ")" : "STOPPED"} · ${latency}ms latency`, isRun ? "success" : "info");
  } catch (e) {
    showToast("Ping failed: Control plane unresponsive", "error");
  }
}

function formatUptime(seconds) {
  if (seconds < 60) return `${seconds}s`;
  const m = Math.floor(seconds / 60);
  const s = seconds % 60;
  if (m < 60) return `${m}m ${s}s`;
  const h = Math.floor(m / 60);
  return `${h}h ${m % 60}m`;
}

function escapeHtml(str) {
  if (!str) return "";
  return String(str).replace(/[&<>"']/g, m => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
  })[m]);
}
