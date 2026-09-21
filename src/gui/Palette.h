#ifndef PALETTE_H
#define PALETTE_H

// Brand colors shared by C++ code. The QSS files cannot reference these
// (no variables in QSS): keep style.qss / style_dark.qss in sync by hand.
namespace Brand {
inline constexpr const char *Accent = "#7c56f5";
inline constexpr const char *AccentLight = "#926bff";
inline constexpr const char *TextMuted = "#8a8a9e";
inline constexpr const char *SurfaceAlt = "#3b3b52";
}

#endif // PALETTE_H
