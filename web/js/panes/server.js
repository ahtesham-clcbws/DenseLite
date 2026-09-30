// Server Gateway and Resource Governor Controller

import { apiPost } from "../api.js";
import { showToast } from "../toast.js";

export function initServerForm(srv = {}, res = {}) {
  if (srv.host) document.getElementById("srv-host").value = srv.host;
  if (srv.port) document.getElementById("srv-port").value = srv.port;
  if (srv.threads) document.getElementById("srv-threads").value = srv.threads;
  if (srv.max_payload_mb) document.getElementById("srv-max-payload").value = srv.max_payload_mb;
  if (srv.enable_api_auth !== undefined) document.getElementById("srv-enable-auth").checked = srv.enable_api_auth;
  if (srv.api_secret_key !== undefined) document.getElementById("srv-api-secret").value = srv.api_secret_key;
  if (srv.cors_allowed_origins !== undefined) document.getElementById("srv-cors-origins").value = srv.cors_allowed_origins;
  if (srv.n_batch !== undefined) document.getElementById("srv-n-batch").value = srv.n_batch;

  if (res.ram_budget_percent) {
    document.getElementById("res-ram-budget").value = res.ram_budget_percent;
    const valRam = document.getElementById("val-ram-budget");
    if (valRam) valRam.textContent = res.ram_budget_percent + "%";
  }
  if (res.enable_gpu !== undefined) document.getElementById("res-enable-gpu").checked = res.enable_gpu;
  if (res.vram_budget_mb) document.getElementById("res-vram-budget").value = res.vram_budget_mb;

  const ramRange = document.getElementById("res-ram-budget");
  ramRange?.addEventListener("input", (e) => {
    const valRam = document.getElementById("val-ram-budget");
    if (valRam) valRam.textContent = e.target.value + "%";
  });
}

export async function saveServerSettings() {
  const payload = {
    server: {
      host: document.getElementById("srv-host")?.value || "0.0.0.0",
      port: parseInt(document.getElementById("srv-port")?.value || 9501),
      threads: parseInt(document.getElementById("srv-threads")?.value || 4),
      max_payload_mb: parseInt(document.getElementById("srv-max-payload")?.value || 32),
      enable_api_auth: document.getElementById("srv-enable-auth")?.checked ?? false,
      api_secret_key: document.getElementById("srv-api-secret")?.value || "",
      cors_allowed_origins: document.getElementById("srv-cors-origins")?.value || "",
      n_batch: parseInt(document.getElementById("srv-n-batch")?.value || 512)
    },
    resource: {
      ram_budget_percent: parseInt(document.getElementById("res-ram-budget")?.value || 45),
      enable_gpu: document.getElementById("res-enable-gpu")?.checked ?? true,
      vram_budget_mb: parseInt(document.getElementById("res-vram-budget")?.value || 2048)
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
