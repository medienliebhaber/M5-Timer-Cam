import Foundation

enum ProcessState: Equatable {
    case stopped
    case starting
    case running
    case failed(String)

    var label: String {
        switch self {
        case .stopped: return "Stopped"
        case .starting: return "Starting"
        case .running: return "Running"
        case .failed: return "Failed"
        }
    }

    var isActive: Bool {
        switch self {
        case .running, .starting: return true
        default: return false
        }
    }

    var errorMessage: String? {
        if case .failed(let msg) = self { return msg }
        return nil
    }
}

@MainActor
final class ManagedProcess: ObservableObject {
    @Published var state: ProcessState = .stopped
    @Published private(set) var log: String = ""

    let label: String
    let maxLogCharacters: Int

    private var process: Process?
    private var stopping = false

    init(label: String, maxLogCharacters: Int = 200_000) {
        self.label = label
        self.maxLogCharacters = maxLogCharacters
    }

    var isRunning: Bool { process?.isRunning == true }

    func launch(executable: String, arguments: [String], workingDirectory: URL) {
        guard process == nil else { return }

        let p = Process()
        p.executableURL = URL(fileURLWithPath: executable)
        p.arguments = arguments
        p.currentDirectoryURL = workingDirectory
        p.environment = ProcessInfo.processInfo.environment

        let outPipe = Pipe()
        let errPipe = Pipe()
        p.standardOutput = outPipe
        p.standardError = errPipe

        outPipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            if data.isEmpty { handle.readabilityHandler = nil; return }
            guard let text = String(data: data, encoding: .utf8) else { return }
            Task { @MainActor [weak self] in self?.appendLog(text) }
        }

        errPipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            if data.isEmpty { handle.readabilityHandler = nil; return }
            guard let text = String(data: data, encoding: .utf8) else { return }
            Task { @MainActor [weak self] in self?.appendLog(text) }
        }

        p.terminationHandler = { [weak self] proc in
            Task { @MainActor [weak self] in
                guard let self else { return }
                self.process = nil
                let wasStopping = self.stopping
                self.stopping = false
                if wasStopping {
                    self.state = .stopped
                    return
                }
                switch proc.terminationReason {
                case .exit:
                    self.state = proc.terminationStatus == 0 ? .stopped : .failed("Exit \(proc.terminationStatus)")
                case .uncaughtSignal:
                    let sig = proc.terminationStatus
                    self.state = (sig == SIGTERM || sig == SIGKILL) ? .stopped : .failed("Signal \(sig)")
                @unknown default:
                    self.state = .stopped
                }
            }
        }

        state = .starting
        do {
            try p.run()
            process = p
            state = .running
        } catch {
            process = nil
            state = .failed(error.localizedDescription)
        }
    }

    func stop(grace: TimeInterval = 3.0) {
        guard let p = process, p.isRunning else {
            process = nil
            state = .stopped
            return
        }
        stopping = true
        p.terminate()
        let pid = p.processIdentifier
        Task.detached {
            try? await Task.sleep(for: .seconds(grace))
            if p.isRunning { kill(pid, SIGKILL) }
        }
    }

    func clearLog() { log = "" }

    private func appendLog(_ text: String) {
        log.append(text)
        if log.count > maxLogCharacters {
            log = String(log.dropFirst(log.count - maxLogCharacters))
        }
    }
}
