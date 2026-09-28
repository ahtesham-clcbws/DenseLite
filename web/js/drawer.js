// Responsive Drawer Controller

export function initDrawer() {
  const toggleBtn = document.getElementById("drawer-toggle");
  const closeBtn = document.getElementById("drawer-close");
  const backdrop = document.getElementById("drawer-backdrop");

  if (toggleBtn) toggleBtn.addEventListener("click", openDrawer);
  if (closeBtn) closeBtn.addEventListener("click", closeDrawer);
  if (backdrop) backdrop.addEventListener("click", closeDrawer);
}

export function openDrawer() {
  const sidebar = document.getElementById("sidebar");
  const backdrop = document.getElementById("drawer-backdrop");
  if (sidebar) sidebar.classList.add("open");
  if (backdrop) backdrop.classList.add("active");
}

export function closeDrawer() {
  const sidebar = document.getElementById("sidebar");
  const backdrop = document.getElementById("drawer-backdrop");
  if (sidebar) sidebar.classList.remove("open");
  if (backdrop) backdrop.classList.remove("active");
}
