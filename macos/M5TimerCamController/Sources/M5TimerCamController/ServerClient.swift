import Foundation

struct StorageInfo: Decodable {
    let frameCount: Int
    let diskUsedBytes: Int
    let diskUsedMb: Double

    enum CodingKeys: String, CodingKey {
        case frameCount = "frame_count"
        case diskUsedBytes = "disk_used_bytes"
        case diskUsedMb = "disk_used_mb"
    }
}

enum ServerClientError: LocalizedError {
    case network(Error)
    case http(Int)
    case decoding(Error)

    var errorDescription: String? {
        switch self {
        case .network(let e): return "Network: \(e.localizedDescription)"
        case .http(let code): return "HTTP \(code)"
        case .decoding(let e): return "Decode: \(e.localizedDescription)"
        }
    }
}

final class ServerClient {
    private let baseURL: URL
    private let session: URLSession

    init(baseURL: URL = URL(string: "http://localhost:8000")!, timeout: TimeInterval = 5) {
        self.baseURL = baseURL
        let config = URLSessionConfiguration.ephemeral
        config.timeoutIntervalForRequest = timeout
        self.session = URLSession(configuration: config)
    }

    func fetchStorage() async throws -> StorageInfo {
        let url = baseURL.appendingPathComponent("api/storage")
        do {
            let (data, response) = try await session.data(from: url)
            guard let http = response as? HTTPURLResponse else {
                throw ServerClientError.http(-1)
            }
            guard http.statusCode == 200 else {
                throw ServerClientError.http(http.statusCode)
            }
            do {
                return try JSONDecoder().decode(StorageInfo.self, from: data)
            } catch {
                throw ServerClientError.decoding(error)
            }
        } catch let e as ServerClientError {
            throw e
        } catch {
            throw ServerClientError.network(error)
        }
    }
}
