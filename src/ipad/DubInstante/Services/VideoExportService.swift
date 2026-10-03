import Foundation
import AVFoundation

public final class VideoExportService: ObservableObject {
    @Published public var isExporting: Bool = false
    @Published public var exportProgress: Int = 0

    public init() {}

    public func exportDubbedVideo(
        sourceVideoURL: URL,
        recordedAudioURL: URL,
        mediaVolume: Float,
        micVolume: Float,
        onProgress: @escaping (Int) -> Void,
        completion: @escaping (Result<URL, Error>) -> Void
    ) {
        isExporting = true
        exportProgress = 0

        Task {
            do {
                let videoAsset = AVURLAsset(url: sourceVideoURL)
                let micAsset = AVURLAsset(url: recordedAudioURL)

                let composition = AVMutableComposition()
                var audioMixParameters = [AVMutableAudioMixInputParameters]()

                // 1. Video Track
                let videoDuration = try await videoAsset.load(.duration)
                let videoTimeRange = CMTimeRange(start: .zero, duration: videoDuration)

                let sourceVideoTracks = try await videoAsset.loadTracks(withMediaType: .video)
                if let sourceVideoTrack = sourceVideoTracks.first,
                   let compositionVideoTrack = composition.addMutableTrack(
                    withMediaType: .video,
                    preferredTrackID: kCMPersistentTrackID_Invalid
                   ) {
                    try compositionVideoTrack.insertTimeRange(videoTimeRange, of: sourceVideoTrack, at: .zero)
                    let transform = try await sourceVideoTrack.load(.preferredTransform)
                    compositionVideoTrack.preferredTransform = transform
                }

                // 2. Original Audio Track
                let sourceAudioTracks = try await videoAsset.loadTracks(withMediaType: .audio)
                if let sourceAudioTrack = sourceAudioTracks.first,
                   let compositionAudioTrack = composition.addMutableTrack(
                    withMediaType: .audio,
                    preferredTrackID: kCMPersistentTrackID_Invalid
                   ) {
                    try compositionAudioTrack.insertTimeRange(videoTimeRange, of: sourceAudioTrack, at: .zero)
                    let params = AVMutableAudioMixInputParameters(track: compositionAudioTrack)
                    params.setVolume(mediaVolume, at: .zero)
                    audioMixParameters.append(params)
                }

                // 3. Microphone Dubbed Audio Track
                let micAudioTracks = try await micAsset.loadTracks(withMediaType: .audio)
                if let micAudioTrack = micAudioTracks.first,
                   let compositionMicTrack = composition.addMutableTrack(
                    withMediaType: .audio,
                    preferredTrackID: kCMPersistentTrackID_Invalid
                   ) {
                    let micDuration = try await micAsset.load(.duration)
                    let exportMicDuration = min(videoDuration, micDuration)
                    let micTimeRange = CMTimeRange(start: .zero, duration: exportMicDuration)

                    try compositionMicTrack.insertTimeRange(micTimeRange, of: micAudioTrack, at: .zero)
                    let micParams = AVMutableAudioMixInputParameters(track: compositionMicTrack)
                    micParams.setVolume(micVolume, at: .zero)
                    audioMixParameters.append(micParams)
                }

                let audioMix = AVMutableAudioMix()
                audioMix.inputParameters = audioMixParameters

                // 4. Output Destination in Documents or Temp
                let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
                let outputFileName = "DubInstante_\(Int(Date().timeIntervalSince1970)).mp4"
                let outputURL = docs.appendingPathComponent(outputFileName)

                if FileManager.default.fileExists(atPath: outputURL.path) {
                    try FileManager.default.removeItem(at: outputURL)
                }

                guard let exportSession = AVAssetExportSession(
                    asset: composition,
                    presetName: AVAssetExportPresetHighestQuality
                ) else {
                    throw NSError(domain: "DubInstante", code: -1, userInfo: [NSLocalizedDescriptionKey: "Failed to initialize AVAssetExportSession"])
                }

                exportSession.outputURL = outputURL
                exportSession.outputFileType = .mp4
                exportSession.audioMix = audioMix
                exportSession.shouldOptimizeForNetworkUse = true

                // Progress timer
                let timer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] t in
                    let prog = Int(exportSession.progress * 100)
                    DispatchQueue.main.async {
                        self?.exportProgress = prog
                        onProgress(prog)
                    }
                    if exportSession.status != .exporting && exportSession.status != .waiting {
                        t.invalidate()
                    }
                }

                await exportSession.export()
                timer.invalidate()

                DispatchQueue.main.async {
                    self.isExporting = false
                    if exportSession.status == .completed {
                        self.exportProgress = 100
                        onProgress(100)
                        completion(.success(outputURL))
                    } else {
                        let err = exportSession.error ?? NSError(domain: "DubInstante", code: -2, userInfo: [NSLocalizedDescriptionKey: "Export canceled or failed."])
                        completion(.failure(err))
                    }
                }
            } catch {
                DispatchQueue.main.async {
                    self.isExporting = false
                    completion(.failure(error))
                }
            }
        }
    }
}
