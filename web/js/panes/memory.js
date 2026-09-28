// Code Intelligence & Semantic Memory RAG Controller

import { apiPost } from "../api.js";
import { showToast } from "../toast.js";

export function initMemoryForm(mem = {}) {
  if (mem.search_top_k !== undefined) document.getElementById("mem-top-k").value = mem.search_top_k;
  if (mem.similarity_threshold !== undefined) {
    document.getElementById("mem-sim-thresh").value = mem.similarity_threshold;
    const el = document.getElementById("val-sim-thresh");
    if (el) el.textContent = mem.similarity_threshold;
  }
  if (mem.max_snippet_lines !== undefined) document.getElementById("mem-snippet-lines").value = mem.max_snippet_lines;
  if (mem.excluded_paths !== undefined) document.getElementById("mem-excluded-paths").value = mem.excluded_paths;

  const simSlider = document.getElementById("mem-sim-thresh");
  simSlider?.addEventListener("input", (e) => {
    const el = document.getElementById("val-sim-thresh");
    if (el) el.textContent = e.target.value;
  });
}

export async function saveMemorySettings() {
  const payload = {
    memory: {
      search_top_k: parseInt(document.getElementById("mem-top-k")?.value || 5),
      similarity_threshold: parseFloat(document.getElementById("mem-sim-thresh")?.value || 0.70),
      max_snippet_lines: parseInt(document.getElementById("mem-snippet-lines")?.value || 60),
      excluded_paths: document.getElementById("mem-excluded-paths")?.value || "vendor,node_modules,storage,.git,build"
    }
  };

  try {
    const res = await apiPost("/api/settings", payload);
    if (res.success) showToast("Memory and RAG configuration saved", "success");
    else showToast(res.error || "Failed to save memory settings", "error");
  } catch (err) {
    showToast("Error saving memory settings", "error");
  }
}
