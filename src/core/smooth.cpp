#include "smooth.h"

#include <algorithm>
#include <cmath>

namespace rf
{
    namespace
    {
        double RoundDistance(double px, double py, const Box& b, double radius)
        {
            const double hw = b.w / 2.0;
            const double hh = b.h / 2.0;
            const double r = std::clamp(radius, 0.0, std::min(hw, hh));
            const double qx = std::fabs(px - (b.x + hw)) - (hw - r);
            const double qy = std::fabs(py - (b.y + hh)) - (hh - r);
            return std::hypot(std::max(qx, 0.0), std::max(qy, 0.0)) + std::min(std::max(qx, qy), 0.0) - r;
        }

        double SegmentDistance(double px, double py, double x0, double y0, double x1, double y1)
        {
            const double dx = x1 - x0;
            const double dy = y1 - y0;
            const double len = dx * dx + dy * dy;
            const double t = len > 0.0 ? std::clamp(((px - x0) * dx + (py - y0) * dy) / len, 0.0, 1.0) : 0.0;
            return std::hypot(px - (x0 + t * dx), py - (y0 + t * dy));
        }

        void Bounds(const Image& img, double x0, double y0, double x1, double y1, int& ax, int& ay, int& bx, int& by)
        {
            ax = std::max(0, int(std::floor(x0)) - 1);
            ay = std::max(0, int(std::floor(y0)) - 1);
            bx = std::min(img.width, int(std::ceil(x1)) + 1);
            by = std::min(img.height, int(std::ceil(y1)) + 1);
        }

        bool Inside(const std::vector<std::pair<double, double>>& pts, double x, double y)
        {
            bool in = false;
            for (std::size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++)
            {
                const double yi = pts[i].second;
                const double yj = pts[j].second;
                if ((yi > y) != (yj > y))
                {
                    const double cross = pts[j].first + (y - yj) * (pts[i].first - pts[j].first) / (yi - yj);
                    if (x < cross)
                    {
                        in = !in;
                    }
                }
            }
            return in;
        }
    }

    Color WithAlpha(Color c, double alpha)
    {
        const double a = std::clamp(alpha, 0.0, 1.0) * (Alpha(c) == 0 ? 255.0 : double(Alpha(c)));
        return (c & 0x00FFFFFFu) | (Color(std::lround(a)) << 24);
    }

    Color Ramp(const std::vector<std::pair<double, Color>>& stops, double t)
    {
        if (stops.empty())
        {
            return Rgb(0, 0, 0);
        }
        if (t <= stops.front().first)
        {
            return stops.front().second;
        }
        for (std::size_t i = 1; i < stops.size(); ++i)
        {
            if (t <= stops[i].first)
            {
                const double span = stops[i].first - stops[i - 1].first;
                return Mix(stops[i - 1].second, stops[i].second, span > 0.0 ? (t - stops[i - 1].first) / span : 1.0);
            }
        }
        return stops.back().second;
    }

    void BlendPixel(Image& img, int x, int y, Color c, double cover)
    {
        if (x < 0 || y < 0 || x >= img.width || y >= img.height || cover <= 0.0)
        {
            return;
        }
        const double a = std::min(1.0, cover) * (double(Alpha(c)) / 255.0);
        if (a <= 0.0)
        {
            return;
        }
        Color& d = img.pixels[std::size_t(y) * std::size_t(img.width) + std::size_t(x)];
        d = a >= 1.0 ? Opaque(c) : Over(d, (c & 0x00FFFFFFu) | (Color(std::lround(a * 255.0)) << 24));
    }

    void FillRound(Image& img, const Box& b, double radius, const Shade& shade)
    {
        int ax = 0;
        int ay = 0;
        int bx = 0;
        int by = 0;
        Bounds(img, b.x, b.y, b.x + b.w, b.y + b.h, ax, ay, bx, by);
        for (int y = ay; y < by; ++y)
        {
            const double fy = b.h > 0.0 ? std::clamp((y + 0.5 - b.y) / b.h, 0.0, 1.0) : 0.0;
            for (int x = ax; x < bx; ++x)
            {
                const double d = RoundDistance(x + 0.5, y + 0.5, b, radius);
                const double cover = std::clamp(0.5 - d, 0.0, 1.0);
                if (cover > 0.0)
                {
                    const double fx = b.w > 0.0 ? std::clamp((x + 0.5 - b.x) / b.w, 0.0, 1.0) : 0.0;
                    BlendPixel(img, x, y, shade(fx, fy), cover);
                }
            }
        }
    }

    void FillRound(Image& img, const Box& b, double radius, Color c)
    {
        FillRound(img, b, radius, [c](double, double) { return c; });
    }

