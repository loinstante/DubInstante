import SwiftUI

public enum FontFamilyType: String, CaseIterable, Identifiable {
    case monospace = "Monospace"
    case sansSerif = "Sans-Serif"
    case serif = "Serif"

    public var id: String { rawValue }

    public var fontDesign: Font.Design {
        switch self {
        case .monospace:
            return .monospaced
        case .sansSerif:
            return .default
        case .serif:
            return .serif
        }
    }
}

public struct RythmoStyle: Equatable {
    public var textColor: Color
    public var backgroundColor: Color
    public var textSize: CGFloat
    public var fontFamilyType: FontFamilyType
    public var playheadColor: Color

    public init(
        textColor: Color = .white,
        backgroundColor: Color = Color(red: 0.12, green: 0.12, blue: 0.12),
        textSize: CGFloat = 40,
        fontFamilyType: FontFamilyType = .monospace,
        playheadColor: Color = .red
    ) {
        self.textColor = textColor
        self.backgroundColor = backgroundColor
        self.textSize = textSize
        self.fontFamilyType = fontFamilyType
        self.playheadColor = playheadColor
    }
}

public enum RythmoPresets {
    public static let dark = RythmoStyle(
        textColor: .white,
        backgroundColor: Color(red: 0.13, green: 0.13, blue: 0.13),
        textSize: 40,
        fontFamilyType: .monospace,
        playheadColor: Color(red: 1.0, green: 0.32, blue: 0.32)
    )

    public static let classic = RythmoStyle(
        textColor: Color(red: 0.13, green: 0.13, blue: 0.13),
        backgroundColor: .white,
        textSize: 40,
        fontFamilyType: .monospace,
        playheadColor: Color(red: 0.83, green: 0.18, blue: 0.18)
    )

    public static let blue = RythmoStyle(
        textColor: Color(red: 0.0, green: 0.41, blue: 0.75),
        backgroundColor: .white,
        textSize: 40,
        fontFamilyType: .monospace,
        playheadColor: Color(red: 0.0, green: 0.41, blue: 0.75)
    )

    public static let red = RythmoStyle(
        textColor: Color(red: 0.76, green: 0.22, blue: 0.20),
        backgroundColor: .white,
        textSize: 40,
        fontFamilyType: .monospace,
        playheadColor: Color(red: 0.76, green: 0.22, blue: 0.20)
    )

    public static let green = RythmoStyle(
        textColor: Color(red: 0.15, green: 0.68, blue: 0.38),
        backgroundColor: Color(red: 0.13, green: 0.13, blue: 0.13),
        textSize: 40,
        fontFamilyType: .monospace,
        playheadColor: Color(red: 0.18, green: 0.80, blue: 0.44)
    )

    public static let yellow = RythmoStyle(
        textColor: Color(red: 0.95, green: 0.77, blue: 0.06),
        backgroundColor: Color(red: 0.13, green: 0.13, blue: 0.13),
        textSize: 40,
        fontFamilyType: .monospace,
        playheadColor: Color(red: 0.95, green: 0.61, blue: 0.07)
    )

    public static let all: [(name: String, style: RythmoStyle)] = [
        ("Dark", dark),
        ("Classic", classic),
        ("Blue", blue),
        ("Red", red),
        ("Green", green),
        ("Yellow", yellow)
    ]
}
