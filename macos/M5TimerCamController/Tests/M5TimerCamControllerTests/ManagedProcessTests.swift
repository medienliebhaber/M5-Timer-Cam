@testable import M5TimerCamController
import XCTest
import Foundation

@MainActor
final class ManagedProcessTests: XCTestCase {

    func testCapturesStdout() async throws {
        let proc = ManagedProcess(label: "test")
        proc.launch(
            executable: "/bin/echo",
            arguments: ["hello stdout"],
            workingDirectory: URL(fileURLWithPath: "/tmp")
        )
        try await Task.sleep(for: .milliseconds(500))
        XCTAssertTrue(proc.log.contains("hello stdout"), "Log was: \(proc.log)")
    }

    func testCapturesStderr() async throws {
        let proc = ManagedProcess(label: "test")
        proc.launch(
            executable: "/bin/bash",
            arguments: ["-c", "echo error_output >&2"],
            workingDirectory: URL(fileURLWithPath: "/tmp")
        )
        try await Task.sleep(for: .milliseconds(500))
        XCTAssertTrue(proc.log.contains("error_output"), "Log was: \(proc.log)")
    }

    func testReportsNonZeroExitAsFailure() async throws {
        let proc = ManagedProcess(label: "test")
        proc.launch(
            executable: "/bin/bash",
            arguments: ["-c", "exit 42"],
            workingDirectory: URL(fileURLWithPath: "/tmp")
        )
        try await Task.sleep(for: .milliseconds(500))
        guard case .failed(let msg) = proc.state else {
            return XCTFail("Expected .failed, got \(proc.state)")
        }
        XCTAssertTrue(msg.contains("42"), "Error message: \(msg)")
    }

    func testStopSetsStoppedState() async throws {
        let proc = ManagedProcess(label: "test")
        proc.launch(
            executable: "/bin/sleep",
            arguments: ["30"],
            workingDirectory: URL(fileURLWithPath: "/tmp")
        )
        try await Task.sleep(for: .milliseconds(100))
        XCTAssertEqual(proc.state, .running)

        proc.stop(grace: 0.5)
        try await Task.sleep(for: .seconds(1))
        XCTAssertEqual(proc.state, .stopped)
    }

    func testStopDoesNotAffectExternalProcess() async throws {
        let external = Process()
        external.executableURL = URL(fileURLWithPath: "/bin/sleep")
        external.arguments = ["30"]
        try external.run()
        defer { external.terminate() }

        let proc = ManagedProcess(label: "test")
        proc.launch(
            executable: "/bin/sleep",
            arguments: ["30"],
            workingDirectory: URL(fileURLWithPath: "/tmp")
        )
        try await Task.sleep(for: .milliseconds(100))

        proc.stop()
        try await Task.sleep(for: .seconds(1))

        XCTAssertTrue(external.isRunning, "External process was unexpectedly terminated")
    }

    func testLogBufferRemainsWithinBound() async throws {
        let cap = 200
        let proc = ManagedProcess(label: "test", maxLogCharacters: cap)
        // Output more than `cap` characters
        let longString = String(repeating: "x", count: 500)
        proc.launch(
            executable: "/bin/echo",
            arguments: [longString],
            workingDirectory: URL(fileURLWithPath: "/tmp")
        )
        try await Task.sleep(for: .milliseconds(500))
        XCTAssertLessThanOrEqual(proc.log.count, cap + 2, "Log exceeded bound: \(proc.log.count)")
    }

    func testClearLog() async throws {
        let proc = ManagedProcess(label: "test")
        proc.launch(
            executable: "/bin/echo",
            arguments: ["something"],
            workingDirectory: URL(fileURLWithPath: "/tmp")
        )
        try await Task.sleep(for: .milliseconds(300))
        XCTAssertFalse(proc.log.isEmpty)
        proc.clearLog()
        XCTAssertTrue(proc.log.isEmpty)
    }
}

@MainActor
final class RepositoryRootTests: XCTestCase {

    func testResolvesValidRoot() throws {
        let fm = FileManager.default
        let tmp = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? fm.removeItem(at: tmp) }

        try fm.createDirectory(at: tmp.appendingPathComponent("scripts"), withIntermediateDirectories: true)
        fm.createFile(atPath: tmp.appendingPathComponent("scripts/monitor.sh").path, contents: nil)

        try fm.createDirectory(at: tmp.appendingPathComponent("server/app"), withIntermediateDirectories: true)
        fm.createFile(atPath: tmp.appendingPathComponent("server/app/main.py").path, contents: nil)

        try fm.createDirectory(at: tmp.appendingPathComponent("data"), withIntermediateDirectories: true)

        let resolved = try AppModel.resolveRepositoryRoot(startingFrom: tmp)
        XCTAssertEqual(resolved.standardized, tmp.standardized)
    }

    func testResolvesFromSubdirectory() throws {
        let fm = FileManager.default
        let tmp = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        defer { try? fm.removeItem(at: tmp) }

        try fm.createDirectory(at: tmp.appendingPathComponent("scripts"), withIntermediateDirectories: true)
        fm.createFile(atPath: tmp.appendingPathComponent("scripts/monitor.sh").path, contents: nil)
        try fm.createDirectory(at: tmp.appendingPathComponent("server/app"), withIntermediateDirectories: true)
        fm.createFile(atPath: tmp.appendingPathComponent("server/app/main.py").path, contents: nil)
        try fm.createDirectory(at: tmp.appendingPathComponent("data"), withIntermediateDirectories: true)

        let subdir = tmp.appendingPathComponent("server/app")
        let resolved = try AppModel.resolveRepositoryRoot(startingFrom: subdir)
        XCTAssertEqual(resolved.standardized, tmp.standardized)
    }

    func testThrowsWhenMarkersAbsent() {
        let empty = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        XCTAssertThrowsError(try AppModel.resolveRepositoryRoot(startingFrom: empty))
    }
}
