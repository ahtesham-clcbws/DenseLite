// Server Gateway and Resource Governor Controller

import { apiPost } from "../api.js";
import { showToast } from "../toast.js";

export function initServerForm(srv = {}, res = {}) {
  if (srv.host) document.getElementById("srv-host").value = srv.host;
  if (srv.port) document.getElementById("srv-port").value = srv.port;
  if (srv.threads) document.getElementById("srv-threads").value = srv.threads;
  if (srv.max_payload_mb) document.getElementById("srv-max-payload").value = srv.max_payload_mb;

  if (res.ram_budget_percent) {
    document.getElementById("res-ram-budget").value = res.ram_budget_percent;
    document.getElementById("val-ram-budget").textContent = res.ram_budget_percent + "%";
  }
  if (res.enable_gpu !== undefined) document.getElementById("res-enable-gpu").checked = res.enable_gpu;
  if (res.vram_budget_mb) document.getElementById("res-vram-budget").value = res.vram_budget_mb;

  const ramRange = document.getElementById("res-ram-budget");
  ramRange?.addEventListener("input", (e) => {
    document.getElementById("val-ram-budget").textContent = e.target.value + "%";
  });
}

export async function saveServerSettings() {
  const payload = {
    server: {
      host: document.getElementById("srv-host").value,
      port: parseInt(document.getElementById("srv-port").value),
      threads: parseInt(document.getElementById("srv-threads").value),
      max_payload_mb: parseInt(document.getElementById("srv-max-payload").value)
    },
    resource: {
      ram_budget_percent: parseInt(document.getElementById("res-ram-budget").value),
      enable_gpu: document.getElementById("res-enable-gpu").checked,
      vram_budget_mb: parseInt(document.getElementById("res-vram-budget").value)
    }
  };

  try {
    const res = await apiPost("/api/settings", payload);
    if (res.success) showToast("Server and resource configuration saved", "success");
    else showToast(res.error || "Failed to save server settings", "error");
  } catch (err) {
    showToast("Error saving server settings", "error");
  }
}
