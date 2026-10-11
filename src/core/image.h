#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rf
{
    using Color = std::uint32_t;

    constexpr Color Rgb(int r, int g, int b)
    {
        return 0xFF000000u | (Color(r & 255) << 16) | (Color(g & 255) << 8) | Color(b & 255);
    }

    constexpr Color Argb(int a, int r, int g, int b)
    {
        return (Color(a & 255) << 24) | (Color(r & 255) << 16) | (Color(g & 255) << 8) | Color(b & 255);
    }

    constexpr int Red(Color c) { return int((c >> 16) & 255u); }
    constexpr int Green(Color c) { return int((c >> 8) & 255u); }
    constexpr int Blue(Color c) { return int(c & 255u); }
    constexpr int Alpha(Color c) { return int((c >> 24) & 255u); }
    constexpr Color Opaque(Color c) { return c | 0xFF000000u; }

    struct Rect
    {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;

        int Right() const { return x + w; }
        int Bottom() const { return y + h; }
        bool Empty() const { return w <= 0 || h <= 0; }
        bool Contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
        Rect Offset(int dx, int dy) const { return Rect{x + dx, y + dy, w, h}; }
        Rect Inflate(int d) const { return Rect{x - d, y - d, w + 2 * d, h + 2 * d}; }

        Rect Intersect(const Rect& o) const
        {
            const int l = std::max(x, o.x);
            const int t = std::max(y, o.y);
            const int r = std::min(Right(), o.Right());
            const int b = std::min(Bottom(), o.Bottom());
            if (r <= l || b <= t)
            {
                return Rect{l, t, 0, 0};
            }
            return Rect{l, t, r - l, b - t};
        }
    };

    struct Image
    {
        int width = 0;
        int height = 0;
        std::vector<Color> pixels;

        Image() = default;

        Image(int w, int h, Color fill = 0)
            : width(std::max(0, w)), height(std::max(0, h)), pixels(std::size_t(std::max(0, w)) * std::size_t(std::max(0, h)), fill)
        {
        }

        bool Empty() const { return width <= 0 || height <= 0; }
        Color Get(int x, int y) const { return pixels[std::size_t(y) * std::size_t(width) + std::size_t(x)]; }
        void Set(int x, int y, Color c) { pixels[std::size_t(y) * std::size_t(width) + std::size_t(x)] = c; }
        Color* Row(int y) { return pixels.data() + std::size_t(y) * std::size_t(width); }
        const Color* Row(int y) const { return pixels.data() + std::size_t(y) * std::size_t(width); }
    };
}
