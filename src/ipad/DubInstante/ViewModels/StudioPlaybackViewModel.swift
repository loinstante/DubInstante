import Foundation
import SwiftUI
import Combine

@MainActor
public final class StudioPlaybackViewModel: ObservableObject {
    public let videoService = VideoPlayerService()
    public let recordService = AudioRecordingService()
    public let exportService = VideoExportService()

    @Published public var rythmoText: String = "Ceci est une bande rythmo de test pour le doublage iPad..."
    @Published public var rythmoSpeed: Double = 100.0 // Pixels per second
    @Published public var rythmoStyle: RythmoStyle = RythmoPresets.dark
    @Published public var currentTake: AudioTake?

    @Published public var volume: Float = 1.0 {
        didSet { videoService.setVolume(volume) }
    }
    @Published public var micVolume: Float = 1.0 {
        didSet { videoService.setAuditionVolume(micVolume) }
    }

    @Published public var videoFileName: String?
    @Published public var errorMessage: String?
    @Published public var showExportShareSheet: Bool = false
    @Published public var exportedFileURL: URL?

    private var cancellables = Set<AnyCancellable>()

    public init() {
        // Forward changes from sub-services
        videoService.objectWillChange
            .sink { [weak self] _ in self?.objectWillChange.send() }
            .store(in: &cancellables)

        recordService.objectWillChange
            .sink { [weak self] _ in self?.objectWillChange.send() }
            .store(in: &cancellables)

        exportService.objectWillChange
            .sink { [weak self] _ in self?.objectWillChange.send() }
            .store(in: &cancellables)
    }

    public func openVideo(url: URL) {
        videoFileName = url.lastPathComponent
        clearTake()
        videoService.loadVideo(url: url)
    }

    public func handleRecordToggle() {
        if recordService.isRecording {
            stopRecording()
        } else {
            recordService.requestPermission { [weak self] granted in
                guard let self = self else { return }
                if granted {
                    self.startRecording()
                } else {
                    self.errorMessage = "Permission micro requise pour le doublage."
                }
            }
        }
    }

    private func startRecording() {
        if videoService.isAuditionPlaying {
            videoService.stopAudition()
        }

        // Seek video to start
        videoService.seek(toMs: 0)

        if recordService.startRecording() {
            videoService.play()
        } else {
            errorMessage = "Impossible de démarrer l'enregistrement micro."
        }
    }

    public func stopRecording() {
        videoService.pause()
        let result = recordService.stopRecording()
        if let url = result.url, result.durationMs > 200 {
            currentTake = AudioTake(fileURL: url, durationMs: result.durationMs)
        } else {
            errorMessage = "Prise audio trop courte ou invalide."
        }
    }

    public func toggleAudition() {
        guard let take = currentTake else { return }
        videoService.toggleAudition(with: take.fileURL)
    }

    public func clearTake() {
        videoService.stopAudition()
        currentTake = nil
    }

    public func exportDubbedVideo() {
        guard let videoURL = videoService.currentVideoURL,
              let take = currentTake else {
            errorMessage = "Vidéo ou prise audio manquante pour l'export."
            return
        }

        exportService.exportDubbedVideo(
            sourceVideoURL: videoURL,
            recordedAudioURL: take.fileURL,
            mediaVolume: volume,
            micVolume: micVolume,
            onProgress: { _ in }
        ) { [weak self] result in
            guard let self = self else { return }
            switch result {
            case .success(let outputURL):
                self.exportedFileURL = outputURL
                self.showExportShareSheet = true
            case .failure(let error):
                self.errorMessage = "Échec de l'export: \(error.localizedDescription)"
            }
        }
    }
}
