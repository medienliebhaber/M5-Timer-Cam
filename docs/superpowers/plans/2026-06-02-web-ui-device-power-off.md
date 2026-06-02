# Web UI Device Power Off Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a guarded browser action that powers off the Timer Camera X indefinitely until USB reconnect or hardware wake/reset.

**Architecture:** Add a timerless shutdown helper to the RTC component, expose it through camera `POST /power-off`, and proxy that action through FastAPI without persistence or retries. Add a destructive Settings-modal button with browser confirmation, then document the distinction between scheduled BM8563 sleep and manual indefinite shutdown.

**Tech Stack:** ESP-IDF C firmware, BM8563 RTC over I2C, FastAPI, pytest, plain HTML/CSS/JavaScript

---

### Task 1: Add The Firmware Indefinite Shutdown Path

**Files:**
- Modify: `firmware/components/rtc/include/bm8563.h`
- Modify: `firmware/components/rtc/rtc.c`
- Modify: `firmware/components/http_server/http_server.c`

- [ ] **Step 1: Run a failing source-level regression check**

Run:

```bash
python3 - <<'PY'
from pathlib import Path

rtc = Path("firmware/components/rtc/rtc.c").read_text()
header = Path("firmware/components/rtc/include/bm8563.h").read_text()
server = Path("firmware/components/http_server/http_server.c").read_text()

assert "esp_err_t bm8563_power_off(void);" in header
assert "esp_err_t bm8563_power_off(void)" in rtc
helper = rtc[rtc.index("esp_err_t bm8563_power_off(void)"):]
assert "bm8563_write_reg(BM8563_REG_TIMER_CTRL, 0)" in helper
assert "gpio_set_level(BAT_HOLD_PIN, 0)" in helper
assert "esp_deep_sleep_start()" in helper
assert "esp_sleep_enable_timer_wakeup" not in helper.split("}", 1)[0]
assert helper.index("bm8563_write_reg(BM8563_REG_TIMER_CTRL, 0)") < helper.index("gpio_set_level(BAT_HOLD_PIN, 0)")
assert '"/power-off"' in server
assert "bm8563_power_off()" in server
print("PASS")
PY
```

Expected: FAIL because `bm8563_power_off` and `POST /power-off` do not exist.

- [ ] **Step 2: Declare the timerless shutdown helper**

Add to `firmware/components/rtc/include/bm8563.h`:

```c
/* Disable scheduled wake, release Battery Hold (GPIO33 LOW), and enter ESP32
 * timerless deep sleep. The device stays inactive until external wake/reset. */
esp_err_t bm8563_power_off(void);
```

- [ ] **Step 3: Implement the timerless shutdown helper**

Add to `firmware/components/rtc/rtc.c`:

```c
esp_err_t bm8563_power_off(void)
{
    ESP_LOGI(TAG, "powering off indefinitely");

    esp_err_t err = bm8563_write_reg(BM8563_REG_TIMER_CTRL, 0);
    if (err != ESP_OK) return err;

    err = gpio_set_level(BAT_HOLD_PIN, 0);
    if (err != ESP_OK) return err;

    esp_deep_sleep_start();
    return ESP_OK; /* never reached while the ESP32 remains powered */
}
```

- [ ] **Step 4: Add camera `POST /power-off`**

Add `#include "bm8563.h"` to `firmware/components/http_server/http_server.c`.
Add this handler and URI before the server lifecycle section:

```c
/* ── POST /power-off ──────────────────────────────────────────────────── */
static esp_err_t power_off_post_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, "{\"status\":\"powering_off\"}");
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(300));
    err = bm8563_power_off();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "power off failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

static const httpd_uri_t POWER_OFF_URI = {
    .uri      = "/power-off",
    .method   = HTTP_POST,
    .handler  = power_off_post_handler,
    .user_ctx = NULL,
};
```

Register `POWER_OFF_URI` in `http_server_start()` and include `/power-off` in
the startup log.

- [ ] **Step 5: Re-run the source-level regression check**

Run the command from Step 1.

Expected: `PASS`.

- [ ] **Step 6: Build firmware**

Run:

```bash
. "$HOME/esp/esp-idf/export.sh" >/dev/null 2>&1 && cd firmware && idf.py build
```

Expected: firmware build completes successfully.

- [ ] **Step 7: Commit**

