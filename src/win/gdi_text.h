#pragma once

#include "../core/text.h"

#include <windows.h>

namespace rf::win
{
    class GdiTextEngine : public TextEngine
    {
    public:
        GdiTextEngine();
        ~GdiTextEngine() override;
        GdiTextEngine(const GdiTextEngine&) = delete;
        GdiTextEngine& operator=(const GdiTextEngine&) = delete;

        int Width(const std::wstring& text, const Font& font) override;
        int Height(const Font& font) override;
        int Ascent(const Font& font) override;
        void Draw(Image& target, const Rect& clip, int x, int y, const std::wstring& text, const Font& font, Color color) override;

    private:
        HFONT Pick(const Font& font) const;
        const TEXTMETRICW& Metrics(const Font& font) const;
        void EnsureScratch(int w, int h);

        HDC dc_ = nullptr;
        HFONT fonts_[2][2] = {{nullptr, nullptr}, {nullptr, nullptr}};
        TEXTMETRICW metrics_[2][2]{};
        HBITMAP scratch_ = nullptr;
        HGDIOBJ oldBitmap_ = nullptr;
        void* bits_ = nullptr;
        int scratchW_ = 0;
        int scratchH_ = 0;
    };
}
