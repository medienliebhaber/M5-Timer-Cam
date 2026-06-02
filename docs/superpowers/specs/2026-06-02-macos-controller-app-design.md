# macOS Controller App

## Summary

Add a small windowed macOS SwiftUI utility for controlling the local FastAPI
server and ESP-IDF serial monitor from this repository. The app uses Swift
Package Manager only. It does not add an Xcode project and does not add process
controls to the browser UI.

## Goals

- Start, stop, and restart a FastAPI server process launched by the app.
- Start and stop an embedded serial-monitor process launched by the app.
- Keep logs hidden by default and reveal them on demand.
- Show local server information and provide shortcuts to the browser UI and
  stored-image folder.
- Use native SwiftUI materials for a compact macOS 26 glass-style appearance.
- Keep the app independent from unrelated processes already running on the
  workstation.

## Non-Goals

- No Xcode project.
- No App Store packaging, signing workflow, or distributable DMG.
- No browser-based process controls.
- No firmware build or flash button in the first version.
- No adoption, termination, or restart of server processes not launched by the
  app.
- No privileged helper process.

## Location and Build

Create a Swift package at:

```text
macos/M5TimerCamController/
```

The package contains an executable SwiftUI macOS application:

```text
macos/M5TimerCamController/
├── Package.swift
├── README.md
├── Sources/
│   └── M5TimerCamController/
│       ├── M5TimerCamControllerApp.swift
│       ├── AppModel.swift
│       ├── ManagedProcess.swift
│       ├── ServerClient.swift
│       └── ContentView.swift
└── Tests/
    └── M5TimerCamControllerTests/
        ├── ManagedProcessTests.swift
        └── ServerClientTests.swift
```

Build and run from the repository root:

```bash
cd macos/M5TimerCamController
swift build
swift run
```

The app targets macOS and SwiftUI. It does not require generated project files.

## Repository Root Resolution

The app needs the repository root to launch scripts and open `data/images`.

Resolution order:

1. Use the current working directory when it contains `scripts/monitor.sh`,
   `server/app/main.py`, and `data/`.
2. Walk upward from the current working directory until those markers exist.
3. Walk upward from the executable location until those markers exist.
4. If unresolved, show a blocking setup error with the expected repository
   markers.

The first version does not persist a manually selected repository path.

## Process Ownership

The app manages only child processes it launches itself.

It does not scan for, adopt, stop, or restart unrelated Uvicorn or serial
monitor processes. If another server already occupies port `8000`, app-owned
server startup fails and the Server card displays a clear error.

When the app closes its main window or terminates, it sends termination to its
active child processes and updates their state. If a child does not terminate
within a short grace period, the app force-terminates that child only.

## Server Process

The app launches:

```bash
cd server
.venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000
```

The process environment inherits the user's environment. The app checks that
`server/.venv/bin/uvicorn` exists before launch and reports a setup error when
it does not.

The Server card provides:

- **Start Server**
- **Stop Server**
- **Restart Server**
- Status badge: `Stopped`, `Starting`, `Running`, or `Failed`
- Server URL: `http://localhost:8000`
- Last startup or runtime error
- Frame count and disk usage from `GET /api/storage`
- **Open Web UI**
- **Open Images Folder**

The app polls `GET /api/storage` while the app-owned server is running. A
successful response updates server information. Poll failures do not stop the
child process automatically; they update the information state and keep logs
available for diagnosis.

**Open Web UI** opens `http://localhost:8000` using the system browser.

**Open Images Folder** creates `data/images` when missing and reveals it in
Finder.

## Serial Monitor Process

The app launches:

```bash
scripts/monitor.sh -p <selected-port>
```

The Serial Monitor card provides:

- Serial-port picker populated from `/dev/cu.*`
- **Refresh Ports**
- **Start Monitor**
- **Stop Monitor**
- Status badge: `Stopped`, `Starting`, `Running`, or `Failed`
- Last startup or runtime error

The picker shows every discovered `/dev/cu.*` entry. Likely USB serial devices
sort first. Common Bluetooth and debug-console entries remain available but
are labeled as unlikely TimerCam choices. If the selected port disappears, the
app stops the monitor child process and reports that the serial device was
disconnected.

Only one app-owned serial monitor process runs at a time.

## Embedded Logs

Logs are collapsed by default to keep the window compact.

The main window provides **Show Logs** and **Hide Logs**. When visible, the log
area contains:

- **Server** tab
- **Serial Monitor** tab
- Read-only monospaced text output
- **Clear**
- **Auto-scroll** toggle, enabled by default

Each managed process captures standard output and standard error through pipes.
The app appends decoded UTF-8 text on the main actor. It retains a bounded
in-memory log buffer per process so long-running monitor sessions do not grow
memory without limit. The oldest text is discarded when the cap is reached.

## Visual Design

Use native SwiftUI controls and system materials only:

- Translucent material background
- Rounded material cards for Server and Serial Monitor
- Compact status pills using semantic colors
- SF Symbols for start, stop, restart, browser, folder, terminal, and refresh
- A collapsible bottom log drawer using a material background
- Standard macOS spacing and typography

The appearance should feel native on macOS 26 without private APIs, custom
shader dependencies, or a third-party UI framework.

## State Model

`AppModel` owns:

- Repository root
- App-owned server `ManagedProcess`
- App-owned serial-monitor `ManagedProcess`
- Server storage information
- Discovered serial ports
- Selected serial port
- Selected log tab
- Log visibility and auto-scroll state

`ManagedProcess` owns:

- Process launch configuration
- Child process reference
- Lifecycle state
- Captured output buffer
- Termination and forced-termination handling

`ServerClient` owns:

- `GET /api/storage`
- Response decoding
- Explicit timeout and error mapping

Keep process launching separate from the SwiftUI view so it can be unit-tested.

## Error Handling

- Missing repository markers: block controls and display setup error.
- Missing `.venv/bin/uvicorn`: fail server startup with setup guidance.
- Port `8000` already occupied: show startup failure without stopping the
  existing process.
- Missing serial selection: disable monitor start.
- Serial monitor exits: preserve output and display exit status.
- Serial device disconnects: stop the monitor child and show disconnect error.
- Storage endpoint unavailable: show unavailable server information while
  preserving process state and logs.

## Testing

### Automated Swift Tests

- Repository-root resolution finds expected marker directories.
- Repository-root resolution reports a useful error when markers are missing.
- Managed process captures standard output and standard error.
- Managed process reports exit status.
- Managed process stops only its own child.
- Log buffers remain bounded.
- Server client decodes valid storage information.
- Server client surfaces connection and malformed-response errors.

### Manual macOS Checks

- `swift build` succeeds.
- `swift run` opens a window without an Xcode project.
- Starting the server launches Uvicorn and updates storage information.
- Opening the web UI launches the system browser.
- Opening the images folder reveals `data/images` in Finder.
- Starting the monitor with a connected TimerCam streams serial output.
- Stopping the monitor exits cleanly.
- Logs remain hidden until requested.
- Clear and auto-scroll controls behave correctly.
- Starting the server while port `8000` is already occupied reports an error
  and leaves the existing process untouched.
- Closing the app terminates only app-owned children.

## Documentation

Update:

- `README.md`
- `docs/development.md`
- `docs/README.md`

Document Swift package build and run commands, supported controls, and the
app-owned-process limitation.
