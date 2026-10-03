import SwiftUI

public struct StyleCustomizationSheet: View {
    @Binding public var style: RythmoStyle
    @Environment(\.dismiss) private var dismiss

    @State private var workingStyle: RythmoStyle

    public init(style: Binding<RythmoStyle>) {
        self._style = style
        self._workingStyle = State(initialValue: style.wrappedValue)
    }

    public var body: some View {
        NavigationStack {
            VStack(spacing: 20) {
                // Live Preview Card
                VStack(alignment: .leading, spacing: 6) {
                    Text("Aperçu de la bande")
                        .font(.system(size: 13, weight: .bold))
                        .foregroundColor(.gray)

                    ZStack {
                        workingStyle.backgroundColor

                        Text("Synchronisation vocale studio...")
                            .font(.system(
                                size: workingStyle.textSize,
                                weight: .bold,
                                design: workingStyle.fontFamilyType.fontDesign
                            ))
                            .foregroundColor(workingStyle.textColor)

                        Rectangle()
                            .fill(workingStyle.playheadColor)
                            .frame(width: 4)
                    }
                    .frame(height: 90)
                    .clipShape(RoundedRectangle(cornerRadius: 10))
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
                            .stroke(Color.white.opacity(0.2), lineWidth: 1)
                    )
                }
                .padding(.horizontal)

                Form {
                    // Presets
                    Section("Préréglages") {
                        ScrollView(.horizontal, showsIndicators: false) {
                            HStack(spacing: 10) {
                                ForEach(RythmoPresets.all, id: \.name) { preset in
                                    Button {
                                        workingStyle = preset.style
                                    } label: {
                                        Text(preset.name)
                                            .font(.system(size: 13, weight: .semibold))
                                            .padding(.horizontal, 14)
                                            .padding(.vertical, 8)
                                            .background(Color.blue.opacity(0.15))
                                            .foregroundColor(.blue)
                                            .cornerRadius(8)
                                    }
                                }
                            }
                            .padding(.vertical, 4)
                        }
                    }

                    // Typography
                    Section("Typographie") {
                        Picker("Police", selection: $workingStyle.fontFamilyType) {
                            ForEach(FontFamilyType.allCases) { font in
                                Text(font.rawValue).tag(font)
                            }
                        }
                        .pickerStyle(.segmented)

                        VStack(alignment: .leading, spacing: 6) {
                            HStack {
                                Text("Taille du texte")
                                Spacer()
                                Text("\(Int(workingStyle.textSize)) pt")
                                    .foregroundColor(.gray)
                            }
                            Slider(value: $workingStyle.textSize, in: 24...60, step: 2)
                        }
                    }

                    // Colors
                    Section("Couleurs") {
                        ColorPicker("Couleur du texte", selection: $workingStyle.textColor)
                        ColorPicker("Couleur du fond", selection: $workingStyle.backgroundColor)
                        ColorPicker("Couleur du curseur", selection: $workingStyle.playheadColor)
                    }
                }
            }
            .navigationTitle("Personnalisation du style")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Annuler") {
                        dismiss()
                    }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Enregistrer") {
                        style = workingStyle
                        dismiss()
                    }
                    .fontWeight(.bold)
                }
            }
        }
        .frame(minWidth: 500, minHeight: 550)
    }
}
