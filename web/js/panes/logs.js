// Live Console Output Controller

import { apiGet } from "../api.js";

let lastLogText = "";

export function startLogsPolling(intervalMs = 2500) {
  fetchLogs();
  return setInterval(fetchLogs, intervalMs);
}

export async function fetchLogs() {
  try {
    const d = await apiGet("/api/logs/tail");
    const box = document.getElementById("console-output");
    if (!box || !d.lines || d.lines.length === 0) return;

    const newText = d.lines.join("");
    if (newText !== lastLogText) {
      lastLogText = newText;
      box.innerHTML = d.lines.map(line => {
        let cls = "log-entry log-info";
        if (line.includes("[ERROR]") || line.includes("FAIL")) cls = "log-entry log-err";
        else if (line.includes("[WARN]")) cls = "log-entry log-warn";
        return `<div class="${cls}">${escapeHtml(line)}</div>`;
      }).join("");

      const autoScroll = document.getElementById("log-autoscroll");
      if (autoScroll && autoScroll.checked) {
        box.scrollTop = box.scrollHeight;
      }
    }
  } catch (e) {}
}

export function clearLogs() {
  const box = document.getElementById("console-output");
  if (box) box.innerHTML = "";
  lastLogText = "";
}

function escapeHtml(str) {
  if (!str) return "";
  return String(str).replace(/[&<>"']/g, m => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
  })[m]);
}
