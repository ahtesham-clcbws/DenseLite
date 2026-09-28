// DenseLite Control Plane Application Bootstrap

import { apiGet } from "./js/api.js";
import { initRouter } from "./js/router.js";
import { initDrawer } from "./js/drawer.js";
import { initShortcuts } from "./js/shortcuts.js";
import { initEngineControls, startTelemetryPolling } from "./js/telemetry.js";
import { initInferenceForm, saveInferenceSettings } from "./js/panes/inference.js";
import { fetchModels, openRegisterModal, closeRegisterModal, inspectGguf, submitRegisterModel } from "./js/panes/models.js";
import { initStorageForm, saveStorageSettings } from "./js/panes/storage.js";
import { initServerForm, saveServerSettings } from "./js/panes/server.js";
import { startLogsPolling, clearLogs } from "./js/panes/logs.js";

// Expose handlers to window for inline HTML onclick attributes
Object.assign(window, {
  saveInferenceSettings,
  saveStorageSettings,
  saveServerSettings,
  openRegisterModal,
  closeRegisterModal,
  inspectGguf,
  submitRegisterModel,
  clearLogs
});

document.addEventListener("DOMContentLoaded", async () => {
  initRouter((activeTab) => {
    if (activeTab === "models") fetchModels();
  });
  initDrawer();
  initShortcuts();
  initEngineControls();

  startTelemetryPolling(1500);
  startLogsPolling(2500);

  // Initial data loading
  fetchModels();
  loadAllSettings();
});

async function loadAllSettings() {
  try {
    const cfg = await apiGet("/api/settings");
    initInferenceForm(cfg.inference);
    initStorageForm(cfg.storage);
    initServerForm(cfg.server, cfg.resource);
  } catch (err) {
    console.error("Failed to load initial settings:", err);
  }
}
