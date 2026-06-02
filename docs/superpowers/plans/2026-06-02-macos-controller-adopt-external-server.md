# macOS Controller External Server Adoption Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detect a compatible same-user TimerCam server already listening on port `8000`, adopt it for explicit stop and restart actions, and leave adopted servers running when the controller quits.

**Architecture:** Add a focused `ExternalServerController` that runs `/usr/sbin/lsof`, parses field output, validates same-user PIDs, and signals adopted listeners. Keep HTTP compatibility detection in `AppModel`: probe `GET /api/storage` from app startup onward, track connection ownership separately from the child-process lifecycle, and route actions according to that state.

**Tech Stack:** Swift 5.9 package, SwiftUI, Foundation `Process`, Darwin signals, `/usr/sbin/lsof`, XCTest

---

## Baseline Toolchain Note

On the current workstation, `cd macos/M5TimerCamController && swift build`
passes. `swift test` cannot compile the pre-existing test target because the
active `/Library/Developer/CommandLineTools` toolchain does not expose an
importable `XCTest` module:

```text
error: no such module 'XCTest'
```

Full Xcode is not installed. Keep the XCTest regression tests in this plan.
Run `swift test` whenever a full Xcode toolchain is available. During execution
on the current workstation, use `swift build` plus the manual checks in Task 6
and report the XCTest environment limitation explicitly.

## File Structure

- Create: `macos/M5TimerCamController/Sources/M5TimerCamController/ExternalServerController.swift`
  - Owns `lsof` execution, field-output parsing, same-user PID validation, and
    explicit adopted-process termination.
- Create: `macos/M5TimerCamController/Tests/M5TimerCamControllerTests/ExternalServerControllerTests.swift`
  - Covers parser safety rules without launching SwiftUI.
- Modify: `macos/M5TimerCamController/Sources/M5TimerCamController/ServerClient.swift`
  - Adds a protocol so `AppModel` probing can be isolated in tests.
- Modify: `macos/M5TimerCamController/Sources/M5TimerCamController/AppModel.swift`
  - Tracks server connection ownership, polls from startup, and routes server
    actions.
- Modify: `macos/M5TimerCamController/Sources/M5TimerCamController/ContentView.swift`
  - Shows adopted and read-only states and uses ownership-aware button
    enablement.
- Modify: `macos/M5TimerCamController/Sources/M5TimerCamController/M5TimerCamControllerApp.swift`
  - Calls cleanup on application termination.
- Create: `macos/M5TimerCamController/Tests/M5TimerCamControllerTests/AppModelServerConnectionTests.swift`
  - Covers reconciliation, capabilities, adopted stop, and shutdown behavior.

### Task 1: Parse Safely Adoptable Listener PIDs

**Files:**
- Create: `macos/M5TimerCamController/Sources/M5TimerCamController/ExternalServerController.swift`
- Create: `macos/M5TimerCamController/Tests/M5TimerCamControllerTests/ExternalServerControllerTests.swift`

- [ ] **Step 1: Write the failing parser tests**

Create `ExternalServerControllerTests.swift`:

```swift
@testable import M5TimerCamController
import XCTest

final class ExternalServerControllerTests: XCTestCase {
    func testParsesOneSameUserListener() {
        let result = ExternalServerController.parseLsofOutput(
            "p55460\ncPython\nu501\nLmaskow\nf10\nn*:8000\n",
            currentUID: 501,
            controllerPID: 999
        )
        XCTAssertEqual(result, .adoptable(pid: 55460))
    }

    func testRejectsDifferentUserListener() {
        let result = ExternalServerController.parseLsofOutput(
            "p55460\ncPython\nu502\nLother\nf10\nn*:8000\n",
            currentUID: 501,
            controllerPID: 999
        )
        XCTAssertEqual(result, .readOnly("Port 8000 listener belongs to another user."))
    }

    func testRejectsAmbiguousListeners() {
        let result = ExternalServerController.parseLsofOutput(
            "p10\nu501\nn*:8000\np11\nu501\nn*:8000\n",
            currentUID: 501,
            controllerPID: 999
        )
        XCTAssertEqual(result, .readOnly("Expected one port 8000 listener, found 2."))
    }

    func testRejectsControllerPID() {
        let result = ExternalServerController.parseLsofOutput(
            "p999\nu501\nn*:8000\n",
            currentUID: 501,
            controllerPID: 999
        )
        XCTAssertEqual(result, .readOnly("Refusing to adopt the controller process."))
    }
}
```

