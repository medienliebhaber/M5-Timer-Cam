import SwiftUI

struct ContentView: View {
    @EnvironmentObject var model: AppModel

    var body: some View {
        if model.repoRoot == nil {
            SetupErrorView(message: model.setupError ?? "Unknown error")
        } else {
            MainView()
        }
    }
}

// MARK: - Setup error

private struct SetupErrorView: View {
    let message: String

    var body: some View {
        VStack(spacing: 16) {
            Image(systemName: "exclamationmark.triangle.fill")
                .font(.system(size: 40))
                .foregroundStyle(.orange)
            Text("Repository Not Found")
                .font(.headline)
            Text(message)
                .font(.caption)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
        }
        .padding(32)
        .frame(width: 420, height: 280)
    }
}

// MARK: - Main layout

private struct MainView: View {
    @EnvironmentObject var model: AppModel

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(spacing: 12) {
                    ServerCard()
                    SerialMonitorCard()
                }
                .padding(16)
            }

            Divider()

            LogFooter()
        }
        .frame(minWidth: 420, minHeight: 480)
    }
}

// MARK: - Server card

private struct ServerCard: View {
    @EnvironmentObject var model: AppModel

    var body: some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 8) {
                    StatusPill(state: model.serverProcess.state)
                    Spacer()
                    Button("Start") { model.startServer() }
                        .disabled(model.serverProcess.state.isActive)
                    Button("Stop") { model.stopServer() }
                        .disabled(!model.serverProcess.state.isActive)
                    Button { model.restartServer() } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                    .help("Restart")
                }

                if let info = model.storageInfo {
                    HStack(spacing: 16) {
                        Label("\(info.frameCount) frames", systemImage: "photo.stack")
                        Label(String(format: "%.1f MB", info.diskUsedMb), systemImage: "internaldrive")
                        Spacer()
                    }
                    .font(.caption)
                    .foregroundStyle(.secondary)
                }

                if let err = model.serverProcess.state.errorMessage {
                    Text(err)
                        .font(.caption)
                        .foregroundStyle(.red)
                        .lineLimit(3)
                }

                HStack(spacing: 8) {
                    Button {
                        model.openWebUI()
                    } label: {
                        Label("Web UI", systemImage: "globe")
                    }
                    Button {
                        model.openImagesFolder()
                    } label: {
                        Label("Images Folder", systemImage: "folder")
                    }
                }
                .buttonStyle(.link)
            }
            .padding(4)
        } label: {
            Label("Server", systemImage: "server.rack")
                .font(.headline)
        }
    }
}

// MARK: - Serial monitor card

private struct SerialMonitorCard: View {
    @EnvironmentObject var model: AppModel

    var body: some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 8) {
                    Picker("Port", selection: $model.selectedPort) {
                        Text("None").tag(String?.none)
                        ForEach(model.discoveredPorts, id: \.self) { port in
                            Text(model.portLabel(port)).tag(Optional(port))
                        }
                    }
                    .frame(maxWidth: .infinity)

                    Button {
                        model.refreshPorts()
                    } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                    .help("Refresh Ports")
                }

                HStack(spacing: 8) {
                    StatusPill(state: model.monitorProcess.state)
                    Spacer()
                    Button("Start") { model.startMonitor() }
                        .disabled(model.selectedPort == nil || model.monitorProcess.state.isActive)
                    Button("Stop") { model.stopMonitor() }
                        .disabled(!model.monitorProcess.state.isActive)
                }

                if let err = model.monitorProcess.state.errorMessage {
                    Text(err)
                        .font(.caption)
                        .foregroundStyle(.red)
                        .lineLimit(3)
                }
            }
            .padding(4)
        } label: {
            Label("Serial Monitor", systemImage: "terminal")
                .font(.headline)
        }
    }
}

// MARK: - Status pill

struct StatusPill: View {
    let state: ProcessState

    var body: some View {
        Text(state.label)
            .font(.caption.weight(.semibold))
            .padding(.horizontal, 8)
            .padding(.vertical, 3)
            .background(bg, in: Capsule())
            .foregroundStyle(fg)
    }

    private var bg: Color {
        switch state {
        case .stopped: return Color.secondary.opacity(0.15)
        case .starting: return Color.yellow.opacity(0.25)
        case .running: return Color.green.opacity(0.25)
        case .failed: return Color.red.opacity(0.25)
        }
    }

    private var fg: Color {
        switch state {
        case .stopped: return .secondary
        case .starting: return .yellow
        case .running: return .green
        case .failed: return .red
        }
    }
}

// MARK: - Log footer

private struct LogFooter: View {
    @EnvironmentObject var model: AppModel

    private var activeProcess: ManagedProcess {
        model.activeLogTab == .server ? model.serverProcess : model.monitorProcess
    }

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                Button(model.showLogs ? "Hide Logs" : "Show Logs") {
                    model.showLogs.toggle()
                }
                .buttonStyle(.link)
                .font(.caption)

                Spacer()

                if model.showLogs {
                    Picker("", selection: $model.activeLogTab) {
                        ForEach(LogTab.allCases) { tab in
                            Text(tab.rawValue).tag(tab)
                        }
                    }
                    .pickerStyle(.segmented)
                    .frame(width: 200)

                    Toggle("Auto-scroll", isOn: $model.autoScroll)
                        .toggleStyle(.checkbox)
                        .font(.caption)

                    Button("Clear") { activeProcess.clearLog() }
                        .font(.caption)
                }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 6)

            if model.showLogs {
                Divider()
                LogTextView(log: activeProcess.log, autoScroll: model.autoScroll)
                    .frame(height: 200)
            }
        }
        .background(.bar)
    }
}

// MARK: - Log text view

private struct LogTextView: View {
    let log: String
    let autoScroll: Bool

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                Text(log.isEmpty ? " " : log)
                    .font(.system(.caption, design: .monospaced))
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(8)
                    .textSelection(.enabled)

                Color.clear.frame(height: 1).id("bottom")
            }
            .onChange(of: log) {
                if autoScroll {
                    withAnimation(.easeOut(duration: 0.1)) {
                        proxy.scrollTo("bottom", anchor: .bottom)
                    }
                }
            }
        }
        .background(Color(NSColor.textBackgroundColor))
    }
}
