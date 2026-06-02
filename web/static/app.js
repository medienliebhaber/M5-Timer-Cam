/* ── Tab switching ─────────────────────────────────────────────────────── */
document.querySelectorAll(".tab").forEach(btn => {
  btn.addEventListener("click", () => {
    const tab = btn.dataset.tab;
    document.querySelectorAll(".tab").forEach(b => b.classList.remove("active"));
    document.querySelectorAll(".pane").forEach(p => p.classList.remove("active"));
    btn.classList.add("active");
    document.getElementById(`pane-${tab}`).classList.add("active");
    if (tab === "gallery") loadGallery();
  });
});

/* ── Live view ─────────────────────────────────────────────────────────── */
const liveImg    = document.getElementById("live-img");
const statusBadge = document.getElementById("camera-status");
let liveTimer = null;

function pollLive() {
  const url = `/api/snapshot?t=${Date.now()}`;
  fetch(url)
    .then(r => {
      if (!r.ok) throw new Error("offline");
      return r.blob();
    })
    .then(blob => {
      const prev = liveImg.src;
      liveImg.src = URL.createObjectURL(blob);
      if (prev.startsWith("blob:")) URL.revokeObjectURL(prev);
      setStatus(true);
    })
    .catch(() => setStatus(false));
}

function setStatus(online) {
  statusBadge.textContent = online ? "Camera online" : "Camera offline";
  statusBadge.className = `status ${online ? "online" : "offline"}`;
}

function startLivePolling() {
  pollLive();
  liveTimer = setInterval(pollLive, 1000);
}

function stopLivePolling() {
  clearInterval(liveTimer);
}

document.getElementById("tab-live").addEventListener("click", startLivePolling);
document.getElementById("tab-gallery").addEventListener("click", stopLivePolling);
startLivePolling(); // start on page load

/* ── Gallery ───────────────────────────────────────────────────────────── */
let galleryPage = 1;
const perPage = 30;

async function loadGallery() {
  const from = document.getElementById("filter-from").value;
  const to   = document.getElementById("filter-to").value;
  const params = new URLSearchParams({ page: galleryPage, per_page: perPage });
  if (from) params.set("from_dt", from + "T00:00:00Z");
  if (to)   params.set("to_dt",   to   + "T23:59:59Z");

  const resp = await fetch(`/api/frames?${params}`);
  const data = await resp.json();

  document.getElementById("gallery-count").textContent = `${data.total} frames`;
  document.getElementById("page-info").textContent = `Page ${galleryPage}`;
  document.getElementById("btn-prev").disabled = galleryPage <= 1;
  document.getElementById("btn-next").disabled = (galleryPage * perPage) >= data.total;

  const grid = document.getElementById("gallery-grid");
  grid.innerHTML = "";
  data.frames.forEach(frame => {
    const item = document.createElement("div");
    item.className = "grid-item";
    const ts = new Date(frame.captured_at).toLocaleString();
    item.innerHTML = `
      <img src="/api/frames/${frame.id}" loading="lazy" alt="${ts}">
      <div class="meta">${ts} · ${frame.trigger}</div>
    `;
    item.addEventListener("click", () => openLightbox(frame));
    grid.appendChild(item);
  });
}

document.getElementById("btn-filter").addEventListener("click", () => {
  galleryPage = 1;
  loadGallery();
});
document.getElementById("btn-prev").addEventListener("click", () => {
  if (galleryPage > 1) { galleryPage--; loadGallery(); }
});
document.getElementById("btn-next").addEventListener("click", () => {
  galleryPage++;
  loadGallery();
});

/* ── Lightbox ──────────────────────────────────────────────────────────── */
const lightbox     = document.getElementById("lightbox");
const lightboxImg  = document.getElementById("lightbox-img");
const lightboxMeta = document.getElementById("lightbox-meta");

function openLightbox(frame) {
  lightboxImg.src = `/api/frames/${frame.id}`;
  const ts = new Date(frame.captured_at).toLocaleString();
  const size = frame.filesize ? `${(frame.filesize / 1024).toFixed(1)} KB` : "";
  const dim  = (frame.width && frame.height) ? `${frame.width}×${frame.height}` : "";
  lightboxMeta.textContent = [ts, frame.trigger, dim, size].filter(Boolean).join(" · ");
  lightbox.classList.remove("hidden");
}

document.getElementById("lightbox-close").addEventListener("click", () => {
  lightbox.classList.add("hidden");
  lightboxImg.src = "";
});
lightbox.addEventListener("click", e => {
  if (e.target === lightbox) {
    lightbox.classList.add("hidden");
    lightboxImg.src = "";
  }
});
document.addEventListener("keydown", e => {
  if (e.key === "Escape") {
    lightbox.classList.add("hidden");
    lightboxImg.src = "";
  }
});