- [ ] **Step 2: Run the parser tests and confirm the expected failure**

Run:

```bash
cd macos/M5TimerCamController
swift test --filter ExternalServerControllerTests
```

Expected with full Xcode: FAIL because `ExternalServerController` is not
defined. Expected on the current workstation: compilation remains blocked by
the baseline missing-`XCTest` error before the new failure is reached.

- [ ] **Step 3: Add the parser and discovery boundary**

Create `ExternalServerController.swift`:

```swift
import Foundation
import Darwin

enum ListenerDiscoveryResult: Equatable {
    case adoptable(pid: Int32)
    case readOnly(String)
}

enum ExternalServerError: LocalizedError {
    case unsafePID(String)
    case signalFailed(Int32)

    var errorDescription: String? {
        switch self {
        case .unsafePID(let reason): return reason
        case .signalFailed(let pid): return "Could not stop server process \(pid)."
        }
    }
}

@MainActor
protocol ExternalServerControlling {
    func discoverListener() -> ListenerDiscoveryResult
    func stop(pid: Int32, grace: Duration) async throws
}

@MainActor
struct ExternalServerController: ExternalServerControlling {
    static func parseLsofOutput(
        _ output: String,
        currentUID: uid_t = getuid(),
        controllerPID: Int32 = getpid()
    ) -> ListenerDiscoveryResult {
        var listeners: [(pid: Int32, uid: uid_t?)] = []
        var currentPID: Int32?
        var currentUIDValue: uid_t?

        func appendCurrent() {
            guard let currentPID else { return }
            listeners.append((currentPID, currentUIDValue))
        }

        for line in output.split(separator: "\n").map(String.init) {
            guard let field = line.first else { continue }
            let value = String(line.dropFirst())
            switch field {
            case "p":
                appendCurrent()
                currentPID = Int32(value)
                currentUIDValue = nil
            case "u":
                currentUIDValue = uid_t(value)
            default:
                break
            }
        }
        appendCurrent()

        guard listeners.count == 1 else {
            return .readOnly("Expected one port 8000 listener, found \(listeners.count).")
        }
        let listener = listeners[0]
        guard listener.pid > 0 else {
            return .readOnly("Port 8000 listener PID is invalid.")
        }
        guard listener.pid != controllerPID else {
            return .readOnly("Refusing to adopt the controller process.")
        }
        guard listener.uid == currentUID else {
            return .readOnly("Port 8000 listener belongs to another user.")
        }
        return .adoptable(pid: listener.pid)
    }

    func discoverListener() -> ListenerDiscoveryResult {
        let process = Process()
        let pipe = Pipe()
        process.executableURL = URL(fileURLWithPath: "/usr/sbin/lsof")
        process.arguments = ["-nP", "-a", "-iTCP:8000", "-sTCP:LISTEN", "-FpcuLn"]
        process.standardOutput = pipe
        process.standardError = Pipe()
        do {
            try process.run()
            process.waitUntilExit()
            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            return Self.parseLsofOutput(String(decoding: data, as: UTF8.self))
        } catch {
            return .readOnly("Could not inspect the port 8000 listener.")
        }
    }

    func stop(pid: Int32, grace: Duration = .seconds(3)) async throws {
        guard pid > 0, pid != getpid() else {
            throw ExternalServerError.unsafePID("Refusing to signal an unsafe PID.")
        }
        guard case .adoptable(let currentPID) = discoverListener(), currentPID == pid else {
            throw ExternalServerError.unsafePID("Port 8000 listener changed before stop.")
        }
        guard kill(pid, SIGTERM) == 0 else { throw ExternalServerError.signalFailed(pid) }
        try? await Task.sleep(for: grace)
        guard kill(pid, 0) == 0 else { return }
        guard case .adoptable(let currentPID) = discoverListener(), currentPID == pid else { return }
        guard kill(pid, SIGKILL) == 0 else { throw ExternalServerError.signalFailed(pid) }
    }
}
```

