// Models Registry and Dynamic Role Bindings Controller

import { apiGet, apiPost } from "../api.js";
import { showToast } from "../toast.js";

const DEFAULT_ROLES = ["general", "coder", "router", "compressor", "embedding", "audio_stt", "image_gen"];

export async function fetchModels() {
  try {
    const data = await apiGet("/api/models");
    renderRoleCards(data.models || [], data.roles || []);
    renderModelsTable(data.models || []);
  } catch (e) {
    showToast("Failed to fetch models", "error");
  }
}

function renderRoleCards(models, bindings) {
  const container = document.getElementById("roles-container");
  if (!container) return;
  container.innerHTML = "";

  DEFAULT_ROLES.forEach(role => {
    const binding = bindings.find(b => b.role === role);
    const assignedModelId = binding ? binding.model_id : "";
    const isActive = binding ? binding.is_active : false;

    const card = document.createElement("div");
    card.className = "role-card";
    card.innerHTML = `
      <div class="role-header">
        <span class="role-name">${escapeHtml(role)}</span>
        <label class="switch">
          <input type="checkbox" ${isActive ? "checked" : ""} data-role="${escapeHtml(role)}" class="role-toggle">
          <span class="slider"></span>
        </label>
      </div>
      <div class="form-group" style="margin-bottom:0">
        <select class="form-control font-mono role-select" data-role="${escapeHtml(role)}">
          <option value="">-- No Model Assigned --</option>
          ${models.map(m => `<option value="${escapeHtml(m.model_id)}" ${m.model_id === assignedModelId ? "selected" : ""}>${escapeHtml(m.model_id)} (${escapeHtml(m.param_size_str || "Edge")})</option>`).join("")}
        </select>
      </div>
    `;
    container.appendChild(card);
  });

  container.querySelectorAll(".role-toggle").forEach(toggle => {
    toggle.addEventListener("change", async (e) => {
      const role = e.target.dataset.role;
      const activate = e.target.checked;
      try {
        const res = await apiPost("/api/roles/toggle", { role, activate });
        if (res.success) showToast(`Role '${role}' ${activate ? "activated" : "deactivated"}`, "success");
        else showToast("Failed to toggle role", "error");
      } catch (err) { showToast("Error toggling role", "error"); }
    });
  });

  container.querySelectorAll(".role-select").forEach(sel => {
    sel.addEventListener("change", async (e) => {
      const role = e.target.dataset.role;
      const model_id = e.target.value;
      if (!model_id) return;
      try {
        const res = await apiPost("/api/roles/bind", { role, model_id });
        if (res.success) {
          showToast(`Bound '${model_id}' to '${role}'`, "success");
          fetchModels();
        } else showToast("Failed to bind model to role", "error");
      } catch (err) { showToast("Error binding model", "error"); }
    });
  });
}

function renderModelsTable(models) {
  const tbody = document.getElementById("models-table-body");
  if (!tbody) return;
  if (models.length === 0) {
    tbody.innerHTML = `<tr><td colspan="7" class="text-center">No models registered yet. Click '+ Register Local GGUF' above.</td></tr>`;
    return;
  }
  tbody.innerHTML = models.map(m => `
    <tr>
      <td><strong>${escapeHtml(m.model_id)}</strong></td>
      <td><span class="badge badge-tag">${escapeHtml(m.architecture || "GGUF")}</span></td>
      <td>${escapeHtml(m.param_size_str || "--")}</td>
      <td>${escapeHtml(m.quant_type || "--")}</td>
      <td>${escapeHtml(String(m.context_length || "--"))}</td>
      <td><span class="badge ${m.is_verified ? "badge-active" : "badge-inactive"}">${m.is_verified ? "PASS" : "FAIL"}</span></td>
      <td title="${escapeHtml(m.file_path)}"><span class="path-truncate">${escapeHtml(m.file_path)}</span></td>
    </tr>
  `).join("");
}

export function openRegisterModal() {
  document.getElementById("modal-register")?.classList.add("active");
}

export function closeRegisterModal() {
  document.getElementById("modal-register")?.classList.remove("active");
  const box = document.getElementById("inspect-result");
  if (box) { box.classList.add("hidden"); box.textContent = ""; }
}

export async function inspectGguf() {
  const path = document.getElementById("reg-filepath").value.trim();
  if (!path) return showToast("Please enter a valid file path", "error");
  const box = document.getElementById("inspect-result");
  box.classList.remove("hidden");
  box.textContent = "Inspecting GGUF header tensors...";

  try {
    const d = await apiPost("/api/models/inspect", { path });
    if (!d.valid) {
      box.textContent = `Inspection Failed: ${d.error}`;
      box.style.borderColor = "#ef4444";
      return;
    }
    box.style.borderColor = "#0ea5e9";
    box.textContent = `Valid GGUF Model:\n• Architecture: ${d.architecture}\n• Parameters: ${d.param_size_str}\n• Quantization: ${d.quant_type}\n• Native Context: ${d.context_length}\n• Edge Budget: ${d.pass_edge_budget ? "PASSED (<14GB)" : "EXCEEDED"}`;

    const idInput = document.getElementById("reg-model-id");
    if (!idInput.value) {
      const filename = path.split("/").pop().replace(/\.gguf$/i, "").toLowerCase().replace(/[^a-z0-9_-]/g, "_");
      idInput.value = filename;
    }
  } catch (e) {
    box.textContent = "Error communicating with inspection engine";
  }
}

export async function submitRegisterModel() {
  const path = document.getElementById("reg-filepath").value.trim();
  const id = document.getElementById("reg-model-id").value.trim();
  if (!path || !id) return showToast("Path and Model ID are required", "error");

  const roles = Array.from(document.querySelectorAll("input[name='reg-roles']:checked")).map(cb => cb.value);
  try {
    const d = await apiPost("/api/models/register", { path, id, roles });
    if (d.success) {
      showToast(`Model '${id}' registered successfully!`, "success");
      closeRegisterModal();
      fetchModels();
    } else {
      showToast(d.error || "Failed to register model", "error");
    }
  } catch (e) {
    showToast("Error registering model", "error");
  }
}

function escapeHtml(str) {
  if (!str) return "";
  return String(str).replace(/[&<>"']/g, m => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
  })[m]);
}
