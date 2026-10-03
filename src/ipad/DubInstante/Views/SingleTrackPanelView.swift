import SwiftUI

public struct SingleTrackPanelView: View {
    public let isRecording: Bool
    public let onRecordToggle: () -> Void
    public let hasTake: Bool
    public let takeDurationMs: Int64
    public let isAuditionPlaying: Bool
    public let onAuditionToggle: () -> Void
    public let onClearTake: () -> Void

    @Binding public var volume: Float
    @Binding public var micVolume: Float
    @Binding public var rythmoSpeed: Double
    public let hasVideo: Bool

    public init(
        isRecording: Bool,
        onRecordToggle: @escaping () -> Void,
        hasTake: Bool,
        takeDurationMs: Int64,
        isAuditionPlaying: Bool,
        onAuditionToggle: @escaping () -> Void,
        onClearTake: @escaping () -> Void,
        volume: Binding<Float>,
        micVolume: Binding<Float>,
        rythmoSpeed: Binding<Double>,
        hasVideo: Bool
    ) {
        self.isRecording = isRecording
        self.onRecordToggle = onRecordToggle
        self.hasTake = hasTake
        self.takeDurationMs = takeDurationMs
        self.isAuditionPlaying = isAuditionPlaying
        self.onAuditionToggle = onAuditionToggle
        self.onClearTake = onClearTake
        self._volume = volume
        self._micVolume = micVolume
        self._rythmoSpeed = rythmoSpeed
        self.hasVideo = hasVideo
    }

    public var body: some View {
        VStack(spacing: 16) {
            // Header with Status Badge
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Piste 1 • Studio Mic")
                        .font(.system(size: 16, weight: .bold))
                        .foregroundColor(.white)
                    Text("1 Micro • 1 Bande Rythmo")
                        .font(.system(size: 12))
                        .foregroundColor(.gray)
                }
                Spacer()

                // Status Badge
                statusBadge
            }

            Divider().background(Color.white.opacity(0.1))

            ScrollView(showsIndicators: false) {
                VStack(spacing: 16) {
                    // Big Recording Button
                    recordButtonSection

                    // Take Audition Section
                    if hasTake {
                        takeAuditionCard
                    }

                    // Mixing Volumes Section
                    mixingSection

                    // Rythmo Band Speed Section
                    speedControlSection
                }
            }
        }
        .padding(16)
        .frame(width: 340)
        .background(Color(white: 0.12))
        .clipShape(RoundedRectangle(cornerRadius: 14))
        .overlay(
            RoundedRectangle(cornerRadius: 14)
                .stroke(Color.white.opacity(0.12), lineWidth: 1)
        )
    }

    // MARK: - Subviews

    private var statusBadge: some View {
        let (text, color): (String, Color) = {
            if isRecording { return ("Recording", .red) }
            if isAuditionPlaying { return ("Audition", .purple) }
            if hasTake { return ("Prise Prête", Color.green) }
            return ("Veille", Color.gray)
        }()

        return Text(text)
            .font(.system(size: 11, weight: .bold))
            .foregroundColor(.white)
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(color)
            .cornerRadius(12)
    }

    private var recordButtonSection: some View {
        Button(action: onRecordToggle) {
            HStack(spacing: 12) {
                Image(systemName: isRecording ? "stop.circle.fill" : "record.circle")
                    .font(.system(size: 24))
                Text(isRecording ? "ARRÊTER L'ENREGISTREMENT" : "ENREGISTRER LA PRISE")
                    .font(.system(size: 13, weight: .bold))
            }
            .frame(maxWidth: .infinity)
            .frame(height: 52)
            .background(isRecording ? Color.red : Color(red: 0.8, green: 0.1, blue: 0.1))
            .foregroundColor(.white)
            .cornerRadius(10)
            .shadow(color: isRecording ? Color.red.opacity(0.6) : .clear, radius: 8)
        }
        .disabled(!hasVideo)
    }

    private var takeAuditionCard: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Image(systemName: "waveform")
                    .foregroundColor(.green)
                Text("Prise enregistrée (\(String(format: "%.1f", Double(takeDurationMs) / 1000.0))s)")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundColor(.white)
                Spacer()
                Button(action: onClearTake) {
                    Image(systemName: "trash")
                        .font(.system(size: 13))
                        .foregroundColor(.red.opacity(0.8))
                }
            }

            Button(action: onAuditionToggle) {
                HStack {
                    Image(systemName: isAuditionPlaying ? "pause.fill" : "play.fill")
                    Text(isAuditionPlaying ? "Arrêter l'audition" : "Écouter la prise synchronisée")
                        .font(.system(size: 13, weight: .medium))
                }
                .frame(maxWidth: .infinity)
                .frame(height: 38)
                .background(isAuditionPlaying ? Color.purple : Color(white: 0.22))
                .foregroundColor(.white)
                .cornerRadius(8)
            }
        }
        .padding(12)
        .background(Color(white: 0.17))
        .cornerRadius(10)
        .overlay(
            RoundedRectangle(cornerRadius: 10)
                .stroke(Color.green.opacity(0.3), lineWidth: 1)
        )
    }

    private var mixingSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Niveaux & Mixage")
                .font(.system(size: 13, weight: .bold))
                .foregroundColor(.white)

            // Video Audio Level
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("Volume Vidéo")
                        .font(.system(size: 12))
                        .foregroundColor(.gray)
                    Spacer()
                    Text("\(Int(volume * 100))%")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundColor(.white)
                }
                Slider(value: $volume, in: 0.0...1.0)
                    .tint(.blue)
            }

            // Microphone Take Level
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("Volume Prise Voix")
                        .font(.system(size: 12))
                        .foregroundColor(.gray)
                    Spacer()
                    Text("\(Int(micVolume * 100))%")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundColor(.white)
                }
                Slider(value: $micVolume, in: 0.0...1.0)
                    .tint(.green)
            }
        }
        .padding(12)
        .background(Color(white: 0.16))
        .cornerRadius(10)
    }

    private var speedControlSection: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text("Vitesse Défilement")
                    .font(.system(size: 12))
                    .foregroundColor(.gray)
                Spacer()
                Text("\(Int(rythmoSpeed)) px/s")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundColor(.white)
            }

            Slider(value: $rythmoSpeed, in: 40.0...250.0, step: 5.0)
                .tint(.orange)
        }
        .padding(12)
        .background(Color(white: 0.16))
        .cornerRadius(10)
    }
}
