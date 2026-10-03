import SwiftUI

public struct TransportBarView: View {
    public let isPlaying: Bool
    public let currentPositionMs: Int64
    public let totalDurationMs: Int64
    public let hasVideo: Bool
    public let isRecording: Bool

    public let onJumpStart: () -> Void
    public let onStepBack5s: () -> Void
    public let onTogglePlayPause: () -> Void
    public let onStepForward5s: () -> Void
    public let onSeek: (Int64) -> Void

    public init(
        isPlaying: Bool,
        currentPositionMs: Int64,
        totalDurationMs: Int64,
        hasVideo: Bool,
        isRecording: Bool,
        onJumpStart: @escaping () -> Void,
        onStepBack5s: @escaping () -> Void,
        onTogglePlayPause: @escaping () -> Void,
        onStepForward5s: @escaping () -> Void,
        onSeek: @escaping (Int64) -> Void
    ) {
        self.isPlaying = isPlaying
        self.currentPositionMs = currentPositionMs
        self.totalDurationMs = totalDurationMs
        self.hasVideo = hasVideo
        self.isRecording = isRecording
        self.onJumpStart = onJumpStart
        self.onStepBack5s = onStepBack5s
        self.onTogglePlayPause = onTogglePlayPause
        self.onStepForward5s = onStepForward5s
        self.onSeek = onSeek
    }

    public var body: some View {
        HStack(spacing: 12) {
            // Jump to start
            Button(action: onJumpStart) {
                Image(systemName: "backward.end.fill")
                    .font(.system(size: 16))
            }
            .disabled(!hasVideo || isRecording)

            // Step back 5s
            Button(action: onStepBack5s) {
                Image(systemName: "gobackward.5")
                    .font(.system(size: 18))
            }
            .disabled(!hasVideo || isRecording)

            // Play / Pause Button
            Button(action: onTogglePlayPause) {
                Image(systemName: isPlaying ? "pause.fill" : "play.fill")
                    .font(.system(size: 20))
                    .frame(width: 44, height: 44)
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .clipShape(Circle())
            }
            .disabled(!hasVideo || isRecording)

            // Step forward 5s
            Button(action: onStepForward5s) {
                Image(systemName: "goforward.5")
                    .font(.system(size: 18))
            }
            .disabled(!hasVideo || isRecording)

            // Timecode display
            Text("\(formatTimecode(currentPositionMs)) / \(formatTimecode(totalDurationMs))")
                .font(.system(size: 13, weight: .semibold, design: .monospaced))
                .foregroundColor(.white)
                .frame(minWidth: 125, alignment: .leading)

            // Scrubbing Slider
            Slider(
                value: Binding(
                    get: {
                        guard totalDurationMs > 0 else { return 0.0 }
                        return Double(currentPositionMs) / Double(totalDurationMs)
                    },
                    set: { ratio in
                        guard totalDurationMs > 0 else { return }
                        let target = Int64(ratio * Double(totalDurationMs))
                        onSeek(target)
                    }
                ),
                in: 0.0...1.0
            )
            .disabled(!hasVideo || isRecording)
            .tint(.blue)
        }
        .padding(.horizontal, 16)
        .frame(height: 60)
        .background(Color(white: 0.15))
        .clipShape(RoundedRectangle(cornerRadius: 10))
        .overlay(
            RoundedRectangle(cornerRadius: 10)
                .stroke(Color.white.opacity(0.1), lineWidth: 1)
        )
    }

    private func formatTimecode(_ ms: Int64) -> String {
        let totalSeconds = max(0, ms / 1000)
        let minutes = totalSeconds / 60
        let seconds = totalSeconds % 60
        let centis = (max(0, ms) % 1000) / 10
        return String(format: "%02d:%02d.%02d", minutes, seconds, centis)
    }
}
