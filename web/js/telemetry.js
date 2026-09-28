// DenseLite Telemetry and Engine Supervisor Controls

import { apiGet, apiPost } from "./api.js";
import { showToast } from "./toast.js";

export function initEngineControls() {
  document.getElementById("btn-start")?.addEventListener("click", () => execEngineAction("/api/engine/start", "Starting Engine..."));
  document.getElementById("btn-stop")?.addEventListener("click", () => execEngineAction("/api/engine/stop", "Stopping Engine..."));
  document.getElementById("btn-restart")?.addEventListener("click", () => execEngineAction("/api/engine/restart", "Restarting Engine..."));
}

async function execEngineAction(endpoint, msg) {
  showToast(msg, "info");
  try {
    const data = await apiPost(endpoint);
    if (data.success) {
      showToast("Engine state updated", "success");
      setTimeout(fetchStatus, 400);
    } else {
      showToast(data.message || "Failed to update engine state", "error");
    }
  } catch (err) {
    showToast("Error communicating with control plane", "error");
  }
}

export function startTelemetryPolling(intervalMs = 1500) {
  fetchStatus();
  return setInterval(fetchStatus, intervalMs);
}

export async function fetchStatus() {
  try {
    const st = await apiGet("/api/status");
    const isRun = st.running;

    const dot = document.getElementById("status-dot");
    const label = document.getElementById("status-label");
    const mini = document.getElementById("metrics-mini");
    const mobDot = document.getElementById("mobile-status-dot");
    const mobLabel = document.getElementById("mobile-status-text");

    const statusText = isRun ? "RUNNING" : "STOPPED";
    const statusColor = isRun ? "#34d399" : "#ef4444";
    const dotClass = isRun ? "status-dot active" : "status-dot";

    if (dot) dot.className = dotClass;
    if (label) { label.textContent = statusText; label.style.color = statusColor; }
    if (mini) mini.textContent = isRun ? `PID: ${st.pid} | Port: ${st.port}` : `PID: -- | Port: ${st.port}`;
    if (mobDot) mobDot.className = dotClass;
    if (mobLabel) { mobLabel.textContent = statusText; mobLabel.style.color = statusColor; }

    const btnStart = document.getElementById("btn-start");
    const btnStop = document.getElementById("btn-stop");
    const btnRestart = document.getElementById("btn-restart");
    if (btnStart) btnStart.disabled = isRun;
    if (btnStop) btnStop.disabled = !isRun;
    if (btnRestart) btnRestart.disabled = !isRun;

    const elUptime = document.getElementById("tel-uptime");
    if (elUptime) elUptime.textContent = isRun ? formatUptime(st.uptime_seconds) : "0s";
  } catch (e) {}
}

export function formatUptime(sec) {
  if (!sec) return "0s";
  const m = Math.floor(sec / 60);
  const s = sec % 60;
  if (m === 0) return `${s}s`;
  const h = Math.floor(m / 60);
  return h > 0 ? `${h}h ${m % 60}m` : `${m}m ${s}s`;
}