```bash
git add firmware/components/rtc/include/bm8563.h firmware/components/rtc/rtc.c firmware/components/http_server/http_server.c
git commit -m "feat: add indefinite device power off endpoint"
```

### Task 2: Add The FastAPI Power-Off Proxy

**Files:**
- Modify: `server/app/api/camera.py`
- Modify: `server/tests/test_camera_api.py`

- [ ] **Step 1: Write failing proxy tests**

Append to `server/tests/test_camera_api.py`:

```python
def test_post_camera_power_off_forwards_to_live_device(client):
    with patch("app.api.camera.httpx.AsyncClient") as async_client:
        post = async_client.return_value.__aenter__.return_value.post = AsyncMock(
            return_value=_response(json_data={"status": "powering_off"})
        )

        response = client.post("/api/camera/power-off")

    assert response.status_code == 200
    assert response.json() == {"status": "powering_off"}
    post.assert_awaited_once()
    assert post.await_args.args[0].endswith("/power-off")


def test_post_camera_power_off_returns_503_when_camera_offline(client):
    with patch("app.api.camera.httpx.AsyncClient") as async_client:
        async_client.return_value.__aenter__.return_value.post = AsyncMock(
            side_effect=httpx.ConnectError("offline")
        )

        response = client.post("/api/camera/power-off")

    assert response.status_code == 503
```

- [ ] **Step 2: Run tests to verify they fail**

Run:

```bash
cd server && .venv/bin/python -m pytest tests/test_camera_api.py -q
```

Expected: FAIL because `/api/camera/power-off` does not exist.

- [ ] **Step 3: Implement the FastAPI proxy**

Add to `server/app/api/camera.py` before `storage_stats()`:

```python
@router.post("/api/camera/power-off")
async def power_off_camera() -> Any:
    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            r = await client.post(_camera_url("/power-off"))
            r.raise_for_status()
            return r.json()
    except Exception as exc:
        raise HTTPException(503, f"camera power off failed: {exc}")
```

- [ ] **Step 4: Run tests to verify they pass**

Run:

```bash
cd server && .venv/bin/python -m pytest tests/test_camera_api.py -q
```

Expected: all camera API tests pass.

- [ ] **Step 5: Commit**

```bash
git add server/app/api/camera.py server/tests/test_camera_api.py
git commit -m "feat: proxy device power off through server"
```

### Task 3: Add The Web UI Power-Off Control

**Files:**
- Modify: `web/index.html`
- Modify: `web/static/app.js`
- Modify: `web/static/style.css`

- [ ] **Step 1: Run a failing browser-source regression check**

Run:

```bash
python3 - <<'PY'
from pathlib import Path

html = Path("web/index.html").read_text()
js = Path("web/static/app.js").read_text()
css = Path("web/static/style.css").read_text()

assert 'id="btn-power-off"' in html
assert "Turn Off Device" in html
assert "confirm(" in js
assert "fetch('/api/camera/power-off'" in js
assert ".btn-danger" in css
print("PASS")
PY
```

Expected: FAIL because the button, browser handler, and destructive style do
not exist.

- [ ] **Step 2: Add the Settings-modal power section**

Insert before the Server Storage section in `web/index.html`:

```html
        <!-- Device power -->
        <div class="settings-section">
          <div class="settings-section-hdr">Device Power</div>
          <p class="settings-hint">Turns off scheduled captures until USB is reconnected or the hardware wake/reset button is pressed.</p>
          <button id="btn-power-off" class="btn btn-danger" style="width:100%">
            Turn Off Device
          </button>
        </div>
```

Increment the `app.js` cache-busting query string in `web/index.html`.

- [ ] **Step 3: Add the destructive button style**

Add to `web/static/style.css` near the existing button styles:

```css
.btn-danger {
  color: #fff;
  background: var(--red);
  border-color: var(--red);
}
.btn-danger:hover { filter: brightness(1.08); }
```

- [ ] **Step 4: Add the browser confirmation and request**

Add to `web/static/app.js` before the storage stats section:

