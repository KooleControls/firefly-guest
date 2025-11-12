#pragma once
#include <stdint.h>
#include "FontDef.h"
#include "font5x7.h"

struct TextStyle
{
    const FontDef* font;
    uint8_t size;
    bool color;

    constexpr TextStyle(const FontDef* f, uint8_t s = 1, bool c = true)
        : font(f), size(s), color(c) {}

    // ---- Predefined common styles ----
    static constexpr TextStyle Default(int size = 1)      { return TextStyle(&Font5x7, size, true); }
};
