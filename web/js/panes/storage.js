// Storage and User Paths Pane Controller

import { apiPost } from "../api.js";
import { showToast } from "../toast.js";

export function initStorageForm(st = {}) {
  if (st.models_dir) document.getElementById("st-models-dir").value = st.models_dir;
  if (st.data_dir) document.getElementById("st-data-dir").value = st.data_dir;
  if (st.kv_cache_dir) document.getElementById("st-kv-dir").value = st.kv_cache_dir;
  if (st.logs_dir) document.getElementById("st-logs-dir").value = st.logs_dir;
}

export async function saveStorageSettings() {
  const payload = {
    storage: {
      models_dir: document.getElementById("st-models-dir").value,
      data_dir: document.getElementById("st-data-dir").value,
      kv_cache_dir: document.getElementById("st-kv-dir").value,
      logs_dir: document.getElementById("st-logs-dir").value
    }
  };

  try {
    const res = await apiPost("/api/settings", payload);
    if (res.success) showToast("Storage paths updated successfully", "success");
    else showToast(res.error || "Failed to update storage paths", "error");
  } catch (err) {
    showToast("Error saving storage settings", "error");
  }
}
