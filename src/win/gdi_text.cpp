#include "gdi_text.h"

#include "../core/paint.h"

#include <algorithm>
#include <cstdint>

namespace rf::win
{
    namespace
    {
        HFONT MakeFont(const wchar_t* face, bool bold, BYTE charset, int height = -11, DWORD pitch = DEFAULT_PITCH | FF_SWISS, DWORD quality = NONANTIALIASED_QUALITY)
        {
            return CreateFontW(height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, charset, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, quality, pitch, face);
        }

        bool HasCyrillic(HDC dc, HFONT font)
        {
            HGDIOBJ old = SelectObject(dc, font);
            WORD index = 0xFFFF;
            const wchar_t probe = 0x0436;
            const bool ok = GetGlyphIndicesW(dc, &probe, 1, &index, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR && index != 0xFFFF;
            SelectObject(dc, old);
            return ok;
        }
    }

    GdiTextEngine::GdiTextEngine()
    {
        dc_ = CreateCompatibleDC(nullptr);
        fonts_[0][0] = MakeFont(L"MS Sans Serif", false, RUSSIAN_CHARSET);
        fonts_[0][1] = MakeFont(L"MS Sans Serif", true, RUSSIAN_CHARSET);
        fonts_[1][0] = MakeFont(L"Tahoma", false, DEFAULT_CHARSET);
        fonts_[1][1] = MakeFont(L"Tahoma", true, DEFAULT_CHARSET);
        fonts_[2][0] = MakeFont(L"Trebuchet MS", false, DEFAULT_CHARSET, -13);
        fonts_[2][1] = MakeFont(L"Trebuchet MS", true, DEFAULT_CHARSET, -13);
        for (int b = 0; b < 2; ++b)
        {
            fonts_[3][b] = MakeFont(L"Fixedsys", b == 1, RUSSIAN_CHARSET, 15, FIXED_PITCH | FF_MODERN);
            if (!HasCyrillic(dc_, fonts_[3][b]))
            {
                DeleteObject(fonts_[3][b]);
                fonts_[3][b] = MakeFont(L"Lucida Console", b == 1, DEFAULT_CHARSET, -12, FIXED_PITCH | FF_MODERN);
            }
            fonts_[4][b] = MakeFont(L"Lucida Console", b == 1, DEFAULT_CHARSET, -12, FIXED_PITCH | FF_MODERN);
            fonts_[5][b] = MakeFont(L"Segoe UI", b == 1, DEFAULT_CHARSET, -12);
        }
        for (int f = 0; f < FontFamilyCount; ++f)
        {
            for (int b = 0; b < 2; ++b)
            {
                HGDIOBJ old = SelectObject(dc_, fonts_[f][b]);
                GetTextMetricsW(dc_, &metrics_[f][b]);
                SelectObject(dc_, old);
            }
        }
        EnsureScratch(512, 32);
    }

    GdiTextEngine::~GdiTextEngine()
    {
        if (oldBitmap_ != nullptr)
        {
            SelectObject(dc_, oldBitmap_);
        }
        if (scratch_ != nullptr)
        {
            DeleteObject(scratch_);
        }
        for (auto& row : fonts_)
        {
            for (HFONT f : row)
            {
                if (f != nullptr)
                {
                    DeleteObject(f);
                }
            }
        }
        for (auto& entry : sized_)
        {
            if (entry.second.font != nullptr)
            {
                DeleteObject(entry.second.font);
            }
        }
        if (dc_ != nullptr)
        {
            DeleteDC(dc_);
        }
    }

    const GdiTextEngine::Sized& GdiTextEngine::Find(const Font& font) const
    {
        const int family = std::clamp(int(font.family), 0, FontFamilyCount - 1);
        const int size = std::clamp(font.size, 0, 400);
        const std::uint32_t key = std::uint32_t(family) | (font.bold ? 16u : 0u) | (font.smooth ? 32u : 0u) | (std::uint32_t(size) << 8);
        const auto it = sized_.find(key);
        if (it != sized_.end())
        {
            return it->second;
        }
        static const wchar_t* const faces[FontFamilyCount] = {L"Microsoft Sans Serif", L"Tahoma", L"Trebuchet MS", L"Lucida Console", L"Lucida Console", L"Segoe UI"};
        static const int heights[FontFamilyCount] = {-11, -11, -13, -12, -12, -12};
        const bool fixed = family == 3 || family == 4;
        Sized s;
        s.font = MakeFont(faces[family], font.bold, DEFAULT_CHARSET, size > 0 ? -size : heights[family], fixed ? FIXED_PITCH | FF_MODERN : DEFAULT_PITCH | FF_SWISS,
                          font.smooth ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY);
        HGDIOBJ old = SelectObject(dc_, s.font);
        GetTextMetricsW(dc_, &s.metrics);
        SelectObject(dc_, old);
        if (sized_.size() > 64)
        {
            for (auto& entry : sized_)
            {
                DeleteObject(entry.second.font);
            }
            sized_.clear();
        }
        return sized_.emplace(key, s).first->second;
    }

