import SwiftUI

public struct TopStudioBar: View {
    public let videoName: String?
    public let canExport: Bool
    public let onOpenVideo: () -> Void
    public let onOpenStyleSheet: () -> Void
    public let onExport: () -> Void

    public init(
        videoName: String?,
        canExport: Bool,
        onOpenVideo: @escaping () -> Void,
        onOpenStyleSheet: @escaping () -> Void,
        onExport: @escaping () -> Void
    ) {
        self.videoName = videoName
        self.canExport = canExport
        self.onOpenVideo = onOpenVideo
        self.onOpenStyleSheet = onOpenStyleSheet
        self.onExport = onExport
    }

    public var body: some View {
        HStack(spacing: 16) {
            // App Branding
            HStack(spacing: 10) {
                Image(systemName: "waveform.badge.mic")
                    .font(.system(size: 26))
                    .foregroundColor(.blue)

                VStack(alignment: .leading, spacing: 2) {
                    Text("DubInstante Studio")
                        .font(.system(size: 18, weight: .bold))
                        .foregroundColor(.blue)
                    Text("iPad Edition • 1 Mic Studio")
                        .font(.system(size: 11))
                        .foregroundColor(.gray)
                }
            }

            // Current Video Badge
            if let name = videoName {
                HStack(spacing: 6) {
                    Image(systemName: "film")
                    Text(name)
                        .lineLimit(1)
                }
                .font(.system(size: 12, weight: .medium))
                .padding(.horizontal, 10)
                .padding(.vertical, 5)
                .background(Color.blue.opacity(0.2))
                .foregroundColor(.blue)
                .cornerRadius(8)
            }

            Spacer()

            // Header Action Buttons
            HStack(spacing: 12) {
                Button(action: onOpenVideo) {
                    Label("Ouvrir Vidéo", systemImage: "folder")
                        .font(.system(size: 13, weight: .semibold))
                        .padding(.horizontal, 14)
                        .padding(.vertical, 8)
                        .background(Color(white: 0.2))
                        .foregroundColor(.white)
                        .cornerRadius(8)
                }

                Button(action: onOpenStyleSheet) {
                    Label("Style Bande", systemImage: "paintbrush")
                        .font(.system(size: 13, weight: .semibold))
                        .padding(.horizontal, 14)
                        .padding(.vertical, 8)
                        .background(Color(white: 0.2))
                        .foregroundColor(.white)
                        .cornerRadius(8)
                }

                Button(action: onExport) {
                    Label("Exporter", systemImage: "square.and.arrow.up")
                        .font(.system(size: 13, weight: .bold))
                        .padding(.horizontal, 16)
                        .padding(.vertical, 8)
                        .background(canExport ? Color.green : Color.gray.opacity(0.3))
                        .foregroundColor(.white)
                        .cornerRadius(8)
                }
                .disabled(!canExport)
            }
        }
        .padding(.horizontal, 20)
        .frame(height: 64)
        .background(Color(white: 0.14))
    }
}
