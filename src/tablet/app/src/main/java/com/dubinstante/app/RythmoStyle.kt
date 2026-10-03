package com.dubinstante.app

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb

enum class FontFamilyType(val displayName: String, val index: Int) {
    MONOSPACE("Monospace", 0),
    SANS_SERIF("Sans-Serif", 1),
    SERIF("Serif", 2);

    companion object {
        fun fromIndex(index: Int): FontFamilyType =
            entries.find { it.index == index } ?: MONOSPACE
    }
}

data class RythmoStyle(
    val textColor: Color = Color.White,
    val backgroundColor: Color = Color(0xFF1E1E1E),
    val textSizeSp: Float = 42f,
    val fontFamilyType: FontFamilyType = FontFamilyType.MONOSPACE,
    val playheadColor: Color = Color.Red
) {
    val textArgb: Int get() = textColor.toArgb()
    val backgroundArgb: Int get() = backgroundColor.toArgb()
    val playheadArgb: Int get() = playheadColor.toArgb()
}

object RythmoPresets {
    val Classic = RythmoStyle(
        textColor = Color(0xFF222222),
        backgroundColor = Color(0xFFFFFFFF),
        textSizeSp = 42f,
        fontFamilyType = FontFamilyType.MONOSPACE,
        playheadColor = Color(0xFFD32F2F)
    )

    val Dark = RythmoStyle(
        textColor = Color(0xFFFFFFFF),
        backgroundColor = Color(0xFF222222),
        textSizeSp = 42f,
        fontFamilyType = FontFamilyType.MONOSPACE,
        playheadColor = Color(0xFFFF5252)
    )

    val Blue = RythmoStyle(
        textColor = Color(0xFF0068C0),
        backgroundColor = Color(0xFFFFFFFF),
        textSizeSp = 42f,
        fontFamilyType = FontFamilyType.MONOSPACE,
        playheadColor = Color(0xFF0068C0)
    )

    val Red = RythmoStyle(
        textColor = Color(0xFFC23934),
        backgroundColor = Color(0xFFFFFFFF),
        textSizeSp = 42f,
        fontFamilyType = FontFamilyType.MONOSPACE,
        playheadColor = Color(0xFFC23934)
    )

    val Green = RythmoStyle(
        textColor = Color(0xFF27AE60),
        backgroundColor = Color(0xFF222222),
        textSizeSp = 42f,
        fontFamilyType = FontFamilyType.MONOSPACE,
        playheadColor = Color(0xFF2ECC71)
    )

    val Yellow = RythmoStyle(
        textColor = Color(0xFFF1C40F),
        backgroundColor = Color(0xFF222222),
        textSizeSp = 42f,
        fontFamilyType = FontFamilyType.MONOSPACE,
        playheadColor = Color(0xFFF39C12)
    )

    val all: List<Pair<String, RythmoStyle>> = listOf(
        "Dark" to Dark,
        "Classic" to Classic,
        "Blue" to Blue,
        "Red" to Red,
        "Green" to Green,
        "Yellow" to Yellow
    )
}