- [ ] **Step 4: Run the parser tests and build**

Run:

```bash
cd macos/M5TimerCamController
swift test --filter ExternalServerControllerTests
swift build
```

Expected with full Xcode: parser tests PASS and build PASS. Expected on the
current workstation: record the missing-`XCTest` blocker and confirm build
PASS.

- [ ] **Step 5: Commit the listener controller**

```bash
git add macos/M5TimerCamController/Sources/M5TimerCamController/ExternalServerController.swift \
  macos/M5TimerCamController/Tests/M5TimerCamControllerTests/ExternalServerControllerTests.swift
git commit -m "feat: discover adoptable TimerCam server listeners"
```

### Task 2: Track HTTP Compatibility Separately From Child Ownership

**Files:**
- Modify: `macos/M5TimerCamController/Sources/M5TimerCamController/ServerClient.swift`
- Modify: `macos/M5TimerCamController/Sources/M5TimerCamController/AppModel.swift`
- Create: `macos/M5TimerCamController/Tests/M5TimerCamControllerTests/AppModelServerConnectionTests.swift`

- [ ] **Step 1: Write failing state-reconciliation tests**

Create `AppModelServerConnectionTests.swift`:

```swift
@testable import M5TimerCamController
import XCTest

@MainActor
final class AppModelServerConnectionTests: XCTestCase {
    private let info = StorageInfo(frameCount: 3, diskUsedBytes: 100, diskUsedMb: 0.1)

    func testSuccessfulProbeAdoptsSafeExternalListener() {
        let model = AppModel(startBackgroundTasks: false)
        model.reconcileSuccessfulProbe(info, listener: .adoptable(pid: 42))
        XCTAssertEqual(model.serverConnection, .adopted(pid: 42))
        XCTAssertEqual(model.serverStatusLabel, "Running (Adopted)")
        XCTAssertEqual(model.serverStatusState, .running)
        XCTAssertTrue(model.canStopServer)
        XCTAssertTrue(model.canRestartServer)
    }

    func testSuccessfulProbeKeepsUnsafeListenerReadOnly() {
        let model = AppModel(startBackgroundTasks: false)
        model.reconcileSuccessfulProbe(info, listener: .readOnly("unsafe"))
        XCTAssertEqual(model.serverConnection, .readOnly("unsafe"))
        XCTAssertEqual(model.serverStatusLabel, "Connected (Read Only)")
        XCTAssertFalse(model.canStopServer)
        XCTAssertFalse(model.canRestartServer)
    }
}
```

- [ ] **Step 2: Run the new tests and confirm the expected failure**

Run:

```bash
cd macos/M5TimerCamController
swift test --filter AppModelServerConnectionTests
```

Expected with full Xcode: FAIL because the new state API is absent. On the
current workstation, record the baseline `XCTest` blocker.

- [ ] **Step 3: Add the fetch protocol and connection state**

In `ServerClient.swift`, add above `final class ServerClient` and declare
conformance:

```swift
protocol ServerFetching {
    func fetchStorage() async throws -> StorageInfo
}

final class ServerClient: ServerFetching {
```

In `AppModel.swift`, add:

```swift
enum ServerConnectionState: Equatable {
    case unavailable
    case appOwned
    case adopted(pid: Int32)
    case readOnly(String)
}
```

Add these stored properties to `AppModel`:

```swift
@Published private(set) var serverConnection: ServerConnectionState = .unavailable
@Published private(set) var serverActionError: String?

private let serverClient: ServerFetching
private let externalServerController: ExternalServerControlling
```

Change initialization to inject dependencies and start polling immediately:

```swift
init(
    serverClient: ServerFetching = ServerClient(),
    externalServerController: ExternalServerControlling = ExternalServerController(),
    startBackgroundTasks: Bool = true
) {
    self.serverClient = serverClient
    self.externalServerController = externalServerController
    // Keep the existing objectWillChange subscriptions, root resolution,
    // and refreshPorts() call here.
    if startBackgroundTasks { startPolling() }
}
```

