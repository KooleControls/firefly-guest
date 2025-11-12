#pragma once
#include <stdint.h>


struct FontDef
{
    const uint8_t* table;   // pointer to first byte of the font data
    uint8_t width;          // number of bytes per glyph (each column)
    uint8_t height;         // bits per column
    uint8_t firstChar;      // usually 32
    uint8_t lastChar;       // usually 127

    inline const uint8_t* GetGlyph(char c) const
    {
        if (c < firstChar || c > lastChar)
            return nullptr;
        return table + (c - firstChar) * width;
    }
};
