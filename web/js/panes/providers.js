// Cloud Fallback & LLM Provider API Key Controller

import { apiPost } from "../api.js";
import { showToast } from "../toast.js";

export function initProvidersForm(cc = {}) {
  if (cc.openrouter_api_key !== undefined) document.getElementById("prov-openrouter-key").value = cc.openrouter_api_key;
  if (cc.gemini_api_key !== undefined) document.getElementById("prov-gemini-key").value = cc.gemini_api_key;
  if (cc.openai_api_key !== undefined) document.getElementById("prov-openai-key").value = cc.openai_api_key;
  if (cc.cloud_priority !== undefined) document.getElementById("prov-cloud-priority").value = cc.cloud_priority;
  if (cc.cloud_fallback_enabled !== undefined) document.getElementById("prov-fallback-enabled").checked = cc.cloud_fallback_enabled;
}

export async function saveProvidersSettings() {
  const payload = {
    cloud: {
      openrouter_api_key: document.getElementById("prov-openrouter-key")?.value || "",
      gemini_api_key: document.getElementById("prov-gemini-key")?.value || "",
      openai_api_key: document.getElementById("prov-openai-key")?.value || "",
      cloud_priority: document.getElementById("prov-cloud-priority")?.value || "openrouter,gemini,openai",
      cloud_fallback_enabled: document.getElementById("prov-fallback-enabled")?.checked ?? true
    }
  };

  try {
    const res = await apiPost("/api/settings", payload);
    if (res.success) showToast("Cloud provider settings saved. Empty keys fall back to .env.", "success");
    else showToast(res.error || "Failed to save cloud provider settings", "error");
  } catch (err) {
    showToast("Error saving cloud settings", "error");
  }
}
