#pragma once
#include "esp_log.h"
#include "TextStyle.h"

class Display
{
public:
    virtual void fill(uint8_t color) = 0;
    virtual void drawPixel(int x, int y, bool color) = 0;
    virtual void show() = 0;
    virtual void drawChar(int x, int y, char c, const TextStyle &style) = 0;
    virtual void drawText(int x, int y, const char *str, const TextStyle &style) = 0;
};
