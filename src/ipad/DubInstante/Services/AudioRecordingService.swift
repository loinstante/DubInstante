import Foundation
import AVFoundation

public final class AudioRecordingService: NSObject, ObservableObject, AVAudioRecorderDelegate {
    @Published public var isRecording: Bool = false
    @Published public var currentRecordedURL: URL?

    private var audioRecorder: AVAudioRecorder?
    private var recordStartTime: Date?

    public override init() {
        super.init()
    }

    public func requestPermission(completion: @escaping (Bool) -> Void) {
        if #available(iOS 17.0, *) {
            AVAudioApplication.requestRecordPermission { granted in
                DispatchQueue.main.async { completion(granted) }
            }
        } else {
            AVAudioSession.sharedInstance().requestRecordPermission { granted in
                DispatchQueue.main.async { completion(granted) }
            }
        }
    }

    public func startRecording() -> Bool {
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.playAndRecord, mode: .videoRecording, options: [.defaultToSpeaker, .allowBluetooth])
            try session.setActive(true, options: .notifyOthersOnDeactivation)
        } catch {
            print("Failed to configure AVAudioSession: \(error)")
            return false
        }

        let tempDir = FileManager.default.temporaryDirectory
        let fileName = "dubinstante_voice_\(Int(Date().timeIntervalSince1970 * 1000)).m4a"
        let fileURL = tempDir.appendingPathComponent(fileName)

        let settings: [String: Any] = [
            AVFormatIDKey: Int(kAudioFormatMPEG4AAC),
            AVSampleRateKey: 48000.0,
            AVNumberOfChannelsKey: 1,
            AVEncoderBitRateKey: 192000,
            AVEncoderAudioQualityKey: AVAudioQuality.high.rawValue
        ]

        do {
            audioRecorder = try AVAudioRecorder(url: fileURL, settings: settings)
            audioRecorder?.delegate = self
            audioRecorder?.prepareToRecord()

            let success = audioRecorder?.record() ?? false
            if success {
                isRecording = true
                recordStartTime = Date()
                currentRecordedURL = fileURL
                return true
            } else {
                return false
            }
        } catch {
            print("Failed to start AVAudioRecorder: \(error)")
            return false
        }
    }

    public func stopRecording() -> (url: URL?, durationMs: Int64) {
        guard isRecording, let recorder = audioRecorder else {
            return (nil, 0)
        }

        let durationSeconds: Double
        if let start = recordStartTime {
            durationSeconds = Date().timeIntervalSince(start)
        } else {
            durationSeconds = recorder.currentTime
        }

        let durationMs = Int64(durationSeconds * 1000)
        recorder.stop()
        audioRecorder = nil
        isRecording = false
        recordStartTime = nil

        let url = currentRecordedURL
        return (url, max(0, durationMs))
    }
}
