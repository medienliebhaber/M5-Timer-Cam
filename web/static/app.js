/* ── Shared state ─────────────────────────────────────────────────────── */
let activeTab = 'live';

/* ── Tab switching ────────────────────────────────────────────────────── */
function switchTab(tab) {
  activeTab = tab;
  document.querySelectorAll('[data-tab]').forEach(el => {
    el.classList.toggle('active', el.dataset.tab === tab);
  });
  document.querySelectorAll('.pane').forEach(p => {
    p.classList.toggle('active', p.id === `pane-${tab}`);
  });
  if (tab === 'live') {
    stopGalleryWatch();
    startLivePolling();
  } else {
    stopLivePolling();
    if (tab === 'gallery') {
      loadGallery();
      startGalleryWatch();
    } else {
      stopGalleryWatch();
    }
    if (tab === 'playback') initPlayback();
  }
}

document.querySelectorAll('[data-tab]').forEach(btn => {
  btn.addEventListener('click', () => switchTab(btn.dataset.tab));
});

/* ── Live view ────────────────────────────────────────────────────────── */
const liveImg     = document.getElementById('live-img');
const statusBadge = document.getElementById('camera-status');
const statusText  = document.getElementById('status-text');
const liveTs      = document.getElementById('live-ts');
let livePolling   = false;
let liveTimer     = null;

async function pollLive() {
  if (!livePolling) return;
  try {
    const r = await fetch(`/api/snapshot?t=${Date.now()}`);
    if (!r.ok) throw new Error(r.status);
    const blob       = await r.blob();
    const source     = r.headers.get('X-Snapshot-Source') ?? 'cached';
    const capturedAt = r.headers.get('X-Captured-At');
    const prev = liveImg.src;
    liveImg.src = URL.createObjectURL(blob);
    if (prev && prev.startsWith('blob:')) URL.revokeObjectURL(prev);
    setSnapshotSource(source);
    liveTs.textContent = source === 'cached' && capturedAt
      ? `Cached · ${formatTs(capturedAt)}`
      : new Date().toLocaleTimeString();
  } catch {
    setSnapshotSource('offline');
  } finally {
    if (livePolling) liveTimer = setTimeout(pollLive, 1000);
  }
}

function setSnapshotSource(source) {
  statusBadge.className = `status ${source}`;
  statusText.textContent = source === 'live'   ? 'Live'
    : source === 'cached' ? 'Cached frame'
    : 'Offline';
}

function startLivePolling() {
  if (livePolling) return;
  livePolling = true;
  pollLive();
}
function stopLivePolling() {
  livePolling = false;
  clearTimeout(liveTimer);
  liveTimer = null;
}

startLivePolling();

/* ── Gallery ───────────────────────────────────────────────────────────── */
const grid       = document.getElementById('gallery-grid');
const emptyState = document.getElementById('gallery-empty');

let currentFrames    = [];
let currentIndex     = -1;
let galleryPage      = 1;
let galleryTotal     = 0;
let galleryLatestId  = 0;
let galleryWatchTimer = null;
const PER_PAGE       = 30;

async function loadGallery() {
  const from = document.getElementById('filter-from').value;
  const to   = document.getElementById('filter-to').value;
  const params = new URLSearchParams({ page: galleryPage, per_page: PER_PAGE });
  if (from) params.set('from_dt', new Date(from).toISOString());
  if (to)   params.set('to_dt',   new Date(to).toISOString());

  let data;
  try {
    const r = await fetch(`/api/frames?${params}`);
    if (!r.ok) throw new Error();
    data = await r.json();
  } catch {
    return;
  }

  galleryTotal  = data.total ?? 0;
  currentFrames = data.frames ?? [];
  const newestId = currentFrames[0]?.id ?? 0;
  if (newestId) galleryLatestId = Math.max(galleryLatestId, newestId);

  const totalPages = Math.max(1, Math.ceil(galleryTotal / PER_PAGE));
  document.getElementById('gallery-count').textContent =
    galleryTotal > 0 ? `${galleryTotal} frame${galleryTotal === 1 ? '' : 's'}` : '';
  document.getElementById('page-info').textContent = `${galleryPage} / ${totalPages}`;
  document.getElementById('btn-prev').disabled = galleryPage <= 1;
  document.getElementById('btn-next').disabled = galleryPage * PER_PAGE >= galleryTotal;

  grid.innerHTML = '';
  if (galleryTotal === 0) {
    emptyState.classList.remove('hidden');
    return;
  }
  emptyState.classList.add('hidden');

  currentFrames.forEach((frame, i) => {
    const item = document.createElement('div');
    item.className = 'grid-item';
    const ts = formatTs(frame.captured_at);
    const img = document.createElement('img');
    img.src = `/api/frames/${frame.id}`;
    img.loading = 'lazy';
    img.alt = ts;
    const meta = document.createElement('div');
    meta.className = 'meta';
    meta.textContent = ts;
    item.appendChild(img);
    item.appendChild(meta);
    item.addEventListener('click', () => openLightbox(i));
    grid.appendChild(item);
  });
}

