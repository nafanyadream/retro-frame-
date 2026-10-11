#pragma once

#include "image.h"
#include "text.h"

#include <string>
#include <vector>

namespace rf
{
    struct Shades
    {
        Color face = Rgb(0xC0, 0xC0, 0xC0);
        Color hilite = Rgb(0xFF, 0xFF, 0xFF);
        Color light = Rgb(0xDF, 0xDF, 0xDF);
        Color shadow = Rgb(0x80, 0x80, 0x80);
        Color dark = Rgb(0x00, 0x00, 0x00);
    };

    using Glyph = std::vector<const char*>;

    Color Mix(Color a, Color b, double t);
    Color Over(Color dst, Color src);
    int Luminance(Color c);

    Image ScaleNearest(const Image& src, int factor);
    Image ResizeSmooth(const Image& src, int width, int height);
    Image FitInside(const Image& src, int maxWidth, int maxHeight, bool allowUpscale);
    Image Crop(const Image& src, const Rect& r);
    Image MakeIcon16(const Image& src);
    Image Rotate90(const Image& src);
    Image Rotate180(const Image& src);
    Image Rotate270(const Image& src);
    Image FlipHorizontal(const Image& src);
    Image FlipVertical(const Image& src);

    class Painter
    {
    public:
        explicit Painter(Image& target);
        Painter Sub(const Rect& local) const;
        Painter Clipped(const Rect& local) const;

        Image& Target() const { return *target_; }
        Rect LocalClip() const;

        void Fill(int x, int y, int w, int h, Color c);
        void Fill(const Rect& r, Color c) { Fill(r.x, r.y, r.w, r.h, c); }
        void Pixel(int x, int y, Color c);
        void Blend(int x, int y, Color c);
        void HLine(int x, int y, int w, Color c) { Fill(x, y, w, 1, c); }
        void VLine(int x, int y, int h, Color c) { Fill(x, y, 1, h, c); }
        void Frame(const Rect& r, Color c);
        void Draw(const Image& src, int x, int y);
        void Draw(const Image& src, const Rect& from, int x, int y);
        void DrawOpaque(const Image& src, int x, int y);
        void Edge(int x, int y, int w, int h, Color topLeft, Color bottomRight);
        void Edge(const Rect& r, Color topLeft, Color bottomRight) { Edge(r.x, r.y, r.w, r.h, topLeft, bottomRight); }
        void GradientH(int x, int y, int w, int h, Color from, Color to);
        void DrawGlyph(int x, int y, const Glyph& g, Color c);
        void Dither(const Rect& r, Color a, Color b);
        void Dotted(const Rect& r, Color c);

        void Text(int x, int y, const std::wstring& s, const Font& f, Color c);
        void TextEmbossed(int x, int y, const std::wstring& s, const Font& f, const Shades& sh);
        void TextUnderlined(int x, int y, const std::wstring& s, int underline, const Font& f, Color c);

        void Raised(const Rect& r, const Shades& sh, bool fill = true);
        void Pressed(const Rect& r, const Shades& sh, bool fill = true);
        void Sunken(const Rect& r, const Shades& sh, const Color* fill = nullptr);
        void ThinRaised(const Rect& r, const Shades& sh);
        void ThinSunken(const Rect& r, const Shades& sh);
        void WindowEdge(const Rect& r, const Shades& sh);
        void EtchedH(int x, int y, int w, const Shades& sh);
        void EtchedV(int x, int y, int h, const Shades& sh);
        void EtchedFrame(const Rect& r, const Shades& sh);

    private:
        Painter(Image* target, int ox, int oy, const Rect& clip);
        Image* target_;
        int ox_;
        int oy_;
        Rect clip_;
    };

    int GlyphWidth(const Glyph& g);
    int GlyphHeight(const Glyph& g);
}