```javascript
/* ── Device power ─────────────────────────────────────────────────────── */
document.getElementById('btn-power-off').addEventListener('click', async () => {
  if (!confirm('Turn off the device indefinitely? Scheduled captures stop until USB is reconnected or the hardware wake/reset button is pressed.')) return;

  const btn = document.getElementById('btn-power-off');
  btn.disabled = true;
  try {
    const r = await fetch('/api/camera/power-off', { method: 'POST' });
    if (!r.ok) throw new Error(`server error ${r.status}`);
    settingsModal.classList.add('hidden');
    showToast('Device turned off. Reconnect USB or press the hardware wake/reset button to start it again.', 'success');
  } catch (err) {
    showToast(`Power off failed: ${err.message}`, 'error');
    btn.disabled = false;
  }
});
```

- [ ] **Step 5: Run browser-source regression and syntax checks**

Run:

```bash
python3 - <<'PY'
from pathlib import Path

html = Path("web/index.html").read_text()
js = Path("web/static/app.js").read_text()
css = Path("web/static/style.css").read_text()

assert 'id="btn-power-off"' in html
assert "Turn Off Device" in html
assert "confirm(" in js
assert "fetch('/api/camera/power-off'" in js
assert ".btn-danger" in css
print("PASS")
PY
node --check web/static/app.js
```

Expected: `PASS` and JavaScript syntax check exits successfully.

- [ ] **Step 6: Commit**

```bash
git add web/index.html web/static/app.js web/static/style.css
git commit -m "feat: add web ui device power off control"
```

### Task 4: Document Manual Indefinite Shutdown

**Files:**
- Modify: `docs/firmware/http-api.md`
- Modify: `docs/server/http-api.md`
- Modify: `docs/web-ui.md`
- Modify: `docs/hardware.md`
- Modify: `docs/firmware/lifecycle.md`

- [ ] **Step 1: Update the camera HTTP API**

Add to `docs/firmware/http-api.md`:

```markdown
## `POST /power-off`

Disable scheduled wake, release battery hold on GPIO33, and enter timerless
ESP32 deep sleep. The device remains inactive until USB is unplugged and
reconnected or the hardware wake/reset button is pressed.
```

- [ ] **Step 2: Update the server HTTP API**

Add to the Camera Management section of `docs/server/http-api.md`:

```markdown
### `POST /api/camera/power-off`

Forward an immediate indefinite shutdown request to camera `POST /power-off`.
The action is not persisted or retried. Returns `503` when the camera is
unreachable.
```

- [ ] **Step 3: Update the web UI reference**

Add a `Device Power` subsection to `docs/web-ui.md`:

```markdown
### Device Power

**Turn Off Device** asks for confirmation, then requests indefinite shutdown.
Scheduled captures stop until USB is unplugged and reconnected or the hardware
wake/reset button is pressed. The action fails when the camera is already
offline.
```

- [ ] **Step 4: Update hardware and lifecycle references**

Add to `docs/hardware.md` under Power and to `docs/firmware/lifecycle.md` after
Scheduled Sleep:

```markdown
Manual web UI shutdown differs from scheduled sleep: it disables the BM8563
countdown timer, releases GPIO33, and enters ESP32 deep sleep without a timer
wake source. The device remains inactive until USB reconnect or hardware
wake/reset.
```

- [ ] **Step 5: Run documentation scan**

Run:

```bash
rg -n "power-off|Power Off|Turn Off Device|Manual web UI shutdown|indefinite shutdown" docs --glob '!docs/superpowers/**'
```

Expected: maintained references cover firmware API, server API, web UI,
hardware behavior, and firmware lifecycle.

- [ ] **Step 6: Commit**

```bash
git add docs/firmware/http-api.md docs/server/http-api.md docs/web-ui.md docs/hardware.md docs/firmware/lifecycle.md
git commit -m "docs: describe manual device power off"
```

### Task 5: Run Full Verification

**Files:**
- Verify only

- [ ] **Step 1: Run server regression suite**

Run:

```bash
cd server && .venv/bin/python -m pytest -q
```

Expected: all tests pass.

- [ ] **Step 2: Build firmware**

Run:

```bash
. "$HOME/esp/esp-idf/export.sh" >/dev/null 2>&1 && cd firmware && idf.py build
```

Expected: firmware build completes successfully.

- [ ] **Step 3: Run web UI syntax check**

Run:

```bash
node --check web/static/app.js
```

Expected: JavaScript syntax check exits successfully.

- [ ] **Step 4: Inspect repository state**

Run:

```bash
git diff --check
git status --short
```

Expected: no whitespace errors. Any remaining changes are identified and
explained.
