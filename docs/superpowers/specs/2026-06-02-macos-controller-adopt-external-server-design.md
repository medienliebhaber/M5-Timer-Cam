# macOS Controller External Server Adoption

## Summary

Extend the macOS SwiftUI controller so it detects and controls a compatible
TimerCam FastAPI server that was already started outside the app. The existing
controller can launch and stop its own Uvicorn child process, but it only polls
the API after that child launches. An existing server on port `8000` is
therefore invisible.

The controller does not need additional macOS entitlements or a privileged
helper. It can probe the local HTTP API and discover a same-user listener PID
through `/usr/sbin/lsof`.

## Goals

- Detect a compatible TimerCam server already listening on
  `http://localhost:8000`.
- Display storage statistics for both app-launched and externally started
  compatible servers.
- Adopt an external listener only when it serves the expected TimerCam API and
  belongs to the current macOS user.
- Allow an adopted server to be stopped explicitly.
- Allow an adopted server to be restarted as a new app-owned Uvicorn child.
- Preserve the existing rule that app termination cleans up app-owned child
  processes only.

## Non-Goals

- No elevated privileges, privileged helper, or App Sandbox entitlement
  changes.
- No termination of another user's process.
- No termination of an unrelated service that happens to use port `8000`.
- No attempt to adopt serial-monitor processes.
- No remote-host server management.
- No support for custom server ports in this change.

## Server Identity And Listener Safety

The app treats a service as a compatible TimerCam server only when
`GET http://localhost:8000/api/storage` returns a successful response that
decodes as `StorageInfo`.

After confirming API compatibility, the app runs:

```text
/usr/sbin/lsof -nP -a -iTCP:8000 -sTCP:LISTEN -FpcuLn
```

The app parses field output and accepts a listener PID only when:

1. Exactly one listener process is identified for port `8000`.
2. The listener UID matches the current process UID.
3. The PID is positive and is not the controller application's PID.

API compatibility and same-user PID discovery are both required before the
app enables destructive controls for an externally started server.

If the API responds but a safe listener PID cannot be identified, the app
shows a read-only connection state. It continues to display storage
statistics and leaves destructive controls disabled.

## State Model

Add a server-connection state separate from the app-owned `ManagedProcess`
lifecycle:

```text
unavailable
appOwned
adopted(pid)
readOnly(reason)
```

`appOwned` means the existing `ManagedProcess` child is active.

`adopted(pid)` means a compatible external server and a same-user listener PID
were confirmed. Adoption records the PID for explicit server actions but does
not convert that PID into a child `Process`.

`readOnly(reason)` means the API is compatible but the listener cannot be
safely controlled, for example because PID discovery is ambiguous or the UID
does not match.

The Server card derives its status text from the connection state:

- `Stopped` when the API is unavailable and no app-owned child is active.
- `Starting` while an app-owned child is launching.
- `Running` for an app-owned child with a reachable compatible API.
- `Running (Adopted)` for a safely adopted external listener.
- `Connected (Read Only)` for a compatible API without a safely adoptable
  listener.
- `Failed` when an app-owned process fails or a requested action cannot be
  completed.

## Polling And Detection

Start the server probe loop during `AppModel` initialization rather than only
after `startServer()`. Probe immediately and then every five seconds.

Each successful API response updates `storageInfo` and reconciles connection
state:

- Keep `appOwned` while the app-owned child is active.
- Otherwise run listener discovery and set `adopted(pid)` or
  `readOnly(reason)`.

Each failed API response clears `storageInfo` and reconciles connection state:

- Keep the app-owned process lifecycle separate so logs and launch failures
  remain visible.
- Clear an adopted or read-only connection to `unavailable`.

After explicit stop or restart actions, poll more quickly while waiting for
the listener to exit so the UI does not depend on the regular five-second
interval.

## Server Actions

### Start

Enable **Start** only when no compatible server connection is active and no
app-owned child is active. Launch the existing app-owned Uvicorn command.

### Stop

For `appOwned`, call the existing `ManagedProcess.stop()`.

For `adopted(pid)`, send `SIGTERM` to the recorded PID. Wait up to the existing
three-second grace period. If the same PID is still alive, send `SIGKILL`.
Before signaling, validate that the PID remains positive, is not the
controller PID, and still belongs to the current user.

Do not expose **Stop** for `readOnly`.

### Restart

For `appOwned`, stop the child, wait until port `8000` becomes available, and
launch the existing Uvicorn command.

For `adopted(pid)`, explicitly stop the adopted listener, wait until port
`8000` becomes available, and launch the existing Uvicorn command as a new
app-owned child.

Do not expose **Restart** for `readOnly`.

If the port does not become available within a bounded timeout, report an
action failure and do not start another Uvicorn process.

### App Termination

App termination or window closure continues to stop app-owned child processes
only. An adopted external server remains running unless the user explicitly
presses **Stop** or **Restart**.

## Components

Add a small listener-discovery type responsible for:

- Running `/usr/sbin/lsof` with fixed arguments.
- Parsing field-oriented output.
- Returning a safely adoptable PID or a read-only reason.
- Validating a recorded PID before signaling it.

Keep this separate from `AppModel` and `ManagedProcess` so parsing and safety
rules can be tested without launching the SwiftUI app.

Update `AppModel` to:

- Start probing during initialization.
- Track connection state independently from child-process state.
- Route server actions according to connection ownership.
- Preserve `ManagedProcess` for app-owned children and logs.

Update `ContentView` to:

- Display the richer server status.
- Enable or disable server actions using the connection state.
- Show a concise read-only explanation when destructive controls are
  unavailable.

## Error Handling

- Compatible API but no listener PID: show `Connected (Read Only)`.
- Compatible API but multiple listeners: show `Connected (Read Only)`.
- Compatible API but UID mismatch: show `Connected (Read Only)`.
- Adopted PID disappears before stop: reconcile state through an immediate
  probe and treat the stop as complete.
- Stop signal fails: show an action error and preserve the connection until a
  probe proves it unavailable.
- Restart timeout waiting for port release: show an action error and do not
  launch a second server.
- `/usr/sbin/lsof` launch or parsing failure: show `Connected (Read Only)` and
  preserve API information.

## Testing

Add Swift tests for:

- Parsing one same-user `lsof` listener into an adoptable PID.
- Rejecting a UID mismatch.
- Rejecting ambiguous listener output.
- Rejecting invalid or controller PIDs.
- Mapping a successful API probe plus safe listener discovery to
  `Running (Adopted)`.
- Leaving a compatible but unsafe listener read-only.
- Enabling stop and restart for adopted listeners.
- Leaving adopted listeners running during app shutdown cleanup.
- Restart waiting for port release before launching an app-owned child.

Manual checks:

1. Start Uvicorn from Terminal, then launch the app. Confirm
   `Running (Adopted)`, storage statistics, **Stop**, and **Restart**.
2. Stop the adopted server from the app. Confirm the Terminal-started process
   exits.
3. Restart a Terminal-started server from the app. Confirm the replacement is
   app-owned and server information returns.
4. Quit the app while an adopted server is active. Confirm the external server
   remains running.
5. Start an unrelated HTTP service on port `8000`. Confirm the app does not
   enable destructive controls.

