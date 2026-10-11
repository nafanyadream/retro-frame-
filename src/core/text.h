#pragma once

#include "image.h"

#include <string>

namespace rf
{
    enum class FontFamily
    {
        SansSerif,
        Tahoma
    };

    struct Font
    {
        FontFamily family = FontFamily::SansSerif;
        bool bold = false;
    };

    class TextEngine
    {
    public:
        virtual ~TextEngine() = default;
        virtual int Width(const std::wstring& text, const Font& font) = 0;
        virtual int Height(const Font& font) = 0;
        virtual int Ascent(const Font& font) = 0;
        virtual void Draw(Image& target, const Rect& clip, int x, int y, const std::wstring& text, const Font& font, Color color) = 0;
    };

    void SetTextEngine(TextEngine* engine);
    TextEngine& Text();

    std::wstring Ellipsize(const std::wstring& text, const Font& font, int maxWidth);
}