    HFONT GdiTextEngine::Pick(const Font& font) const
    {
        if (font.size > 0 || font.smooth)
        {
            return Find(font).font;
        }
        return fonts_[std::clamp(int(font.family), 0, FontFamilyCount - 1)][font.bold ? 1 : 0];
    }

    const TEXTMETRICW& GdiTextEngine::Metrics(const Font& font) const
    {
        if (font.size > 0 || font.smooth)
        {
            return Find(font).metrics;
        }
        return metrics_[std::clamp(int(font.family), 0, FontFamilyCount - 1)][font.bold ? 1 : 0];
    }

    void GdiTextEngine::EnsureScratch(int w, int h)
    {
        if (w <= scratchW_ && h <= scratchH_)
        {
            return;
        }
        const int nw = std::max(w, scratchW_);
        const int nh = std::max(h, scratchH_);
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = nw;
        bi.bmiHeader.biHeight = -nh;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(dc_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bmp == nullptr)
        {
            return;
        }
        if (oldBitmap_ != nullptr)
        {
            SelectObject(dc_, oldBitmap_);
        }
        if (scratch_ != nullptr)
        {
            DeleteObject(scratch_);
        }
        scratch_ = bmp;
        bits_ = bits;
        oldBitmap_ = SelectObject(dc_, scratch_);
        scratchW_ = nw;
        scratchH_ = nh;
    }

    int GdiTextEngine::Width(const std::wstring& text, const Font& font)
    {
        if (text.empty())
        {
            return 0;
        }
        HGDIOBJ old = SelectObject(dc_, Pick(font));
        SIZE size{};
        GetTextExtentPoint32W(dc_, text.c_str(), int(text.size()), &size);
        SelectObject(dc_, old);
        return int(size.cx);
    }

    int GdiTextEngine::Height(const Font& font)
    {
        return int(Metrics(font).tmHeight);
    }

    int GdiTextEngine::Ascent(const Font& font)
    {
        return int(Metrics(font).tmAscent);
    }

    void GdiTextEngine::Draw(Image& target, const Rect& clip, int x, int y, const std::wstring& text, const Font& font, Color color)
    {
        if (text.empty())
        {
            return;
        }
        const int w = Width(text, font) + Metrics(font).tmOverhang + 2;
        const int h = Height(font);
        const Rect area = Rect{x, y, w, h}.Intersect(clip).Intersect(Rect{0, 0, target.width, target.height});
        if (area.Empty())
        {
            return;
        }
        EnsureScratch(w, h);
        if (bits_ == nullptr || w > scratchW_ || h > scratchH_)
        {
            return;
        }
        HGDIOBJ old = SelectObject(dc_, Pick(font));
        SetTextColor(dc_, RGB(255, 255, 255));
        SetBkColor(dc_, RGB(0, 0, 0));
        SetBkMode(dc_, OPAQUE);
        RECT rc{0, 0, w, h};
        ExtTextOutW(dc_, 0, 0, ETO_OPAQUE, &rc, text.c_str(), UINT(text.size()), nullptr);
        SelectObject(dc_, old);
        GdiFlush();
        const auto* src = static_cast<const std::uint32_t*>(bits_);
        const unsigned alpha = Alpha(color) == 0 ? 255u : Alpha(color);
        for (int yy = area.y; yy < area.Bottom(); ++yy)
        {
            const std::uint32_t* row = src + std::size_t(yy - y) * std::size_t(scratchW_);
            Color* dst = target.Row(yy);
            for (int xx = area.x; xx < area.Right(); ++xx)
            {
                const std::uint32_t v = row[xx - x];
                if (font.smooth)
                {
                    const unsigned cover = std::max({(v >> 16) & 255u, (v >> 8) & 255u, v & 255u});
                    if (cover > 0)
                    {
                        const unsigned a = cover * alpha / 255u;
                        dst[xx] = Over(dst[xx], (color & 0x00FFFFFFu) | (Color(a) << 24));
                    }
                }
                else if (((v >> 16) & 255u) > 127u || ((v >> 8) & 255u) > 127u)
                {
                    dst[xx] = Opaque(color);
                }
            }
        }
    }
}
