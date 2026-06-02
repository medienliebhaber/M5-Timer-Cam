import SwiftUI

@main
struct M5TimerCamControllerApp: App {
    @StateObject private var model = AppModel()

    var body: some Scene {
        WindowGroup("M5 Timer Cam Controller") {
            ContentView()
                .environmentObject(model)
        }
        .defaultSize(width: 480, height: 640)
        .commands {
            CommandGroup(replacing: .newItem) {}
        }
    }
}
