import Foundation
import AVFoundation
import Combine

public final class VideoPlayerService: ObservableObject {
    public let player: AVPlayer = AVPlayer()
    public let auditionPlayer: AVPlayer = AVPlayer()

    @Published public var isPlaying: Bool = false
    @Published public var currentPositionMs: Int64 = 0
    @Published public var totalDurationMs: Int64 = 0
    @Published public var isAuditionPlaying: Bool = false
    @Published public var currentVideoURL: URL?

    private var timeObserverToken: Any?
    private var isAccessingSecurityScoped: Bool = false
    private var cancellables = Set<AnyCancellable>()

    public init() {
        setupTimeObserver()
        setupEndObserver()
    }

    deinit {
        removeTimeObserver()
        if isAccessingSecurityScoped, let url = currentVideoURL {
            url.stopAccessingSecurityScopedResource()
        }
    }

    public func loadVideo(url: URL) {
        if isAccessingSecurityScoped, let prevUrl = currentVideoURL {
            prevUrl.stopAccessingSecurityScopedResource()
            isAccessingSecurityScoped = false
        }

        if url.startAccessingSecurityScopedResource() {
            isAccessingSecurityScoped = true
        }

        currentVideoURL = url
        let asset = AVURLAsset(url: url)
        let playerItem = AVPlayerItem(asset: asset)

        player.replaceCurrentItem(with: playerItem)
        currentPositionMs = 0
        isPlaying = false

        Task { @MainActor in
            do {
                let duration = try await asset.load(.duration)
                let seconds = CMTimeGetSeconds(duration)
                if !seconds.isNaN && seconds > 0 {
                    self.totalDurationMs = Int64(seconds * 1000)
                } else {
                    self.totalDurationMs = 0
                }
            } catch {
                print("Failed to load duration: \(error)")
                self.totalDurationMs = 0
            }
        }
    }

    public func play() {
        player.play()
        if isAuditionPlaying {
            auditionPlayer.play()
        }
        isPlaying = true
    }

    public func pause() {
        player.pause()
        auditionPlayer.pause()
        isPlaying = false
    }

    public func togglePlayPause() {
        if isPlaying {
            pause()
        } else {
            play()
        }
    }

    public func seek(toMs ms: Int64) {
        let clamped = max(0, min(ms, totalDurationMs > 0 ? totalDurationMs : ms))
        currentPositionMs = clamped
        let cmTime = CMTime(value: clamped, timescale: 1000)

        player.seek(to: cmTime, toleranceBefore: .zero, toleranceAfter: .zero)
        if isAuditionPlaying {
            auditionPlayer.seek(to: cmTime, toleranceBefore: .zero, toleranceAfter: .zero)
        }
    }

    public func stepSeconds(_ delta: Double) {
        let currentSec = Double(currentPositionMs) / 1000.0
        let targetSec = max(0, min(currentSec + delta, Double(totalDurationMs) / 1000.0))
        seek(toMs: Int64(targetSec * 1000))
    }

    public func setVolume(_ volume: Float) {
        player.volume = max(0.0, min(volume, 1.0))
    }

    public func setAuditionVolume(_ volume: Float) {
        auditionPlayer.volume = max(0.0, min(volume, 1.0))
    }

    public func startAudition(with takeURL: URL) {
        let takeItem = AVPlayerItem(url: takeURL)
        auditionPlayer.replaceCurrentItem(with: takeItem)

        // Reset both to beginning for auditioning
        seek(toMs: 0)
        isAuditionPlaying = true
        play()
    }

    public func stopAudition() {
        auditionPlayer.pause()
        isAuditionPlaying = false
    }

    public func toggleAudition(with takeURL: URL) {
        if isAuditionPlaying {
            stopAudition()
            pause()
        } else {
            startAudition(with: takeURL)
        }
    }

    private func setupTimeObserver() {
        let interval = CMTime(value: 1, timescale: 60) // 60 FPS precision
        timeObserverToken = player.addPeriodicTimeObserver(forInterval: interval, queue: .main) { [weak self] time in
            guard let self = self, self.isPlaying else { return }
            let ms = Int64(CMTimeGetSeconds(time) * 1000)
            if ms >= 0 {
                self.currentPositionMs = ms
            }
        }
    }

    private func setupEndObserver() {
        NotificationCenter.default.addObserver(
            forName: .AVPlayerItemDidPlayToEndTime,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            guard let self = self else { return }
            self.pause()
            if self.isAuditionPlaying {
                self.stopAudition()
            }
        }
    }

    private func removeTimeObserver() {
        if let token = timeObserverToken {
            player.removeTimeObserver(token)
            timeObserverToken = nil
        }
    }
}