Replace the existing polling body with an immediate probe followed by a
five-second interval:

```swift
private func startPolling() {
    pollTask?.cancel()
    pollTask = Task { [weak self] in
        while !Task.isCancelled {
            await self?.probeServer()
            try? await Task.sleep(for: .seconds(5))
        }
    }
}

func probeServer() async {
    do {
        let info = try await serverClient.fetchStorage()
        reconcileSuccessfulProbe(info, listener: externalServerController.discoverListener())
    } catch {
        storageInfo = nil
        if !serverProcess.isRunning { serverConnection = .unavailable }
    }
}

func reconcileSuccessfulProbe(_ info: StorageInfo, listener: ListenerDiscoveryResult) {
    storageInfo = info
    if serverProcess.isRunning {
        serverConnection = .appOwned
        return
    }
    switch listener {
    case .adoptable(let pid): serverConnection = .adopted(pid: pid)
    case .readOnly(let reason): serverConnection = .readOnly(reason)
    }
}
```

Add computed presentation and capability properties:

```swift
var serverStatusLabel: String {
    switch serverConnection {
    case .appOwned: return "Running"
    case .adopted: return "Running (Adopted)"
    case .readOnly: return "Connected (Read Only)"
    case .unavailable: return serverProcess.state.label
    }
}

var serverStatusState: ProcessState {
    switch serverConnection {
    case .appOwned, .adopted, .readOnly: return .running
    case .unavailable: return serverProcess.state
    }
}

var canStartServer: Bool {
    serverConnection == .unavailable && !serverProcess.state.isActive
}

var canStopServer: Bool {
    if case .appOwned = serverConnection { return true }
    if case .adopted = serverConnection { return true }
    return false
}

var canRestartServer: Bool { canStopServer }
```

Remove the call to `startPolling()` from `startServer()`. Polling now starts
from initialization.

- [ ] **Step 4: Run state tests and build**

Run:

```bash
cd macos/M5TimerCamController
swift test --filter AppModelServerConnectionTests
swift build
```

Expected with full Xcode: tests PASS and build PASS. On the current
workstation: record the `XCTest` blocker and confirm build PASS.

- [ ] **Step 5: Commit state reconciliation**

```bash
git add macos/M5TimerCamController/Sources/M5TimerCamController/ServerClient.swift \
  macos/M5TimerCamController/Sources/M5TimerCamController/AppModel.swift \
  macos/M5TimerCamController/Tests/M5TimerCamControllerTests/AppModelServerConnectionTests.swift
git commit -m "feat: detect external TimerCam server connections"
```

### Task 3: Stop And Restart Adopted Servers Explicitly

**Files:**
- Modify: `macos/M5TimerCamController/Sources/M5TimerCamController/AppModel.swift`
- Modify: `macos/M5TimerCamController/Tests/M5TimerCamControllerTests/AppModelServerConnectionTests.swift`

- [ ] **Step 1: Add a failing adopted-stop test**

Append to `AppModelServerConnectionTests.swift`:

```swift
@MainActor
private final class ExternalServerControllerSpy: ExternalServerControlling {
    var stoppedPIDs: [Int32] = []
    var discovery: ListenerDiscoveryResult = .readOnly("unused")

    func discoverListener() -> ListenerDiscoveryResult { discovery }

    func stop(pid: Int32, grace: Duration) async throws {
        stoppedPIDs.append(pid)
    }
}

private struct UnavailableServerClient: ServerFetching {
    func fetchStorage() async throws -> StorageInfo {
        throw URLError(.cannotConnectToHost)
    }
}

extension AppModelServerConnectionTests {
    func testStopSignalsAdoptedServer() async {
        let spy = ExternalServerControllerSpy()
        let model = AppModel(
            serverClient: UnavailableServerClient(),
            externalServerController: spy,
            startBackgroundTasks: false
        )
        model.reconcileSuccessfulProbe(info, listener: .adoptable(pid: 42))
        await model.stopServerNow()
        XCTAssertEqual(spy.stoppedPIDs, [42])
    }
}
```

- [ ] **Step 2: Run the adopted-stop test and confirm failure**

