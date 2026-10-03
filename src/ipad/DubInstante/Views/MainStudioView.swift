import SwiftUI
import UniformTypeIdentifiers

public struct MainStudioView: View {
    @StateObject private var viewModel = StudioPlaybackViewModel()

    @State private var showFileImporter: Bool = false
    @State private var showStyleSheet: Bool = false

    public init() {}

    public var body: some View {
        ZStack {
            Color(white: 0.08).ignoresSafeArea()

            VStack(spacing: 0) {
                // Top Navigation & Actions Bar
                TopStudioBar(
                    videoName: viewModel.videoFileName,
                    canExport: viewModel.currentTake != nil && !viewModel.recordService.isRecording && !viewModel.exportService.isExporting,
                    onOpenVideo: { showFileImporter = true },
                    onOpenStyleSheet: { showStyleSheet = true },
                    onExport: { viewModel.exportDubbedVideo() }
                )

                // Main Studio 2-Pane Console (Landscape iPad)
                HStack(spacing: 12) {
                    // Left Column: Video Stage + Rythmo Band + Transport Bar
                    VStack(spacing: 10) {
                        // 16:9 Video Canvas
                        VideoStageView(
                            player: viewModel.videoService.player,
                            hasVideo: viewModel.videoService.currentVideoURL != nil,
                            onOpenVideo: { showFileImporter = true }
                        )
                        .frame(maxWidth: .infinity, maxHeight: .infinity)

                        // Rythmo Band (Height 110)
                        RythmoBandView(
                            text: $viewModel.rythmoText,
                            currentPositionMs: viewModel.videoService.currentPositionMs,
                            speedPixelsPerSecond: viewModel.rythmoSpeed,
                            style: viewModel.rythmoStyle,
                            onSeekRequested: { targetMs in
                                viewModel.videoService.seek(toMs: targetMs)
                            }
                        )
                        .frame(height: 110)

                        // Transport & Scrubbing Bar (Height 60)
                        TransportBarView(
                            isPlaying: viewModel.videoService.isPlaying,
                            currentPositionMs: viewModel.videoService.currentPositionMs,
                            totalDurationMs: viewModel.videoService.totalDurationMs,
                            hasVideo: viewModel.videoService.currentVideoURL != nil,
                            isRecording: viewModel.recordService.isRecording,
                            onJumpStart: { viewModel.videoService.seek(toMs: 0) },
                            onStepBack5s: { viewModel.videoService.stepSeconds(-5.0) },
                            onTogglePlayPause: { viewModel.videoService.togglePlayPause() },
                            onStepForward5s: { viewModel.videoService.stepSeconds(5.0) },
                            onSeek: { targetMs in viewModel.videoService.seek(toMs: targetMs) }
                        )
                    }

                    // Right Column: Single Track Console
                    SingleTrackPanelView(
                        isRecording: viewModel.recordService.isRecording,
                        onRecordToggle: { viewModel.handleRecordToggle() },
                        hasTake: viewModel.currentTake != nil,
                        takeDurationMs: viewModel.currentTake?.durationMs ?? 0,
                        isAuditionPlaying: viewModel.videoService.isAuditionPlaying,
                        onAuditionToggle: { viewModel.toggleAudition() },
                        onClearTake: { viewModel.clearTake() },
                        volume: $viewModel.volume,
                        micVolume: $viewModel.micVolume,
                        rythmoSpeed: $viewModel.rythmoSpeed,
                        hasVideo: viewModel.videoService.currentVideoURL != nil
                    )
                }
                .padding(12)
            }

            // Export Progress Overlay
            if viewModel.exportService.isExporting {
                exportingModal
            }
        }
        .fileImporter(
            isPresented: $showFileImporter,
            allowedContentTypes: [.movie, .video, .quickTimeMovie, .mpeg4Movie],
            allowsMultipleSelection: false
        ) { result in
            switch result {
            case .success(let urls):
                if let url = urls.first {
                    viewModel.openVideo(url: url)
                }
            case .failure(let error):
                viewModel.errorMessage = "Échec d'ouverture du fichier : \(error.localizedDescription)"
            }
        }
        .sheet(isPresented: $showStyleSheet) {
            StyleCustomizationSheet(style: $viewModel.rythmoStyle)
        }
        .sheet(isPresented: $viewModel.showExportShareSheet) {
            if let exportURL = viewModel.exportedFileURL {
                ShareActivityView(activityItems: [exportURL])
            }
        }
        .alert("Information", isPresented: Binding(
            get: { viewModel.errorMessage != nil },
            set: { if !$0 { viewModel.errorMessage = nil } }
        )) {
            Button("OK", role: .cancel) { viewModel.errorMessage = nil }
        } message: {
            Text(viewModel.errorMessage ?? "")
        }
    }

    private var exportingModal: some View {
        ZStack {
            Color.black.opacity(0.75).ignoresSafeArea()

            VStack(spacing: 20) {
                ProgressView(value: Double(viewModel.exportService.exportProgress), total: 100.0)
                    .progressViewStyle(.circular)
                    .scaleEffect(1.5)
                    .tint(.blue)

                Text("Export de la vidéo doublée...")
                    .font(.system(size: 18, weight: .bold))
                    .foregroundColor(.white)

                Text("\(viewModel.exportService.exportProgress)%")
                    .font(.system(size: 16, weight: .semibold, design: .monospaced))
                    .foregroundColor(.blue)

                Text("Mixage matériel AVFoundation du son vidéo et de la piste micro")
                    .font(.system(size: 12))
                    .foregroundColor(.gray)
            }
            .padding(32)
            .background(Color(white: 0.15))
            .cornerRadius(16)
        }
    }
}

private struct ShareActivityView: UIViewControllerRepresentable {
    let activityItems: [Any]

    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: activityItems, applicationActivities: nil)
    }

    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}
}
