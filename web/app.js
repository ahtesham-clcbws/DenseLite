// DenseLite Control Plane Application Bootstrap

import { apiGet } from "./js/api.js";
import { initRouter } from "./js/router.js";
import { initDrawer } from "./js/drawer.js";
import { initShortcuts } from "./js/shortcuts.js";
import { initEngineControls, startTelemetryPolling } from "./js/telemetry.js";
import { initDashboard, refreshDashboard, pingEngine } from "./js/panes/dashboard.js";
import { initInferenceForm, saveInferenceSettings } from "./js/panes/inference.js";
import { fetchModels, openRegisterModal, closeRegisterModal, inspectGguf, submitRegisterModel } from "./js/panes/models.js";
import { initStorageForm, saveStorageSettings } from "./js/panes/storage.js";
import { initServerForm, saveServerSettings } from "./js/panes/server.js";
import { initSystemPane, executeFactoryReset, purgeKvCache, vacuumDatabases } from "./js/panes/system.js";
import { startLogsPolling, clearLogs } from "./js/panes/logs.js";

// Expose handlers to window for inline HTML onclick attributes
Object.assign(window, {
  pingEngine,
  refreshDashboard,
  saveInferenceSettings,
  saveStorageSettings,
  saveServerSettings,
  executeFactoryReset,
  purgeKvCache,
  vacuumDatabases,
  openRegisterModal,
  closeRegisterModal,
  inspectGguf,
  submitRegisterModel,
  clearLogs,
  loadAllSettings
});

document.addEventListener("DOMContentLoaded", async () => {
  initRouter((activeTab) => {
    if (activeTab === "dashboard") refreshDashboard();
    else if (activeTab === "models") fetchModels();
  });
  initDrawer();
  initShortcuts();
  initEngineControls();
  initDashboard();
  initSystemPane();

  startTelemetryPolling(1500);
  startLogsPolling(2500);

  // Initial data loading
  fetchModels();
  loadAllSettings();
});

export async function loadAllSettings() {
  try {
    const cfg = await apiGet("/api/settings");
    initInferenceForm(cfg.inference);
    initStorageForm(cfg.storage);
    initServerForm(cfg.server, cfg.resource);
  } catch (err) {
    console.error("Failed to load initial settings:", err);
  }
}
