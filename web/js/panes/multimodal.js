// Multimodal Studio Controller (Stable Diffusion & Whisper STT)

import { apiPost } from "../api.js";
import { showToast } from "../toast.js";

export function initMultimodalForm(mm = {}) {
  if (mm.sd_steps !== undefined) document.getElementById("mm-sd-steps").value = mm.sd_steps;
  if (mm.sd_cfg_scale !== undefined) {
    document.getElementById("mm-sd-cfg-scale").value = mm.sd_cfg_scale;
    const el = document.getElementById("val-sd-cfg-scale");
    if (el) el.textContent = mm.sd_cfg_scale;
  }
  if (mm.sd_width !== undefined) document.getElementById("mm-sd-width").value = mm.sd_width;
  if (mm.sd_height !== undefined) document.getElementById("mm-sd-height").value = mm.sd_height;
  if (mm.sd_negative_prompt !== undefined) document.getElementById("mm-sd-neg-prompt").value = mm.sd_negative_prompt;
  if (mm.whisper_language !== undefined) document.getElementById("mm-whisper-lang").value = mm.whisper_language;
  if (mm.whisper_beam_size !== undefined) document.getElementById("mm-whisper-beam").value = mm.whisper_beam_size;

  const cfgSlider = document.getElementById("mm-sd-cfg-scale");
  cfgSlider?.addEventListener("input", (e) => {
    const el = document.getElementById("val-sd-cfg-scale");
    if (el) el.textContent = e.target.value;
  });
}

export async function saveMultimodalSettings() {
  const payload = {
    multimodal: {
      sd_steps: parseInt(document.getElementById("mm-sd-steps")?.value || 20),
      sd_cfg_scale: parseFloat(document.getElementById("mm-sd-cfg-scale")?.value || 7.5),
      sd_width: parseInt(document.getElementById("mm-sd-width")?.value || 512),
      sd_height: parseInt(document.getElementById("mm-sd-height")?.value || 512),
      sd_negative_prompt: document.getElementById("mm-sd-neg-prompt")?.value || "",
      whisper_language: document.getElementById("mm-whisper-lang")?.value || "auto",
      whisper_beam_size: parseInt(document.getElementById("mm-whisper-beam")?.value || 5)
    }
  };

  try {
    const res = await apiPost("/api/settings", payload);
    if (res.success) showToast("Multimodal configuration saved", "success");
    else showToast(res.error || "Failed to save multimodal settings", "error");
  } catch (err) {
    showToast("Error saving multimodal settings", "error");
  }
}