    void StrokeRound(Image& img, const Box& b, double radius, double width, Color c)
    {
        int ax = 0;
        int ay = 0;
        int bx = 0;
        int by = 0;
        Bounds(img, b.x, b.y, b.x + b.w, b.y + b.h, ax, ay, bx, by);
        for (int y = ay; y < by; ++y)
        {
            for (int x = ax; x < bx; ++x)
            {
                const double d = RoundDistance(x + 0.5, y + 0.5, b, radius);
                const double outer = std::clamp(0.5 - d, 0.0, 1.0);
                const double inner = std::clamp(0.5 - (d + width), 0.0, 1.0);
                BlendPixel(img, x, y, c, outer - inner);
            }
        }
    }

    void FillDisc(Image& img, double cx, double cy, double r, const Shade& shade)
    {
        FillRound(img, Box{cx - r, cy - r, 2.0 * r, 2.0 * r}, r, shade);
    }

    void FillDisc(Image& img, double cx, double cy, double r, Color c)
    {
        FillRound(img, Box{cx - r, cy - r, 2.0 * r, 2.0 * r}, r, c);
    }

    void StrokeLine(Image& img, double x0, double y0, double x1, double y1, double width, Color c)
    {
        const double h = width / 2.0;
        int ax = 0;
        int ay = 0;
        int bx = 0;
        int by = 0;
        Bounds(img, std::min(x0, x1) - h, std::min(y0, y1) - h, std::max(x0, x1) + h, std::max(y0, y1) + h, ax, ay, bx, by);
        for (int y = ay; y < by; ++y)
        {
            for (int x = ax; x < bx; ++x)
            {
                const double d = SegmentDistance(x + 0.5, y + 0.5, x0, y0, x1, y1) - h;
                BlendPixel(img, x, y, c, std::clamp(0.5 - d, 0.0, 1.0));
            }
        }
    }

    void StrokeArc(Image& img, double cx, double cy, double r, double a0, double a1, double width, Color c)
    {
        const double h = width / 2.0;
        const double pi = 3.14159265358979;
        if (a1 < a0)
        {
            std::swap(a0, a1);
        }
        const double ex0 = cx + r * std::cos(a0);
        const double ey0 = cy + r * std::sin(a0);
        const double ex1 = cx + r * std::cos(a1);
        const double ey1 = cy + r * std::sin(a1);
        int ax = 0;
        int ay = 0;
        int bx = 0;
        int by = 0;
        Bounds(img, cx - r - h, cy - r - h, cx + r + h, cy + r + h, ax, ay, bx, by);
        for (int y = ay; y < by; ++y)
        {
            for (int x = ax; x < bx; ++x)
            {
                const double px = x + 0.5 - cx;
                const double py = y + 0.5 - cy;
                double a = std::atan2(py, px);
                while (a < a0)
                {
                    a += 2.0 * pi;
                }
                double d = 0.0;
                if (a <= a1)
                {
                    d = std::fabs(std::hypot(px, py) - r) - h;
                }
                else
                {
                    d = std::min(std::hypot(x + 0.5 - ex0, y + 0.5 - ey0), std::hypot(x + 0.5 - ex1, y + 0.5 - ey1)) - h;
                }
                BlendPixel(img, x, y, c, std::clamp(0.5 - d, 0.0, 1.0));
            }
        }
    }

    void FillPolygon(Image& img, const std::vector<std::pair<double, double>>& points, Color c)
    {
        if (points.size() < 3)
        {
            return;
        }
        double x0 = points[0].first;
        double y0 = points[0].second;
        double x1 = x0;
        double y1 = y0;
        for (const auto& p : points)
        {
            x0 = std::min(x0, p.first);
            y0 = std::min(y0, p.second);
            x1 = std::max(x1, p.first);
            y1 = std::max(y1, p.second);
        }
        int ax = 0;
        int ay = 0;
        int bx = 0;
        int by = 0;
        Bounds(img, x0, y0, x1, y1, ax, ay, bx, by);
        for (int y = ay; y < by; ++y)
        {
            for (int x = ax; x < bx; ++x)
            {
                int hits = 0;
                for (int sy = 0; sy < 4; ++sy)
                {
                    for (int sx = 0; sx < 4; ++sx)
                    {
                        hits += Inside(points, x + (sx + 0.5) / 4.0, y + (sy + 0.5) / 4.0) ? 1 : 0;
                    }
                }
                BlendPixel(img, x, y, c, hits / 16.0);
            }
        }
    }

    void FillGlow(Image& img, double cx, double cy, double rx, double ry, Color c)
    {
        int ax = 0;
        int ay = 0;
        int bx = 0;
        int by = 0;
        Bounds(img, cx - rx, cy - ry, cx + rx, cy + ry, ax, ay, bx, by);
        for (int y = ay; y < by; ++y)
        {
            for (int x = ax; x < bx; ++x)
            {
                const double dx = (x + 0.5 - cx) / std::max(1.0, rx);
                const double dy = (y + 0.5 - cy) / std::max(1.0, ry);
                const double q = 1.0 - (dx * dx + dy * dy);
                if (q > 0.0)
                {
                    BlendPixel(img, x, y, c, q * q);
                }
            }
        }
    }
}