Run:

```bash
cd macos/M5TimerCamController
swift test --filter AppModelServerConnectionTests/testStopSignalsAdoptedServer
```

Expected with full Xcode: FAIL because `stopServerNow()` is absent. On the
current workstation, record the baseline `XCTest` blocker.

- [ ] **Step 3: Route stop and restart through ownership-aware async helpers**

Replace the existing `stopServer()` and `restartServer()` methods in
`AppModel.swift`:

```swift
func stopServer() {
    Task { await stopServerNow() }
}

func stopServerNow() async {
    serverActionError = nil
    switch serverConnection {
    case .appOwned:
        serverProcess.stop()
    case .adopted(let pid):
        do {
            try await externalServerController.stop(pid: pid, grace: .seconds(3))
        } catch {
            serverActionError = error.localizedDescription
        }
    case .unavailable, .readOnly:
        return
    }
    await probeServer()
}

func restartServer() {
    Task {
        await stopServerNow()
        guard await waitForServerToStop() else {
            serverActionError = "Port 8000 did not become available before restart."
            return
        }
        startServer()
    }
}

private func waitForServerToStop(timeout: Duration = .seconds(5)) async -> Bool {
    let clock = ContinuousClock()
    let deadline = clock.now.advanced(by: timeout)
    while clock.now < deadline {
        await probeServer()
        if serverConnection == .unavailable { return true }
        try? await Task.sleep(for: .milliseconds(200))
    }
    return false
}
```

In `startServer()`, clear stale action failures before launch:

```swift
serverActionError = nil
```

- [ ] **Step 4: Run connection tests and build**

Run:

```bash
cd macos/M5TimerCamController
swift test --filter AppModelServerConnectionTests
swift build
```

Expected with full Xcode: tests PASS and build PASS. On the current
workstation: record the `XCTest` blocker and confirm build PASS.

- [ ] **Step 5: Commit adopted server actions**

```bash
git add macos/M5TimerCamController/Sources/M5TimerCamController/AppModel.swift \
  macos/M5TimerCamController/Tests/M5TimerCamControllerTests/AppModelServerConnectionTests.swift
git commit -m "feat: stop and restart adopted TimerCam servers"
```

### Task 4: Update The Server Card

**Files:**
- Modify: `macos/M5TimerCamController/Sources/M5TimerCamController/ContentView.swift`

- [ ] **Step 1: Update server status presentation**

Change `StatusPill` to accept a label override:

```swift
struct StatusPill: View {
    let state: ProcessState
    var label: String? = nil

    var body: some View {
        Text(label ?? state.label)
```

In `ServerCard`, replace button state checks and status rendering:

```swift
StatusPill(state: model.serverStatusState, label: model.serverStatusLabel)
Spacer()
Button("Start") { model.startServer() }
    .disabled(!model.canStartServer)
Button("Stop") { model.stopServer() }
    .disabled(!model.canStopServer)
Button { model.restartServer() } label: {
    Image(systemName: "arrow.clockwise")
}
.help("Restart")
.disabled(!model.canRestartServer)
```

Below the existing process error message, display action and read-only errors:

```swift
if let err = model.serverActionError {
    Text(err)
        .font(.caption)
        .foregroundStyle(.red)
        .lineLimit(3)
}

if case .readOnly(let reason) = model.serverConnection {
    Text(reason)
        .font(.caption)
        .foregroundStyle(.secondary)
        .lineLimit(3)
}
```

- [ ] **Step 2: Build the SwiftUI target**

Run:

```bash
cd macos/M5TimerCamController
swift build
```

Expected: PASS.

- [ ] **Step 3: Commit the server-card update**

```bash
git add macos/M5TimerCamController/Sources/M5TimerCamController/ContentView.swift
git commit -m "feat: show adopted server controls in macOS app"
```

### Task 5: Preserve Adopted Servers During App Cleanup

**Files:**
- Modify: `macos/M5TimerCamController/Sources/M5TimerCamController/AppModel.swift`
- Modify: `macos/M5TimerCamController/Sources/M5TimerCamController/M5TimerCamControllerApp.swift`
- Modify: `macos/M5TimerCamController/Tests/M5TimerCamControllerTests/AppModelServerConnectionTests.swift`

