// DenseLite Control Plane API Client

export const API_BASE = window.location.origin;

export async function apiGet(endpoint) {
  try {
    const res = await fetch(API_BASE + endpoint);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (err) {
    console.error(`[API] GET ${endpoint} error:`, err);
    throw err;
  }
}

export async function apiPost(endpoint, body = {}) {
  try {
    const res = await fetch(API_BASE + endpoint, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    return await res.json();
  } catch (err) {
    console.error(`[API] POST ${endpoint} error:`, err);
    throw err;
  }
}