document.getElementById('btn-filter').addEventListener('click', () => {
  galleryPage = 1;
  loadGallery();
});
document.getElementById('btn-clear').addEventListener('click', () => {
  document.getElementById('filter-from').value = '';
  document.getElementById('filter-to').value   = '';
  galleryPage = 1;
  loadGallery();
});
document.getElementById('btn-refresh').addEventListener('click', () => loadGallery());
document.getElementById('btn-prev').addEventListener('click', () => { galleryPage--; loadGallery(); });
document.getElementById('btn-next').addEventListener('click', () => { galleryPage++; loadGallery(); });

/* ── Gallery new-frame watcher ─────────────────────────────────────────── */
async function watchGallery() {
  try {
    const r = await fetch('/api/frames?page=1&per_page=1');
    if (!r.ok) return;
    const d    = await r.json();
    const latest = d.frames?.[0]?.id ?? 0;
    if (galleryLatestId && latest > galleryLatestId) {
      const hasFilter = document.getElementById('filter-from').value
                     || document.getElementById('filter-to').value;
      if (galleryPage === 1 && !hasFilter) {
        loadGallery();
      } else {
        showToast('New frame captured — click Refresh to see it', 'info');
      }
    }
    if (latest) galleryLatestId = Math.max(galleryLatestId, latest);
  } catch { /* non-fatal */ }
  if (activeTab === 'gallery') galleryWatchTimer = setTimeout(watchGallery, 15000);
}

function startGalleryWatch() {
  stopGalleryWatch();
  watchGallery();
}

function stopGalleryWatch() {
  clearTimeout(galleryWatchTimer);
  galleryWatchTimer = null;
}

/* ── Lightbox ──────────────────────────────────────────────────────────── */
const lightbox   = document.getElementById('lightbox');
const lbImg      = document.getElementById('lightbox-img');
const lbMeta     = document.getElementById('lightbox-meta');
const lbCounter  = document.getElementById('lightbox-counter');
const lbDownload = document.getElementById('lightbox-download');
const btnLbPrev  = document.getElementById('lightbox-prev');
const btnLbNext  = document.getElementById('lightbox-next');

function showFrameAt(index) {
  if (index < 0 || index >= currentFrames.length) return;
  currentIndex = index;
  const frame  = currentFrames[index];

  lbImg.src       = `/api/frames/${frame.id}`;
  lbDownload.href = `/api/frames/${frame.id}`;
  lbDownload.download = frame.filename
    ? frame.filename.split('/').pop()
    : `frame-${frame.id}.jpg`;

  const size = frame.filesize ? ` · ${(frame.filesize / 1024).toFixed(0)} KB` : '';
  const dim  = frame.width   ? ` · ${frame.width}×${frame.height}` : '';
  lbMeta.textContent    = formatTs(frame.captured_at) + dim + size;
  lbCounter.textContent = `${index + 1} / ${currentFrames.length}`;

  btnLbPrev.disabled = index === 0;
  btnLbNext.disabled = index === currentFrames.length - 1;
}

function openLightbox(index) {
  lightbox.classList.remove('hidden');
  document.body.style.overflow = 'hidden';
  showFrameAt(index);
}

function closeLightbox() {
  lightbox.classList.add('hidden');
  document.body.style.overflow = '';
  lbImg.src    = '';
  currentIndex = -1;
}

