import SwiftUI

public struct RythmoBandView: View {
    @Binding public var text: String
    public let currentPositionMs: Int64
    public let speedPixelsPerSecond: Double
    public let style: RythmoStyle
    public let onSeekRequested: (Int64) -> Void

    @State private var dragStartMs: Int64 = 0
    @State private var isDragging: Bool = false
    @State private var isEditingText: Bool = false
    @State private var editedText: String = ""

    public init(
        text: Binding<String>,
        currentPositionMs: Int64,
        speedPixelsPerSecond: Double,
        style: RythmoStyle = RythmoPresets.dark,
        onSeekRequested: @escaping (Int64) -> Void
    ) {
        self._text = text
        self.currentPositionMs = currentPositionMs
        self.speedPixelsPerSecond = speedPixelsPerSecond
        self.style = style
        self.onSeekRequested = onSeekRequested
    }

    public var body: some View {
        GeometryReader { geometry in
            let width = geometry.size.width
            let height = geometry.size.height
            let centerX = width / 2.0

            // Position calculation
            let offsetPx = Double(currentPositionMs) * (speedPixelsPerSecond / 1000.0)
            let textStartX = centerX - offsetPx

            ZStack {
                // Background
                style.backgroundColor

                // Scrolling Text using Canvas for maximum 120 FPS efficiency
                Canvas { context, size in
                    let font: Font = .system(size: style.textSize, weight: .bold, design: style.fontFamilyType.fontDesign)
                    let resolvedText = context.resolve(
                        Text(text)
                            .font(font)
                            .foregroundColor(style.textColor)
                    )

                    let textY = size.height / 2.0
                    context.draw(
                        resolvedText,
                        at: CGPoint(x: textStartX, y: textY),
                        anchor: .leading
                    )
                }

                // Vertical Playhead Bar in the exact center
                Rectangle()
                    .fill(style.playheadColor)
                    .frame(width: 4)
                    .overlay(
                        VStack(spacing: 0) {
                            // Triangle arrow at the top
                            Image(systemName: "arrowtriangle.down.fill")
                                .resizable()
                                .scaledToFit()
                                .frame(width: 12, height: 10)
                                .foregroundColor(style.playheadColor)
                            Spacer()
                            // Triangle arrow at the bottom
                            Image(systemName: "arrowtriangle.up.fill")
                                .resizable()
                                .scaledToFit()
                                .frame(width: 12, height: 10)
                                .foregroundColor(style.playheadColor)
                        }
                    )

                // Drag gesture overlay
                Color.clear
                    .contentShape(Rectangle())
                    .gesture(
                        DragGesture(minimumDistance: 0)
                            .onChanged { value in
                                if !isDragging {
                                    isDragging = true
                                    dragStartMs = currentPositionMs
                                }
                                // Drag left moves forward, drag right moves backward
                                let deltaX = value.translation.width
                                let deltaMs = Int64(-deltaX / (speedPixelsPerSecond / 1000.0))
                                let targetMs = max(0, dragStartMs + deltaMs)
                                onSeekRequested(targetMs)
                            }
                            .onEnded { _ in
                                isDragging = false
                            }
                    )
                    .onTapGesture(count: 2) {
                        editedText = text
                        isEditingText = true
                    }
            }
            .clipShape(RoundedRectangle(cornerRadius: 10))
            .overlay(
                RoundedRectangle(cornerRadius: 10)
                    .stroke(Color.white.opacity(0.15), lineWidth: 1)
            )
            .alert("Modifier le texte rythmo", isPresented: $isEditingText) {
                TextField("Texte rythmo", text: $editedText)
                Button("Annuler", role: .cancel) {}
                Button("Enregistrer") {
                    text = editedText
                }
            }
        }
    }
}
