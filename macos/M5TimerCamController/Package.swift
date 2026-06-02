// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "M5TimerCamController",
    platforms: [.macOS(.v14)],
    targets: [
        .executableTarget(
            name: "M5TimerCamController",
            path: "Sources/M5TimerCamController"
        ),
        .testTarget(
            name: "M5TimerCamControllerTests",
            dependencies: ["M5TimerCamController"],
            path: "Tests/M5TimerCamControllerTests"
        ),
    ]
)
