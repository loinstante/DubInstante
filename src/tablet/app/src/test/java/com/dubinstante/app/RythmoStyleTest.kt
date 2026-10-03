package com.dubinstante.app

import androidx.compose.ui.graphics.Color
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class RythmoStyleTest {

    @Test
    fun testPresetsCountAndIntegrity() {
        assertEquals(6, RythmoPresets.all.size)
        val names = RythmoPresets.all.map { it.first }
        assertEquals(listOf("Dark", "Classic", "Blue", "Red", "Green", "Yellow"), names)
    }

    @Test
    fun testClassicPresetColors() {
        val classic = RythmoPresets.Classic
        assertEquals(Color(0xFF222222), classic.textColor)
        assertEquals(Color(0xFFFFFFFF), classic.backgroundColor)
        assertEquals(FontFamilyType.MONOSPACE, classic.fontFamilyType)
    }

    @Test
    fun testFontFamilyIndexMapping() {
        assertEquals(FontFamilyType.MONOSPACE, FontFamilyType.fromIndex(0))
        assertEquals(FontFamilyType.SANS_SERIF, FontFamilyType.fromIndex(1))
        assertEquals(FontFamilyType.SERIF, FontFamilyType.fromIndex(2))
        // Out of bound fallback
        assertEquals(FontFamilyType.MONOSPACE, FontFamilyType.fromIndex(99))
    }

    @Test
    fun testCustomStyleCopy() {
        val custom = RythmoPresets.Dark.copy(
            textSizeSp = 56f,
            playheadColor = Color.Cyan
        )
        assertEquals(56f, custom.textSizeSp)
        assertEquals(Color.Cyan, custom.playheadColor)
        assertEquals(Color.White, custom.textColor)
    }
}
