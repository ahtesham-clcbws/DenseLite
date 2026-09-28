// Inference and Needle Routing Pane Controller

import { apiPost } from "../api.js";
import { showToast } from "../toast.js";

export function initInferenceForm(inf = {}) {
  if (inf.needle3_mode) document.getElementById("inf-needle-mode").value = inf.needle3_mode;
  if (inf.enable_tool_dedup !== undefined) document.getElementById("inf-tool-dedup").checked = inf.enable_tool_dedup;
  if (inf.enable_context_injection !== undefined) document.getElementById("inf-context-injection").checked = inf.enable_context_injection;
  if (inf.context_window) {
    document.getElementById("inf-context-window").value = inf.context_window;
    document.getElementById("val-context-window").textContent = inf.context_window;
  }
  if (inf.default_temperature) document.getElementById("inf-temperature").value = inf.default_temperature;
  if (inf.default_top_p) document.getElementById("inf-top-p").value = inf.default_top_p;

  const range = document.getElementById("inf-context-window");
  range?.addEventListener("input", (e) => {
    document.getElementById("val-context-window").textContent = e.target.value;
  });
}

export async function saveInferenceSettings() {
  const payload = {
    inference: {
      needle3_mode: document.getElementById("inf-needle-mode").value,
      enable_tool_dedup: document.getElementById("inf-tool-dedup").checked,
      enable_context_injection: document.getElementById("inf-context-injection").checked,
      context_window: parseInt(document.getElementById("inf-context-window").value),
      default_temperature: parseFloat(document.getElementById("inf-temperature").value),
      default_top_p: parseFloat(document.getElementById("inf-top-p").value)
    }
  };

  try {
    const res = await apiPost("/api/settings", payload);
    if (res.success) showToast("Inference settings saved successfully", "success");
    else showToast(res.error || "Failed to save settings", "error");
  } catch (err) {
    showToast("Error saving inference settings", "error");
  }
}
