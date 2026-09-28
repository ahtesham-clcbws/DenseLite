// System Maintenance & Factory Reset Pane Controller

import { apiPost, apiGet } from "../api.js";
import { showToast } from "../toast.js";

export function openResetModal() {
  document.getElementById("modal-reset-confirm")?.classList.add("active");
}

export function closeResetModal() {
  document.getElementById("modal-reset-confirm")?.classList.remove("active");
}

export async function confirmReset() {
  closeResetModal();
  await executeFactoryReset();
}

export function initSystemPane() {
  const openBtn = document.getElementById("btn-open-reset-modal");
  const cancelBtns = document.querySelectorAll(".btn-cancel-reset");
  const confirmBtn = document.getElementById("btn-confirm-reset");

  if (openBtn) openBtn.addEventListener("click", openResetModal);
  cancelBtns.forEach(btn => btn.addEventListener("click", closeResetModal));
  if (confirmBtn) confirmBtn.addEventListener("click", confirmReset);

  fetchDatabaseStats();
}

export async function fetchDatabaseStats() {
  try {
    const s = await apiGet("/api/system/stats");
    const elSet = document.getElementById("stats-settings-rows");
    const elMod = document.getElementById("stats-models-count");
    const elRol = document.getElementById("stats-roles-count");
    const elMem = document.getElementById("stats-memory-records");
    const elSym = document.getElementById("stats-indexed-symbols");
    if (elSet) elSet.textContent = s.settings_rows ?? "--";
    if (elMod) elMod.textContent = s.registered_models ?? "--";
    if (elRol) elRol.textContent = s.role_bindings ?? "--";
    if (elMem) elMem.textContent = s.memory_records ?? "--";
    if (elSym) elSym.textContent = s.indexed_symbols ?? "--";
  } catch (e) {}
}

export async function executeFactoryReset() {
  try {
    showToast("Resetting settings to migration defaults...", "info");
    const res = await apiPost("/api/settings/reset", {});
    if (res.success) {
      showToast("DenseLite successfully reset to factory defaults!", "success");
      if (window.loadAllSettings) window.loadAllSettings();
      fetchDatabaseStats();
    } else {
      showToast(res.message || "Failed to reset settings", "error");
    }
  } catch (err) {
    showToast("Error executing reset: " + err.message, "error");
  }
}

export async function purgeKvCache() {
  if (!confirm("Are you sure you want to purge all cached session KV states? Active sessions will re-evaluate prompt context.")) return;
  try {
    const res = await apiPost("/api/system/purge-cache", {});
    if (res.success) showToast("Session KV Cache purged successfully", "success");
    else showToast("Failed to purge KV cache", "error");
  } catch (err) {
    showToast("Error purging cache: " + err.message, "error");
  }
}

export async function vacuumDatabases() {
  try {
    showToast("Optimizing and vacuuming SQLite WAL databases...", "info");
    const res = await apiPost("/api/system/vacuum", {});
    if (res.success) {
      showToast("Databases optimized and defragmented (VACUUM OK)", "success");
      fetchDatabaseStats();
    } else {
      showToast("Vacuum operation failed", "error");
    }
  } catch (err) {
    showToast("Error vacuuming databases: " + err.message, "error");
  }
}

export function exportSettings() {
  window.location.href = "/api/settings/export";
  showToast("Downloading settings backup...", "info");
}

export async function importSettings(event) {
  const file = event.target.files?.[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = async (e) => {
    try {
      const parsed = JSON.parse(e.target.result);
      showToast("Restoring settings from backup...", "info");
      const res = await apiPost("/api/settings/import", parsed);
      if (res.success) {
        showToast("Settings restored successfully!", "success");
        if (window.loadAllSettings) window.loadAllSettings();
        fetchDatabaseStats();
      } else {
        showToast(res.error || "Import failed", "error");
      }
    } catch (err) {
      showToast("Invalid JSON backup file: " + err.message, "error");
    }
  };
  reader.readAsText(file);
}
