# Documentation Structure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace duplicated documentation with a navigable, topic-owned documentation tree that reflects the committed application.

**Architecture:** Keep `README.md`, `CLAUDE.md`, and directory `README.md` files as short indexes. Move detailed reference material into focused topic files so every maintained fact has one source of truth.

**Tech Stack:** Markdown, shell link checks, Git

---

### Task 1: Create Entry Points

**Files:**
- Modify: `README.md`
- Modify: `CLAUDE.md`
- Create: `docs/README.md`
- Create: `docs/architecture.md`
- Create: `docs/setup.md`

- [ ] **Step 1:** Rewrite the root entry points as short indexes.
- [ ] **Step 2:** Add canonical architecture and setup topic files.
- [ ] **Step 3:** Check that all links resolve.

Run:

```bash
python3 - <<'PY'
from pathlib import Path
import re
for source in [Path("README.md"), Path("CLAUDE.md"), Path("docs/README.md")]:
    for target in re.findall(r"\[[^]]+\]\(([^)#]+)", source.read_text()):
        assert (source.parent / target).resolve().exists(), (source, target)
PY
```

Expected: exit code `0`.

### Task 2: Split Firmware Reference

**Files:**
- Delete: `docs/firmware.md`
- Create: `docs/firmware/README.md`
- Create: `docs/firmware/lifecycle.md`
- Create: `docs/firmware/configuration.md`
- Create: `docs/firmware/components.md`
- Create: `docs/firmware/http-api.md`
- Create: `docs/firmware/partitions.md`

- [ ] **Step 1:** Move firmware details into topic-owned files.
- [ ] **Step 2:** Correct test-mode, timed sleep, camera API, and OTA partition details.
- [ ] **Step 3:** Search active docs for the superseded file path.

Run:

```bash
rg -n 'docs/firmware\.md|RTC alarm|sdkconfig\.test' README.md CLAUDE.md docs --glob '!docs/superpowers/**'
```

Expected: no matches.

### Task 3: Split Server Reference

**Files:**
- Delete: `docs/server.md`
- Create: `docs/server/README.md`
- Create: `docs/server/http-api.md`
- Create: `docs/server/database.md`

- [ ] **Step 1:** Move server setup, API, and persistence facts into their owning files.
- [ ] **Step 2:** Ensure camera-management, storage-statistics, and OTA proxy routes are documented.
- [ ] **Step 3:** Search active docs for the superseded file path.

Run:

```bash
rg -n 'docs/server\.md' README.md CLAUDE.md docs --glob '!docs/superpowers/**'
```

Expected: no matches.

### Task 4: Tighten Maintainer Guides

**Files:**
- Modify: `docs/development.md`
- Modify: `docs/hardware.md`
- Modify: `docs/web-ui.md`

- [ ] **Step 1:** Keep each guide focused on its owning topic.
- [ ] **Step 2:** Add links to canonical topic files instead of repeating details.
- [ ] **Step 3:** Validate all active Markdown links and diff whitespace.

Run:

```bash
python3 - <<'PY'
from pathlib import Path
import re
for source in [Path("README.md"), Path("CLAUDE.md"), *Path("docs").rglob("*.md")]:
    if "docs/superpowers" in source.as_posix():
        continue
    for target in re.findall(r"\[[^]]+\]\(([^)#]+)", source.read_text()):
        if "://" not in target:
            assert (source.parent / target).resolve().exists(), (source, target)
PY
git diff --check
```

Expected: both commands exit with code `0`.