btnLbPrev.addEventListener('click', e => { e.stopPropagation(); showFrameAt(currentIndex - 1); });
btnLbNext.addEventListener('click', e => { e.stopPropagation(); showFrameAt(currentIndex + 1); });
document.getElementById('lightbox-close').addEventListener('click', closeLightbox);
document.querySelector('.lightbox-backdrop').addEventListener('click', closeLightbox);

document.addEventListener('keydown', e => {
  if (lightbox.classList.contains('hidden')) return;
  if (e.key === 'Escape')     closeLightbox();
  if (e.key === 'ArrowLeft')  showFrameAt(currentIndex - 1);
  if (e.key === 'ArrowRight') showFrameAt(currentIndex + 1);
});

/* ── Toast notifications ───────────────────────────────────────────────── */
function showToast(message, type = 'info') {
  const t = document.createElement('div');
  t.className = `toast toast-${type}`;
  t.setAttribute('role', 'status');
  t.setAttribute('aria-live', 'polite');
  t.textContent = message;
  document.body.appendChild(t);
  requestAnimationFrame(() => { requestAnimationFrame(() => t.classList.add('visible')); });
  setTimeout(() => {
    t.classList.remove('visible');
    setTimeout(() => t.remove(), 300);
  }, 4500);
}

/* ── Playback ──────────────────────────────────────────────────────────── */
let playbackFrames  = [];
let playbackIndex   = 0;
let playbackTimer   = null;
let playbackPlaying = false;
let playbackFPS     = 60;
let playbackInited  = false;
let latestFrameTs   = null;

const pbImg      = document.getElementById('playback-img');
const pbEmpty    = document.getElementById('playback-empty');
const pbStage    = document.getElementById('playback-stage');
const pbProgress = document.getElementById('playback-progress');
const pbFrameNum = document.getElementById('playback-frame-num');
const pbTotal    = document.getElementById('playback-total');
const pbTs       = document.getElementById('playback-ts');
const fpsSlider  = document.getElementById('fps-slider');
const fpsVal     = document.getElementById('fps-value');

fpsSlider.addEventListener('input', () => {
  playbackFPS        = parseInt(fpsSlider.value, 10);
  fpsVal.textContent = `${playbackFPS} fps`;
  if (playbackPlaying) {
    clearInterval(playbackTimer);
    playbackTimer = setInterval(pbStep, 1000 / playbackFPS);
  }
});

