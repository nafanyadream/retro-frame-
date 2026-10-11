#pragma once

#include "paint.h"

#include <string>

namespace rf
{
    enum class Style
    {
        Win95 = 0,
        Win98 = 1,
        Win2000 = 2
    };

    struct FrameColors
    {
        Color face = Rgb(0xC0, 0xC0, 0xC0);
        Color active1 = Rgb(0x00, 0x00, 0x80);
        Color active2 = Rgb(0x10, 0x84, 0xD0);
        Color inactive1 = Rgb(0x80, 0x80, 0x80);
        Color inactive2 = Rgb(0xC0, 0xC0, 0xC0);
        Color titleText = Rgb(0xFF, 0xFF, 0xFF);
        Color inactiveText = Rgb(0xC0, 0xC0, 0xC0);
        Color text = Rgb(0x00, 0x00, 0x00);
        Color window = Rgb(0xFF, 0xFF, 0xFF);
        Color select = Rgb(0x00, 0x00, 0x80);
    };

    bool HasGradient(Style style);
    FontFamily FamilyFor(Style style);
    Shades ShadesFor(Color face, Style style);

    int SchemeCount();
    std::wstring SchemeName(int index);
    FrameColors SchemeColors(int index, Style style);

    Shades ClassicShades();
}
