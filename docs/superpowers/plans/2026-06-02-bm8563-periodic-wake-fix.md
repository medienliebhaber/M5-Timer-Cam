# BM8563 Periodic Wake Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore periodic Timer Camera X boots by programming the BM8563 countdown timer before GPIO33 cuts ESP32 power.

**Architecture:** Keep the existing minute-based configuration contract. Add minimal RTC register write helpers inside the RTC component, configure the BM8563 countdown timer, retain the ESP32 deep-sleep timer as a USB-power fallback, and release battery hold only after successful timer setup.

**Tech Stack:** ESP-IDF C firmware, BM8563 RTC over I2C, FastAPI regression tests

---

### Task 1: Replace The Powered-Off ESP32 Timer Path

**Files:**
- Modify: `firmware/components/rtc/rtc.c`
- Modify: `firmware/components/rtc/CMakeLists.txt`
- Modify: `firmware/components/rtc/include/bm8563.h`

- [ ] **Step 1: Run a failing source-level regression check**

Run a Python assertion script that requires BM8563 timer register writes, the
ESP32 USB-power fallback, and the required setup order before GPIO33 release.

- [ ] **Step 2: Add BM8563 register helpers**

Add checked single-register I2C writes for control status, timer control, and
timer value registers.

- [ ] **Step 3: Configure RTC wake before releasing battery hold**

Disable stale timer state, enable timer interrupts, load the countdown, start
the timer, arm the ESP32 USB-power fallback timer, and then drive GPIO33 low.
Return immediately on any I2C error.

- [ ] **Step 4: Re-run the source-level regression check**

Expected: PASS.

- [ ] **Step 5: Build firmware**

Run: `. "$HOME/esp/esp-idf/export.sh" >/dev/null 2>&1 && cd firmware && idf.py build`

Expected: firmware build completes successfully.

### Task 2: Correct Maintained Documentation

**Files:**
- Modify: `docs/hardware.md`
- Modify: `docs/firmware/lifecycle.md`
- Modify: `docs/firmware/components.md`

- [ ] **Step 1: Replace ESP32 timer references**

Document that scheduled sleep programs the BM8563 countdown timer before
releasing GPIO33.

- [ ] **Step 2: Run documentation scan**

Run: `rg -n 'ESP32 timer|deep-sleep timer|internal timer' docs --glob '!docs/superpowers/**'`

Expected: maintained references describe BM8563 wake and the ESP32 USB-power
fallback accurately.

### Task 3: Regression Verification

**Files:**
- Verify only

- [ ] **Step 1: Run server regression suite**

Run: `cd server && .venv/bin/python -m pytest -q`

Expected: all tests pass.

- [ ] **Step 2: Inspect the final diff**

Run: `git diff --check && git status --short`

Expected: no whitespace errors; only intended files are modified.
