// Global Keyboard Shortcuts Controller

import { getActiveTab } from "./router.js";
import { saveInferenceSettings } from "./panes/inference.js";
import { saveMultimodalSettings } from "./panes/multimodal.js";
import { saveProvidersSettings } from "./panes/providers.js";
import { saveMemorySettings } from "./panes/memory.js";
import { saveStorageSettings } from "./panes/storage.js";
import { saveServerSettings } from "./panes/server.js";
import { closeRegisterModal } from "./panes/models.js";
import { closeDrawer } from "./drawer.js";
import { showToast } from "./toast.js";

export function initShortcuts() {
  window.addEventListener("keydown", (e) => {
    const isInput = ["INPUT", "TEXTAREA", "SELECT"].includes(document.activeElement?.tagName);

    // Ctrl+S / Cmd+S: Save active pane settings
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "s") {
      e.preventDefault();
      const current = getActiveTab();
      if (current === "inference") saveInferenceSettings();
      else if (current === "multimodal") saveMultimodalSettings();
      else if (current === "providers") saveProvidersSettings();
      else if (current === "memory") saveMemorySettings();
      else if (current === "storage") saveStorageSettings();
      else if (current === "server") saveServerSettings();
      else showToast(`Tab '${current}' has no configuration form`, "info");
      return;
    }

    // Escape: Close modals and mobile drawer
    if (e.key === "Escape") {
      closeRegisterModal();
      closeDrawer();
      return;
    }

    // 0-9 Number Keys: Quick tab switching when not in text input
    if (!isInput && !e.ctrlKey && !e.altKey && !e.metaKey) {
      const keyMap = {
        "1": "dashboard", "2": "inference", "3": "models",
        "4": "multimodal", "5": "providers", "6": "memory",
        "7": "storage", "8": "server", "9": "system", "0": "logs"
      };
      if (keyMap[e.key]) {
        e.preventDefault();
        window.location.hash = `#/${keyMap[e.key]}`;
      }
    }
  });
}
