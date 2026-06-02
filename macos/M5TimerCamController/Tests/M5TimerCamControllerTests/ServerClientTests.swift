@testable import M5TimerCamController
import XCTest
import Foundation

final class ServerClientTests: XCTestCase {

    func testDecodesValidStorageInfo() throws {
        let json = """
        {"frame_count": 42, "disk_used_bytes": 1048576, "disk_used_mb": 1.0}
        """.data(using: .utf8)!

        let info = try JSONDecoder().decode(StorageInfo.self, from: json)
        XCTAssertEqual(info.frameCount, 42)
        XCTAssertEqual(info.diskUsedBytes, 1_048_576)
        XCTAssertEqual(info.diskUsedMb, 1.0, accuracy: 0.001)
    }

    func testDecodesZeroFrames() throws {
        let json = """
        {"frame_count": 0, "disk_used_bytes": 0, "disk_used_mb": 0.0}
        """.data(using: .utf8)!

        let info = try JSONDecoder().decode(StorageInfo.self, from: json)
        XCTAssertEqual(info.frameCount, 0)
        XCTAssertEqual(info.diskUsedBytes, 0)
    }

    func testSurfacesMalformedResponse() {
        let badJSON = """
        {"wrong_key": "wrong_value"}
        """.data(using: .utf8)!

        XCTAssertThrowsError(try JSONDecoder().decode(StorageInfo.self, from: badJSON))
    }

    func testSurfacesConnectionError() async {
        let client = ServerClient(
            baseURL: URL(string: "http://localhost:19999")!,
            timeout: 0.5
        )
        do {
            _ = try await client.fetchStorage()
            XCTFail("Expected a network error")
        } catch let e as ServerClientError {
            if case .network = e {
                // expected
            } else {
                XCTFail("Expected .network error, got \(e)")
            }
        } catch {
            // Also acceptable — connection refused maps to network error
        }
    }
}
