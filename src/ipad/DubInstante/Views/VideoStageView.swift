import SwiftUI
import AVFoundation

public struct VideoStageView: View {
    public let player: AVPlayer
    public let hasVideo: Bool
    public let onOpenVideo: () -> Void

    public init(player: AVPlayer, hasVideo: Bool, onOpenVideo: @escaping () -> Void) {
        self.player = player
        self.hasVideo = hasVideo
        self.onOpenVideo = onOpenVideo
    }

    public var body: some View {
        ZStack {
            Color.black

            if hasVideo {
                CustomPlayerLayerRepresentable(player: player)
            } else {
                VStack(spacing: 14) {
                    Image(systemName: "film")
                        .font(.system(size: 48))
                        .foregroundColor(.gray)

                    Text("Aucune vidéo chargée")
                        .font(.headline)
                        .foregroundColor(.gray)

                    Button(action: onOpenVideo) {
                        Label("Ouvrir une vidéo pour commencer", systemImage: "folder.badge.plus")
                            .font(.system(size: 14, weight: .semibold))
                            .padding(.horizontal, 16)
                            .padding(.vertical, 10)
                            .background(Color.blue)
                            .foregroundColor(.white)
                            .cornerRadius(8)
                    }
                }
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: 12))
        .overlay(
            RoundedRectangle(cornerRadius: 12)
                .stroke(Color.white.opacity(0.12), lineWidth: 1)
        )
    }
}

private struct CustomPlayerLayerRepresentable: UIViewRepresentable {
    let player: AVPlayer

    func makeUIView(context: Context) -> PlayerUIView {
        let view = PlayerUIView()
        view.playerLayer.player = player
        view.playerLayer.videoGravity = .resizeAspect
        view.backgroundColor = .black
        return view
    }

    func updateUIView(_ uiView: PlayerUIView, context: Context) {
        uiView.playerLayer.player = player
    }
}

private class PlayerUIView: UIView {
    override static var layerClass: AnyClass {
        AVPlayerLayer.self
    }

    var playerLayer: AVPlayerLayer {
        layer as! AVPlayerLayer
    }
}
