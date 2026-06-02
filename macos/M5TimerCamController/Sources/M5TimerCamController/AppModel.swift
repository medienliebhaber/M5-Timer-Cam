import Foundation
import Combine
import AppKit

enum LogTab: String, CaseIterable, Identifiable {
    case server = "Server"
    case monitor = "Serial Monitor"
    var id: String { rawValue }
}

enum RepositoryError: LocalizedError {
    case notFound

    var errorDescription: String? {
        "Repository root not found. Expected: scripts/monitor.sh, server/app/main.py, data/"
    }
}

@MainActor
final class AppModel: ObservableObject {
    let serverProcess = ManagedProcess(label: "Server")
    let monitorProcess = ManagedProcess(label: "Serial Monitor")

    @Published private(set) var repoRoot: URL?
    @Published private(set) var setupError: String?
    @Published private(set) var storageInfo: StorageInfo?
    @Published private(set) var discoveredPorts: [String] = []
    @Published var selectedPort: String?
    @Published var activeLogTab: LogTab = .server
    @Published var showLogs = false
    @Published var autoScroll = true

    private let serverClient = ServerClient()
    private var cancellables = Set<AnyCancellable>()
    private var pollTask: Task<Void, Never>?
    private var portWatchTask: Task<Void, Never>?

    init() {
        serverProcess.objectWillChange
            .sink { [weak self] in self?.objectWillChange.send() }
            .store(in: &cancellables)
        monitorProcess.objectWillChange
            .sink { [weak self] in self?.objectWillChange.send() }
            .store(in: &cancellables)

        do {
            repoRoot = try Self.resolveRepositoryRoot()
        } catch {
            setupError = error.localizedDescription
        }
        refreshPorts()
    }

    // MARK: - Repository root

    static func resolveRepositoryRoot(startingFrom startDir: URL? = nil) throws -> URL {
        let markers = ["scripts/monitor.sh", "server/app/main.py", "data"]
        let fm = FileManager.default

        func hasMarkers(_ url: URL) -> Bool {
            markers.allSatisfy { fm.fileExists(atPath: url.appendingPathComponent($0).path) }
        }

        func walkUp(from start: URL) -> URL? {
            var dir = start
            while dir.path != "/" {
                if hasMarkers(dir) { return dir }
                dir = dir.deletingLastPathComponent()
            }
            return nil
        }

        let cwd = startDir ?? URL(fileURLWithPath: fm.currentDirectoryPath)
        if hasMarkers(cwd) { return cwd }
        if let found = walkUp(from: cwd.deletingLastPathComponent()) { return found }

        let exe = URL(fileURLWithPath: ProcessInfo.processInfo.arguments[0])
        if let found = walkUp(from: exe.deletingLastPathComponent()) { return found }

        throw RepositoryError.notFound
    }

    // MARK: - Server

    func startServer() {
        guard let root = repoRoot else { return }
        let uvicorn = root.appendingPathComponent("server/.venv/bin/uvicorn").path
        guard FileManager.default.fileExists(atPath: uvicorn) else {
            serverProcess.state = .failed(
                "server/.venv/bin/uvicorn not found — run: cd server && python -m venv .venv && .venv/bin/pip install -r requirements.txt"
            )
            return
        }
        serverProcess.launch(
            executable: uvicorn,
            arguments: ["app.main:app", "--host", "0.0.0.0", "--port", "8000"],
            workingDirectory: root.appendingPathComponent("server")
        )
        startPolling()
    }

    func stopServer() {
        pollTask?.cancel()
        pollTask = nil
        storageInfo = nil
        serverProcess.stop()
    }

    func restartServer() {
        stopServer()
        Task {
            try? await Task.sleep(for: .milliseconds(800))
            startServer()
        }
    }

    func openWebUI() {
        NSWorkspace.shared.open(URL(string: "http://localhost:8000")!)
    }

    func openImagesFolder() {
        guard let root = repoRoot else { return }
        let imagesURL = root.appendingPathComponent("data/images")
        try? FileManager.default.createDirectory(at: imagesURL, withIntermediateDirectories: true)
        NSWorkspace.shared.open(imagesURL)
    }

    private func startPolling() {
        pollTask?.cancel()
        pollTask = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(for: .seconds(5))
                guard let self, case .running = self.serverProcess.state else { continue }
                do {
                    let info = try await self.serverClient.fetchStorage()
                    self.storageInfo = info
                } catch {
                    self.storageInfo = nil
                }
            }
        }
    }

    // MARK: - Serial monitor

    func refreshPorts() {
        guard let entries = try? FileManager.default.contentsOfDirectory(atPath: "/dev") else { return }
        let ports = entries
            .filter { $0.hasPrefix("cu.") }
            .map { "/dev/\($0)" }
            .sorted { a, b in
                let aUsb = a.contains("usbserial") || a.contains("SLAB") || a.contains("CP210") || a.contains("wch")
                let bUsb = b.contains("usbserial") || b.contains("SLAB") || b.contains("CP210") || b.contains("wch")
                if aUsb != bUsb { return aUsb }
                return a < b
            }
        discoveredPorts = ports
        if selectedPort == nil || !ports.contains(selectedPort!) {
            selectedPort = ports.first
        }
    }

    func startMonitor() {
        guard let root = repoRoot, let port = selectedPort else { return }
        monitorProcess.launch(
            executable: "/bin/bash",
            arguments: [root.appendingPathComponent("scripts/monitor.sh").path, "-p", port],
            workingDirectory: root
        )
        watchPort(port)
    }

    func stopMonitor() {
        portWatchTask?.cancel()
        portWatchTask = nil
        monitorProcess.stop()
    }

    func portLabel(_ port: String) -> String {
        let isLikely = port.contains("usbserial") || port.contains("SLAB") || port.contains("CP210") || port.contains("wch")
        return isLikely ? port : "\(port) (unlikely)"
    }

    private func watchPort(_ port: String) {
        portWatchTask?.cancel()
        portWatchTask = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(for: .seconds(2))
                guard !FileManager.default.fileExists(atPath: port) else { continue }
                guard let self else { break }
                self.portWatchTask = nil
                self.monitorProcess.stop()
                self.monitorProcess.state = .failed("Serial device disconnected: \(port)")
                break
            }
        }
    }
}
