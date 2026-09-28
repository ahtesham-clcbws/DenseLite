// Client-Side Hash Router

import { closeDrawer } from "./drawer.js";

export const VALID_TABS = ["dashboard", "inference", "models", "storage", "server", "system", "logs"];

export function initRouter(onTabChange) {
  window.addEventListener("hashchange", () => handleRouteChange(onTabChange));

  document.querySelectorAll(".nav-item").forEach(link => {
    link.addEventListener("click", (e) => {
      e.preventDefault();
      const tab = link.dataset.tab;
      if (tab) {
        if (window.location.hash === `#/${tab}`) {
          handleRouteChange(onTabChange);
        } else {
          window.location.hash = `#/${tab}`;
        }
        closeDrawer();
      }
    });
  });

  handleRouteChange(onTabChange);
}

export function handleRouteChange(onTabChange) {
  const hash = window.location.hash.replace(/^#\/?/, "");
  const activeTab = VALID_TABS.includes(hash) ? hash : "dashboard";

  document.querySelectorAll(".nav-item").forEach(b => {
    b.classList.toggle("active", b.dataset.tab === activeTab);
  });
  document.querySelectorAll(".tab-pane").forEach(p => {
    p.classList.toggle("active", p.id === `pane-${activeTab}`);
  });

  if (!window.location.hash || window.location.hash !== `#/${activeTab}`) {
    history.replaceState(null, "", `#/${activeTab}`);
  }

  if (typeof onTabChange === "function") {
    onTabChange(activeTab);
  }
}

export function getActiveTab() {
  const hash = window.location.hash.replace(/^#\/?/, "");
  return VALID_TABS.includes(hash) ? hash : "dashboard";
}
