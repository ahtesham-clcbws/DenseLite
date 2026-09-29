// Inference and ModernBERT Routing Pane Controller

import { apiPost } from "../api.js";
import { showToast } from "../toast.js";

export function initInferenceForm(inf = {}) {
  if (inf.routing_mode) document.getElementById("inf-routing-mode").value = inf.routing_mode;
  if (inf.enable_tool_dedup !== undefined) document.getElementById("inf-tool-dedup").checked = inf.enable_tool_dedup;
  if (inf.enable_context_injection !== undefined) document.getElementById("inf-context-injection").checked = inf.enable_context_injection;
  if (inf.context_window) {
    document.getElementById("inf-context-window").value = inf.context_window;
    const ctxVal = document.getElementById("val-context-window");
    if (ctxVal) ctxVal.textContent = inf.context_window;
  }
  if (inf.default_temperature) document.getElementById("inf-temperature").value = inf.default_temperature;
  if (inf.default_top_p) document.getElementById("inf-top-p").value = inf.default_top_p;
  if (inf.repeat_penalty !== undefined) document.getElementById("inf-repeat-penalty").value = inf.repeat_penalty;
  if (inf.repeat_last_n !== undefined) document.getElementById("inf-repeat-last-n").value = inf.repeat_last_n;
  if (inf.top_k !== undefined) document.getElementById("inf-top-k").value = inf.top_k;
  if (inf.min_p !== undefined) document.getElementById("inf-min-p").value = inf.min_p;
  if (inf.max_output_tokens !== undefined) document.getElementById("inf-max-tokens").value = inf.max_output_tokens;
  if (inf.system_prompt !== undefined) document.getElementById("inf-system-prompt").value = inf.system_prompt;

  const range = document.getElementById("inf-context-window");
  range?.addEventListener("input", (e) => {
    const el = document.getElementById("val-context-window");
    if (el) el.textContent = e.target.value;
  });
}

export async function saveInferenceSettings() {
  const payload = {
    inference: {
      routing_mode: document.getElementById("inf-routing-mode")?.value || "modernbert",
      enable_tool_dedup: document.getElementById("inf-tool-dedup")?.checked ?? true,
      enable_context_injection: document.getElementById("inf-context-injection")?.checked ?? true,
      context_window: parseInt(document.getElementById("inf-context-window")?.value || 65536),
      default_temperature: parseFloat(document.getElementById("inf-temperature")?.value || 0.7),
      default_top_p: parseFloat(document.getElementById("inf-top-p")?.value || 0.9),
      repeat_penalty: parseFloat(document.getElementById("inf-repeat-penalty")?.value || 1.15),
      repeat_last_n: parseInt(document.getElementById("inf-repeat-last-n")?.value || 64),
      top_k: parseInt(document.getElementById("inf-top-k")?.value || 40),
      min_p: parseFloat(document.getElementById("inf-min-p")?.value || 0.05),
      max_output_tokens: parseInt(document.getElementById("inf-max-tokens")?.value || 512),
      system_prompt: document.getElementById("inf-system-prompt")?.value || ""
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
