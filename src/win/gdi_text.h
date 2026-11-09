#pragma once

#include "../core/text.h"

#include <windows.h>

#include <cstdint>
#include <map>

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
        struct Sized
        {
            HFONT font = nullptr;
            TEXTMETRICW metrics{};
        };

        const Sized& Find(const Font& font) const;

        HFONT fonts_[FontFamilyCount][2] = {};
        TEXTMETRICW metrics_[FontFamilyCount][2]{};
        mutable std::map<std::uint32_t, Sized> sized_;
        HBITMAP scratch_ = nullptr;
        HGDIOBJ oldBitmap_ = nullptr;
        void* bits_ = nullptr;
        int scratchW_ = 0;
        int scratchH_ = 0;
    };
}