function toDatetimeLocal(iso) {
  const d   = new Date(iso);
  const pad = n => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}`;
}

async function initPlayback() {
  if (playbackInited) return;
  try {
    const newest = await fetch('/api/frames?per_page=1&page=1').then(r => r.json());
    if (!newest.total) return;
    const oldest = await fetch(`/api/frames?per_page=1&page=${newest.total}`).then(r => r.json());
    const newestTs   = newest.frames?.[0]?.captured_at;
    const oldestTs   = oldest.frames?.[0]?.captured_at;
    latestFrameTs    = newestTs ?? null;
    const newestDate = newestTs ? new Date(newestTs) : null;
    const now        = new Date();
    const isRecent   = newestDate && (now - newestDate) < 24 * 3600_000;
    const pbFrom = document.getElementById('pb-from');
    const pbTo   = document.getElementById('pb-to');
    if (!pbTo.value)   pbTo.value   = toDatetimeLocal(isRecent ? now.toISOString() : newestTs);
    if (!pbFrom.value) pbFrom.value = toDatetimeLocal(isRecent ? new Date(now - 3600_000).toISOString() : oldestTs);
    document.getElementById('pb-range-hint').textContent =
      `${oldestTs ? formatTs(oldestTs) : '?'} → ${newestTs ? formatTs(newestTs) : '?'}`;
    playbackInited = true;
  } catch { /* non-fatal */ }
}

function applyPreset(ms) {
  const ref = latestFrameTs ? new Date(latestFrameTs) : new Date();
  document.getElementById('pb-to').value   = toDatetimeLocal(ref.toISOString());
  document.getElementById('pb-from').value = toDatetimeLocal(new Date(ref - ms).toISOString());
  document.querySelectorAll('.preset-btn').forEach(b =>
    b.classList.toggle('active', parseInt(b.dataset.ms) === ms));
}

document.querySelectorAll('.preset-btn').forEach(btn => {
  btn.addEventListener('click', () => applyPreset(parseInt(btn.dataset.ms)));
});

async function loadPlayback() {
  const from = document.getElementById('pb-from').value;
  const to   = document.getElementById('pb-to').value;

  stopPlayback();
  playbackFrames = [];
  playbackIndex  = 0;

  const params = new URLSearchParams({ page: 1, per_page: 6000 });
  if (from) params.set('from_dt', new Date(from).toISOString());
  if (to)   params.set('to_dt',   new Date(to).toISOString());

  const btnLoad = document.getElementById('btn-pb-load');
  btnLoad.textContent = 'Loading…';
  btnLoad.disabled    = true;

  try {
    const r = await fetch(`/api/frames?${params}`);
    if (!r.ok) throw new Error(`Server error ${r.status}`);
    const data = await r.json();
    playbackFrames = (data.frames ?? []).reverse();
  } catch (err) {
    playbackFrames = [];
    showToast(`Load failed: ${err.message}`, 'error');
  } finally {
    btnLoad.textContent = 'Load';
    btnLoad.disabled    = false;
  }

  if (playbackFrames.length === 0) {
    pbEmpty.classList.remove('hidden');
    pbStage.classList.add('hidden');
    showToast('No frames found — try clearing the date filter or adjusting the range', 'warn');
    updatePbButtons();
    return;
  }

  pbTotal.textContent = playbackFrames.length;
  pbEmpty.classList.add('hidden');
  pbStage.classList.remove('hidden');
  showToast(`${playbackFrames.length} frames loaded`, 'success');
  showPbFrame(0);
  updatePbButtons();
}

function showPbFrame(index) {
  if (index < 0 || index >= playbackFrames.length) return;
  playbackIndex = index;
  const frame   = playbackFrames[index];
  pbImg.src     = `/api/frames/${frame.id}`;

  const pct = playbackFrames.length > 1
    ? (index / (playbackFrames.length - 1)) * 100
    : 100;
  pbProgress.style.width = `${pct.toFixed(1)}%`;
  pbFrameNum.textContent = index + 1;
  pbTs.textContent       = formatTs(frame.captured_at);
}

function pbStep() {
  if (playbackIndex >= playbackFrames.length - 1) {
    stopPlayback();
    return;
  }
  showPbFrame(playbackIndex + 1);
}

function startPlayback() {
  if (!playbackFrames.length || playbackPlaying) return;
  if (playbackIndex >= playbackFrames.length - 1) {
    playbackIndex = 0;
    showPbFrame(0);
  }
  playbackPlaying = true;
  playbackTimer   = setInterval(pbStep, 1000 / playbackFPS);
  updatePbButtons();
}

function pausePlayback() {
  if (!playbackPlaying) return;
  clearInterval(playbackTimer);
  playbackTimer   = null;
  playbackPlaying = false;
  updatePbButtons();
}

function stopPlayback() {
  clearInterval(playbackTimer);
  playbackTimer   = null;
  playbackPlaying = false;
  if (playbackFrames.length) showPbFrame(0);
  updatePbButtons();
}

function updatePbButtons() {
  document.getElementById('btn-pb-play').disabled  = playbackPlaying || !playbackFrames.length;
  document.getElementById('btn-pb-pause').disabled = !playbackPlaying;
  document.getElementById('btn-pb-stop').disabled  = !playbackFrames.length;
}

['pb-from','pb-to'].forEach(id => {
  document.getElementById(id).addEventListener('input', () =>
    document.querySelectorAll('.preset-btn').forEach(b => b.classList.remove('active')));
});

document.getElementById('btn-pb-load').addEventListener('click',  loadPlayback);
document.getElementById('btn-pb-play').addEventListener('click',  startPlayback);
document.getElementById('btn-pb-pause').addEventListener('click', pausePlayback);
document.getElementById('btn-pb-stop').addEventListener('click',  stopPlayback);

updatePbButtons();

/* ── Settings modal ────────────────────────────────────────────────────── */
const settingsModal = document.getElementById('settings-modal');

document.getElementById('btn-settings').addEventListener('click', openSettings);
document.getElementById('btn-settings-close').addEventListener('click', closeSettings);
document.querySelector('.modal-backdrop').addEventListener('click', closeSettings);

function openSettings() {
  document.getElementById('btn-power-off').disabled = false;
  settingsModal.classList.remove('hidden');
  loadHwStatus();
  loadStorageStats();
  loadCameraConfig();
}
function closeSettings() {
  restoreSavedPreview();
  settingsModal.classList.add('hidden');
}

/* ── Hardware status ───────────────────────────────────────────────────── */
async function loadHwStatus() {
  const online  = document.getElementById('hw-online');
  const offline = document.getElementById('hw-offline');
  const sourceEl = document.getElementById('hw-source');
  try {
    const r = await fetch('/api/camera/status');
    if (!r.ok) throw new Error();
    const d = await r.json();

    if (d.source === 'offline') throw new Error();

    online.classList.remove('hidden');
    offline.classList.add('hidden');

    const pct = d.battery_pct ?? 0;
    document.getElementById('battery-fill').style.width = `${pct}%`;
    document.getElementById('battery-fill').style.background =
      pct > 50 ? 'var(--green)' : pct > 20 ? '#f59e0b' : 'var(--red)';
    document.getElementById('battery-pct').textContent = `${pct}%`;
    document.getElementById('heap-free').textContent =
      d.heap_free != null ? `${(d.heap_free / 1024).toFixed(0)} KB` : '—';
    document.getElementById('rssi-val').textContent =
      d.rssi != null ? `${d.rssi} dBm` : '—';

    if (sourceEl) {
      if (d.source === 'cached' && d.seen_at) {
        sourceEl.textContent = `cached · ${formatTs(d.seen_at)}`;
        sourceEl.classList.remove('hidden');
      } else {
        sourceEl.classList.add('hidden');
      }
    }
  } catch {
    online.classList.add('hidden');
    offline.classList.remove('hidden');
  }
}

document.getElementById('btn-refresh-status').addEventListener('click', loadHwStatus);

/* ── Camera config ─────────────────────────────────────────────────────── */
const intervalSlider = document.getElementById('interval-slider');
const intervalValue  = document.getElementById('interval-value');
const sleepToggle    = document.getElementById('sleep-toggle');
const configHint     = document.getElementById('config-push-hint');
const previewImg     = document.getElementById('config-preview-img');
const previewHint    = document.getElementById('config-preview-hint');
const framesizeSelect = document.getElementById('framesize-select');
const hmirrorToggle  = document.getElementById('hmirror-toggle');
const vflipToggle    = document.getElementById('vflip-toggle');
const imageSliders   = ['quality', 'brightness', 'contrast', 'saturation', 'sharpness'];
let savedImageConfig = null;
let previewDirty     = false;
let previewTimer     = null;
let previewAbort     = null;

intervalSlider.addEventListener('input', () => {
  intervalValue.textContent = `${intervalSlider.value} min`;
});

function readImageConfig() {
  const config = {
    framesize: framesizeSelect.value,
    hmirror: hmirrorToggle.checked,
    vflip: vflipToggle.checked,
  };
  imageSliders.forEach(name => {
    config[name] = parseInt(document.getElementById(`${name}-slider`).value, 10);
  });
  return config;
}

function writeImageConfig(config) {
  framesizeSelect.value = config.framesize ?? 'UXGA';
  hmirrorToggle.checked = config.hmirror !== false;
  vflipToggle.checked = config.vflip !== false;
  imageSliders.forEach(name => {
    const value = config[name] ?? (name === 'quality' ? 12 : 0);
    document.getElementById(`${name}-slider`).value = value;
    document.getElementById(`${name}-value`).textContent = value;
  });
}

async function requestPreview(config = readImageConfig()) {
  if (previewAbort) previewAbort.abort();
  previewAbort = new AbortController();
  previewHint.textContent = 'Updating real camera preview…';
  previewHint.classList.remove('hidden');
  try {
    const r = await fetch('/api/camera/preview', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(config),
      signal: previewAbort.signal,
    });
    if (!r.ok) throw new Error();
    const blob = await r.blob();
    const previous = previewImg.src;
    previewImg.src = URL.createObjectURL(blob);
    if (previous && previous.startsWith('blob:')) URL.revokeObjectURL(previous);
    previewHint.classList.add('hidden');
  } catch (err) {
    if (err.name === 'AbortError') return;
    previewHint.textContent = 'Preview requires an awake camera. Changes can still be saved for the next wake.';
    previewHint.classList.remove('hidden');
  }
}

function schedulePreview() {
  previewDirty = true;
  clearTimeout(previewTimer);
  previewTimer = setTimeout(() => requestPreview(), 250);
}

function restoreSavedPreview() {
  clearTimeout(previewTimer);
  if (!previewDirty || !savedImageConfig) return;
  requestPreview(savedImageConfig);
  previewDirty = false;
}

framesizeSelect.addEventListener('change', schedulePreview);
hmirrorToggle.addEventListener('change', schedulePreview);
vflipToggle.addEventListener('change', schedulePreview);
imageSliders.forEach(name => {
  document.getElementById(`${name}-slider`).addEventListener('input', event => {
    document.getElementById(`${name}-value`).textContent = event.target.value;
    schedulePreview();
  });
});

async function loadCameraConfig() {
  try {
    const r = await fetch('/api/camera/config');
    if (!r.ok) throw new Error();
    const d = await r.json();
    const mins = d.interval_minutes ?? 1;
    intervalSlider.value     = mins;
    intervalValue.textContent = `${mins} min`;
    sleepToggle.checked      = d.sleep_enabled !== false;
    writeImageConfig(d);
    savedImageConfig = readImageConfig();
    previewDirty = false;
    requestPreview(savedImageConfig);
    configHint.classList.add('hidden');
  } catch { /* keep defaults */ }
}

document.getElementById('btn-save-config').addEventListener('click', async () => {
  const btn = document.getElementById('btn-save-config');
  btn.disabled = true;
  try {
    const body = {
      interval_minutes: parseInt(intervalSlider.value, 10),
      sleep_enabled: sleepToggle.checked,
      ...readImageConfig(),
    };
    const r = await fetch('/api/camera/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    if (!r.ok) throw new Error(`server error ${r.status}`);
    const d = await r.json();
    savedImageConfig = readImageConfig();
    previewDirty = false;
    if (d.pushed_to_camera) {
      showToast('Config saved and applied to camera', 'success');
      configHint.classList.add('hidden');
    } else {
      showToast('Config saved — will apply on next camera wake', 'warn');
      configHint.classList.remove('hidden');
    }
  } catch (err) {
    showToast(`Save failed: ${err.message}`, 'error');
  } finally {
    btn.disabled = false;
  }
});

/* ── Device power ─────────────────────────────────────────────────────── */
document.getElementById('btn-power-off').addEventListener('click', async () => {
  if (!confirm('Turn off scheduled captures until USB is reconnected or the hardware wake/reset button is pressed?')) return;
  const btn = document.getElementById('btn-power-off');
  btn.disabled = true;
  try {
    const r = await fetch('/api/camera/power-off', { method: 'POST' });
    if (!r.ok) throw new Error(`server error ${r.status}`);
    settingsModal.classList.add('hidden');
    showToast('Device turned off — reconnect USB or press the hardware wake/reset button to resume scheduled captures', 'success');
  } catch (err) {
    showToast(`Power off failed: ${err.message}`, 'error');
    btn.disabled = false;
  }
});

/* ── Storage stats ─────────────────────────────────────────────────────── */
async function loadStorageStats() {
  try {
    const r = await fetch('/api/storage');
    if (!r.ok) throw new Error();
    const d = await r.json();
    document.getElementById('storage-frames').textContent =
      (d.frame_count ?? 0).toLocaleString();
    document.getElementById('storage-disk').textContent =
      d.disk_used_mb != null ? `${d.disk_used_mb} MB` : '—';
  } catch { /* non-fatal */ }
}

/* ── Helpers ───────────────────────────────────────────────────────────── */
function formatTs(iso) {
  if (!iso) return '';
  try { return new Date(iso).toLocaleString(); } catch { return iso; }
}
