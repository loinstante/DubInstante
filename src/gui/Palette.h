#ifndef PALETTE_H
#define PALETTE_H

// Brand colors shared by C++ code. The QSS files cannot reference these
// (no variables in QSS): keep style.qss / style_dark.qss in sync by hand.
namespace Brand {
inline constexpr const char *Accent = "#a3454a";
inline constexpr const char *AccentLight = "#b65a5f";
inline constexpr const char *TextMuted = "#979590";
inline constexpr const char *SurfaceAlt = "#434343";
}

#endif // PALETTE_H