- [ ] **Step 1: Write a failing shutdown regression test**

Append to `AppModelServerConnectionTests.swift`:

```swift
extension AppModelServerConnectionTests {
    func testShutdownDoesNotSignalAdoptedServer() {
        let spy = ExternalServerControllerSpy()
        let model = AppModel(externalServerController: spy, startBackgroundTasks: false)
        model.reconcileSuccessfulProbe(info, listener: .adoptable(pid: 42))
        model.shutdown()
        XCTAssertTrue(spy.stoppedPIDs.isEmpty)
    }
}
```

- [ ] **Step 2: Run the shutdown test and confirm failure**

Run:

```bash
cd macos/M5TimerCamController
swift test --filter AppModelServerConnectionTests/testShutdownDoesNotSignalAdoptedServer
```

Expected with full Xcode: FAIL because `shutdown()` is absent. On the current
workstation, record the baseline `XCTest` blocker.

- [ ] **Step 3: Add app-owned-only shutdown cleanup**

Add to `AppModel.swift`:

```swift
func shutdown() {
    pollTask?.cancel()
    pollTask = nil
    portWatchTask?.cancel()
    portWatchTask = nil
    serverProcess.stop()
    monitorProcess.stop()
}
```

Add `import AppKit` to `M5TimerCamControllerApp.swift`, then attach the
termination notification to `ContentView()`:

```swift
ContentView()
    .environmentObject(model)
    .onReceive(NotificationCenter.default.publisher(
        for: NSApplication.willTerminateNotification
    )) { _ in
        model.shutdown()
    }
```

`shutdown()` deliberately calls only the child-process wrappers. It never
calls `externalServerController.stop`, so an adopted server remains alive.

- [ ] **Step 4: Run tests and build**

Run:

```bash
cd macos/M5TimerCamController
swift test
swift build
```

Expected with full Xcode: all tests PASS and build PASS. On the current
workstation: record the `XCTest` blocker and confirm build PASS.

- [ ] **Step 5: Commit lifecycle cleanup**

```bash
git add macos/M5TimerCamController/Sources/M5TimerCamController/AppModel.swift \
  macos/M5TimerCamController/Sources/M5TimerCamController/M5TimerCamControllerApp.swift \
  macos/M5TimerCamController/Tests/M5TimerCamControllerTests/AppModelServerConnectionTests.swift
git commit -m "fix: preserve adopted server during controller shutdown"
```

### Task 6: Verify The Integrated Controller

**Files:**
- Verify only

- [ ] **Step 1: Run whitespace and source verification**

```bash
git diff --check
cd macos/M5TimerCamController
swift build
swift test
```

Expected: `git diff --check` PASS and `swift build` PASS. With full Xcode,
`swift test` PASS. On the current workstation, report the known missing
`XCTest` toolchain blocker.

- [ ] **Step 2: Start an external TimerCam server**

From the repository root:

```bash
cd server
.venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000
```

Expected: Uvicorn listens on port `8000`.

- [ ] **Step 3: Launch the controller and confirm adoption**

In another terminal:

```bash
cd macos/M5TimerCamController
swift run
```

Expected: the Server card changes to `Running (Adopted)`, displays storage
statistics, and enables **Stop** and **Restart**.

- [ ] **Step 4: Confirm explicit adopted restart**

Press **Restart**.

Expected: the Terminal-started Uvicorn process exits, the controller starts a
replacement Uvicorn child, and the Server card returns to `Running`.

- [ ] **Step 5: Confirm adopted server survival on quit**

Start Uvicorn externally again, launch the controller, confirm
`Running (Adopted)`, then quit the controller without pressing **Stop**.

Run:

```bash
/usr/sbin/lsof -nP -a -iTCP:8000 -sTCP:LISTEN
```

Expected: the externally started Uvicorn process still listens on port `8000`.

- [ ] **Step 6: Review the final change set**

```bash
git status --short
git log --oneline -n 8
```

Expected: only intended source and test files are changed, and each completed
task has its focused commit.
