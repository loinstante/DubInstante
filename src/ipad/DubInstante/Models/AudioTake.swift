import Foundation

public struct AudioTake: Equatable, Identifiable {
    public let id = UUID()
    public let fileURL: URL
    public let durationMs: Int64
    public let createdAt: Date

    public init(fileURL: URL, durationMs: Int64, createdAt: Date = Date()) {
        self.fileURL = fileURL
        self.durationMs = durationMs
        self.createdAt = createdAt
    }
}
