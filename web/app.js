// DenseLite Control Plane Application Bootstrap

import { apiGet } from "./js/api.js";
import { initRouter } from "./js/router.js";
import { initDrawer } from "./js/drawer.js";
import { initShortcuts } from "./js/shortcuts.js";
import { initEngineControls, startTelemetryPolling } from "./js/telemetry.js";
import { initDashboard, refreshDashboard, pingEngine } from "./js/panes/dashboard.js";
import { initInferenceForm, saveInferenceSettings } from "./js/panes/inference.js";
import { fetchModels, openRegisterModal, closeRegisterModal, inspectGguf, submitRegisterModel } from "./js/panes/models.js";
import { initMultimodalForm, saveMultimodalSettings } from "./js/panes/multimodal.js";
import { initProvidersForm, saveProvidersSettings } from "./js/panes/providers.js";
import { initMemoryForm, saveMemorySettings } from "./js/panes/memory.js";
import { initStorageForm, saveStorageSettings } from "./js/panes/storage.js";
import { initServerForm, saveServerSettings } from "./js/panes/server.js";
import {
  initSystemPane, executeFactoryReset, openResetModal, closeResetModal,
  confirmReset, purgeKvCache, vacuumDatabases, exportSettings, importSettings, fetchDatabaseStats
} from "./js/panes/system.js";
import { startLogsPolling, clearLogs } from "./js/panes/logs.js";

// Expose handlers to window for inline HTML onclick attributes
Object.assign(window, {
  pingEngine,
  refreshDashboard,
  saveInferenceSettings,
  saveMultimodalSettings,
  saveProvidersSettings,
  saveMemorySettings,
  saveStorageSettings,
  saveServerSettings,
  openResetModal,
  closeResetModal,
  confirmReset,
  executeFactoryReset,
  purgeKvCache,
  vacuumDatabases,
  exportSettings,
  importSettings,
  fetchDatabaseStats,
  openRegisterModal,
  closeRegisterModal,
  inspectGguf,
  submitRegisterModel,
  clearLogs,
  loadAllSettings
});

function boot() {
  initRouter((activeTab) => {
    if (activeTab === "dashboard") refreshDashboard();
    else if (activeTab === "models") fetchModels();
    else if (activeTab === "system") fetchDatabaseStats();
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
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", boot);
} else {
  boot();
}

export async function loadAllSettings() {
  try {
    const cfg = await apiGet("/api/settings");
    if (cfg.inference) initInferenceForm(cfg.inference);
    if (cfg.multimodal) initMultimodalForm(cfg.multimodal);
    if (cfg.cloud) initProvidersForm(cfg.cloud);
    if (cfg.memory) initMemoryForm(cfg.memory);
    if (cfg.storage) initStorageForm(cfg.storage);
    if (cfg.server || cfg.resource) initServerForm(cfg.server, cfg.resource);
  } catch (err) {
    console.error("Failed to load initial settings:", err);
  }
}
