#include "wmp12.h"

#include "music.h"
#include "smooth.h"

#include <cmath>

namespace rf
{
    namespace
    {
        constexpr int TopStrip = 30;
        constexpr int BottomStrip = 72;
        constexpr double Pi = 3.14159265358979;

        struct Frame
        {
            int left = 0;
            int top = 0;
            int right = 0;
            int bottom = 0;
            double radius = 0.0;
        };

        Frame FrameOf(int frame)
        {
            if (frame == int(Wmp12Frame::Modern))
            {
                return Frame{1, 32, 1, 1, 8.0};
            }
            return Frame{8, 30, 8, 8, 6.0};
        }

        Font Segoe(double px, bool bold = false)
        {
            return Font{FontFamily::Segoe, bold, std::max(8, int(std::lround(px))), true};
        }

        void GlowText(Painter& p, int x, int y, const std::wstring& text, const Font& f, Color ink, Color glow, int spread)
        {
            for (int dy = -spread; dy <= spread; ++dy)
            {
                for (int dx = -spread; dx <= spread; ++dx)
                {
                    if (dx != 0 || dy != 0)
                    {
                        p.Text(x + dx, y + dy, text, f, glow);
                    }
                }
            }
            p.Text(x, y, text, f, ink);
        }

        void DrawSevenFrame(Image& img, Painter& p, const Wmp12Look& look, double s, int wd, int hd, Wmp12Spots* spots)
        {
            const Frame fr = FrameOf(look.frame);
            const Box whole{0.0, 0.0, double(img.width), double(img.height)};
            const bool on = look.active;
            const Color top = on ? Rgb(0xC3, 0xD7, 0xEE) : Rgb(0xD9, 0xE4, 0xF0);
            const Color mid = on ? Rgb(0xA6, 0xC2, 0xE2) : Rgb(0xC8, 0xD6, 0xE6);
            const Color low = on ? Rgb(0x9C, 0xB9, 0xDA) : Rgb(0xC2, 0xD1, 0xE2);
            const double titleY = fr.top * s / img.height;
            FillRound(img, whole, fr.radius * s, [&](double fx, double fy) {
                Color c = fy < titleY ? Mix(top, mid, fy / std::max(1e-6, titleY)) : Mix(mid, low, (fy - titleY) / std::max(1e-6, 1.0 - titleY));
                const double band = std::fmod(fx * img.width / s * 0.9 + fy * img.height / s * 0.5, 160.0);
                if (band > 40.0 && band < 70.0)
                {
                    c = Mix(c, Rgb(0xFF, 0xFF, 0xFF), 0.16 * std::sin((band - 40.0) / 30.0 * Pi));
                }
                return c;
            });
            StrokeRound(img, whole, fr.radius * s, std::max(1.0, s * 0.9), WithAlpha(Rgb(0x2B, 0x40, 0x58), 0.9));
            const Box inner{s, s, img.width - 2.0 * s, img.height - 2.0 * s};
            StrokeRound(img, inner, (fr.radius - 1.0) * s, std::max(1.0, s * 0.8), WithAlpha(Rgb(0xFF, 0xFF, 0xFF), 0.55));
            const Box client{fr.left * s - s, fr.top * s - s, (wd - fr.left - fr.right) * s + 2.0 * s, (hd - fr.top - fr.bottom) * s + 2.0 * s};
            StrokeRound(img, client, 0.0, s, WithAlpha(Rgb(0x4E, 0x64, 0x80), 0.9));
            DrawWmp12Icon(img, 8.0 * s, 7.0 * s, 16.0 * s);
            const Font cf = Segoe(12.0 * s);
            const int ty = int(std::lround((fr.top * s - Text().Height(cf)) / 2.0));
            const int bw = 94;
            const std::wstring caption = Ellipsize(look.caption, cf, int((wd - 40 - bw - 12) * s));
            GlowText(p, int(30 * s), ty, caption, cf, on ? Rgb(0x00, 0x00, 0x00) : Rgb(0x50, 0x5A, 0x66), WithAlpha(Rgb(0xFF, 0xFF, 0xFF), 0.32),
                     std::max(1, int(std::lround(s))));
            const double gx = (wd - 6 - bw) * s;
            const double gy = 0.0;
            const double gh = 20.0 * s;
            const Box min{gx, gy, 26.0 * s, gh};
            const Box max{gx + 26.0 * s, gy, 25.0 * s, gh};
            const Box close{gx + 51.0 * s, gy, 43.0 * s, gh};
            const Color edge = WithAlpha(Rgb(0x2E, 0x43, 0x5E), 0.95);
            for (const Box& b : {min, max})
            {
                FillRound(img, b, 3.0 * s, [&](double, double fy) {
                    return fy < 0.5 ? Mix(Rgb(0xEC, 0xF2, 0xFA), Rgb(0xC9, 0xD8, 0xEA), fy * 2.0) : Mix(Rgb(0xB4, 0xC9, 0xE2), Rgb(0xD0, 0xE0, 0xF2), (fy - 0.5) * 2.0);
                });
                StrokeRound(img, b, 3.0 * s, std::max(1.0, s * 0.8), edge);
            }
            FillRound(img, close, 3.0 * s, [&](double, double fy) {
                return fy < 0.5 ? Mix(Rgb(0xEA, 0xA8, 0x9C), Rgb(0xD0, 0x62, 0x52), fy * 2.0) : Mix(Rgb(0xC2, 0x3C, 0x2A), Rgb(0xE0, 0x7A, 0x5E), (fy - 0.5) * 2.0);
            });
            StrokeRound(img, close, 3.0 * s, std::max(1.0, s * 0.8), WithAlpha(Rgb(0x5A, 0x1E, 0x16), 0.95));
            const Color white = Rgb(0xFF, 0xFF, 0xFF);
            const Color shadow = WithAlpha(Rgb(0x1E, 0x2C, 0x40), 0.85);
            const double mcx = min.x + min.w / 2.0;
            const double mcy = gy + gh / 2.0;
            FillRound(img, Box{mcx - 5.0 * s, mcy + 1.0 * s, 10.0 * s, 4.0 * s}, 0.5 * s, shadow);
            FillRound(img, Box{mcx - 4.0 * s, mcy + 1.5 * s, 8.0 * s, 3.0 * s}, 0.0, white);
            const double xcx = max.x + max.w / 2.0;
            StrokeRound(img, Box{xcx - 5.5 * s, mcy - 5.0 * s, 11.0 * s, 9.0 * s}, 0.5 * s, 3.0 * s, shadow);
            StrokeRound(img, Box{xcx - 4.5 * s, mcy - 4.0 * s, 9.0 * s, 7.0 * s}, 0.0, 2.0 * s, white);
            const double ccx = close.x + close.w / 2.0;
            StrokeLine(img, ccx - 4.5 * s, mcy - 4.0 * s, ccx + 4.5 * s, mcy + 4.0 * s, 3.4 * s, shadow);
            StrokeLine(img, ccx - 4.5 * s, mcy + 4.0 * s, ccx + 4.5 * s, mcy - 4.0 * s, 3.4 * s, shadow);
            StrokeLine(img, ccx - 4.0 * s, mcy - 3.5 * s, ccx + 4.0 * s, mcy + 3.5 * s, 2.0 * s, white);
            StrokeLine(img, ccx - 4.0 * s, mcy + 3.5 * s, ccx + 4.0 * s, mcy - 3.5 * s, 2.0 * s, white);
            if (spots != nullptr)
            {
                spots->close = Rect{int(close.x), int(close.y), int(close.w), int(close.h)};
                spots->minimize = Rect{int(min.x), int(min.y), int(min.w), int(min.h)};
            }
        }

        void DrawModernFrame(Image& img, const Wmp12Look& look, double s, int wd, Wmp12Spots* spots)
        {
            const Frame fr = FrameOf(look.frame);
            const Box whole{0.0, 0.0, double(img.width), double(img.height)};
            FillRound(img, whole, fr.radius * s, Rgb(0xFF, 0xFF, 0xFF));
            DrawWmp12Icon(img, 10.0 * s, 9.0 * s, 16.0 * s);
            const Color ink = look.active ? Rgb(0x1A, 0x1A, 0x1A) : Rgb(0x99, 0x99, 0x99);
            const double bh = (fr.top - 1) * s;
            const double y0 = 1.0 * s;
            const Box close{(wd - 1 - 46) * s, y0, 46.0 * s, bh};
            const Box max{(wd - 1 - 92) * s, y0, 46.0 * s, bh};
            const Box min{(wd - 1 - 138) * s, y0, 46.0 * s, bh};
            const double cy = y0 + bh / 2.0;
            const double line = std::max(1.0, s);
            StrokeLine(img, min.x + min.w / 2.0 - 5.0 * s, cy, min.x + min.w / 2.0 + 5.0 * s, cy, line, ink);
            StrokeRound(img, Box{max.x + max.w / 2.0 - 5.0 * s, cy - 5.0 * s, 10.0 * s, 10.0 * s}, 1.5 * s, line, ink);
            StrokeLine(img, close.x + close.w / 2.0 - 5.0 * s, cy - 5.0 * s, close.x + close.w / 2.0 + 5.0 * s, cy + 5.0 * s, line, ink);
            StrokeLine(img, close.x + close.w / 2.0 - 5.0 * s, cy + 5.0 * s, close.x + close.w / 2.0 + 5.0 * s, cy - 5.0 * s, line, ink);
            if (spots != nullptr)
            {
                spots->close = Rect{int(close.x), int(close.y), int(close.w), int(close.h)};
                spots->minimize = Rect{int(min.x), int(min.y), int(min.w), int(min.h)};
            }
        }

        void Triangle(Image& img, double x0, double y0, double x1, double y1, double x2, double y2, Color c)
        {
            FillPolygon(img, {{x0, y0}, {x1, y1}, {x2, y2}}, c);
        }
    }

    void Wmp12Chrome(int frame, int* chromeW, int* chromeH, int* contentX, int* contentY)
    {
        const Frame fr = FrameOf(frame);
        *chromeW = fr.left + fr.right;
        *chromeH = fr.top + fr.bottom + TopStrip + BottomStrip;
        *contentX = fr.left;
        *contentY = fr.top + TopStrip;
    }

    void DrawWmp12Icon(Image& img, double x, double y, double size)
    {
        const double r = size / 2.0;
        const double cx = x + r;
        const double cy = y + r;
        FillDisc(img, cx, cy, r, [](double, double fy) { return Mix(Rgb(0xFF, 0xC4, 0x5C), Rgb(0xE0, 0x5C, 0x00), fy); });
        FillDisc(img, cx, cy, r * 0.68, [](double, double fy) { return Mix(Rgb(0x7C, 0xC4, 0xFF), Rgb(0x12, 0x4E, 0xB8), fy); });
        Triangle(img, cx - r * 0.24, cy - r * 0.38, cx - r * 0.24, cy + r * 0.38, cx + r * 0.42, cy, Rgb(0xFF, 0xFF, 0xFF));
    }

    namespace
    {
        void PaintWmp12(Image& img, const Wmp12Look& look, int contentW, int contentH, double s, bool base, bool live, Rect* content, Wmp12Spots* spots)
        {
            int chromeW = 0;
            int chromeH = 0;
            int cx0 = 0;
            int cy0 = 0;
            Wmp12Chrome(look.frame, &chromeW, &chromeH, &cx0, &cy0);
            const Frame fr = FrameOf(look.frame);
            const int wd = contentW + chromeW;
            const int hd = contentH + chromeH;
            Painter p(img);
            const double clx = fr.left * s;
            const double cly = fr.top * s;
            const double cw = contentW;
            const double ch = contentH + TopStrip + BottomStrip;
            const auto X = [&](double v) { return clx + v * s; };
            const auto Y = [&](double v) { return cly + v * s; };
            const Color white = Rgb(0xF2, 0xF4, 0xF6);
            const double seekY = ch - BottomStrip + 6.0;
            const double pc = ch - 25.0;
            const double mid = cw / 2.0;
            const double room = cw - 16.0;
            int level = 0;
            const double halves[4] = {154.0, 154.0, 84.0, 86.0};
            while (level < 3 && 2.0 * halves[level] > room)
            {
                ++level;
            }
            const bool shuffle = level == 0;
            const bool slider = level <= 1;
            const bool volume = level <= 2;
            const double leftW = 12.0 + 44.0 + 30.0 + (shuffle ? 68.0 : 0.0);
            const double rightW = 12.0 + 44.0 + (volume ? 28.0 : 0.0) + (slider ? 70.0 : 0.0);
            const double half = std::max(leftW, rightW) + 25.0;
            const double dcx = X(mid);
            const double dcy = Y(pc);
            if (content != nullptr)
            {
                *content = Rect{int(cx0 * s), int(cy0 * s), int(contentW * s), int(contentH * s)};
            }
            if (spots != nullptr)
            {
                spots->play = Rect{int(dcx - 21.0 * s), int(dcy - 21.0 * s), int(42.0 * s), int(42.0 * s)};
                spots->seek = Rect{int(X(9.0)), int(Y(seekY)), int((cw - 18.0) * s), int(6.0 * s)};
            }
            if (base)
            {
                if (look.frame == int(Wmp12Frame::Modern))
                {
                    DrawModernFrame(img, look, s, wd, spots);
                }
                else
                {
                    DrawSevenFrame(img, p, look, s, wd, hd, spots);
                }
                const Box client{clx, cly, cw * s, ch * s};
                const double bottomRadius = look.frame == int(Wmp12Frame::Modern) ? (fr.radius - 1.0) * s : 0.0;
                FillRound(img, client, 0.0, [&](double, double fy) {
                    return Ramp({{0.0, Rgb(0x01, 0x05, 0x0B)},
                                 {0.22, Rgb(0x03, 0x16, 0x2B)},
                                 {0.58, Rgb(0x04, 0x22, 0x3E)},
                                 {0.84, Rgb(0x02, 0x0E, 0x1B)},
                                 {1.0, Rgb(0x00, 0x02, 0x05)}},
                                fy);
                });
                FillGlow(img, clx + cw * s * 0.56, cly + ch * s * 0.42, cw * s * 0.6, ch * s * 0.36, WithAlpha(Rgb(0x16, 0x4C, 0x78), 0.45));
                FillGlow(img, clx + cw * s * 0.3, cly + ch * s * 0.3, cw * s * 0.3, ch * s * 0.2, WithAlpha(Rgb(0x1E, 0x5C, 0x8C), 0.18));
                if (bottomRadius > 0.0)
                {
                    for (int y = int(img.height - bottomRadius); y < img.height; ++y)
                    {
                        for (int x = 0; x < img.width; ++x)
                        {
                            const bool corner = x < bottomRadius || x >= img.width - bottomRadius;
                            if (!corner)
                            {
                                continue;
                            }
                            const double ox = x < bottomRadius ? bottomRadius - (x + 0.5) : (x + 0.5) - (img.width - bottomRadius);
                            const double oy = (y + 0.5) - (img.height - bottomRadius);
                            const double d = std::hypot(ox, oy) - bottomRadius + s;
                            if (d > 0.5)
                            {
                                img.Set(x, y, Color(0));
                            }
                        }
                    }
                }
                if (look.frame == int(Wmp12Frame::Modern))
                {
                    StrokeRound(img, Box{0.0, 0.0, double(img.width), double(img.height)}, fr.radius * s, std::max(1.0, s), WithAlpha(Rgb(0x5A, 0x5A, 0x5A), 0.75));
                }
                const Font titleFont = Segoe(12.5 * s);
                const int titleH = Text().Height(titleFont);
                Painter cp = p.Clipped(Rect{int(clx), int(cly), int(cw * s), int(TopStrip * s)});
                cp.Text(int(X(10)), int(Y((TopStrip - titleH / s) / 2.0)), Ellipsize(look.title, titleFont, int((cw - 50) * s)), titleFont, white);
                const double lx = cw - 26;
                for (int i = 0; i < 2; ++i)
                {
                    for (int j = 0; j < 2; ++j)
                    {
                        FillRound(img, Box{X(lx + i * 5.0), Y(9.0 + j * 5.0), 3.6 * s, 3.6 * s}, 0.6 * s, WithAlpha(white, 0.9));
                    }
                }
                StrokeLine(img, X(lx + 11.0), Y(14.0), X(lx + 16.0), Y(14.0), 1.4 * s, WithAlpha(white, 0.9));
                Triangle(img, X(lx + 10.0), Y(14.0), X(lx + 12.6), Y(11.6), X(lx + 12.6), Y(16.4), WithAlpha(white, 0.9));
                const Box pill{X(mid - half), Y(pc - 17.0), 2.0 * half * s, 34.0 * s};
                FillRound(img, Box{pill.x - s, pill.y - s, pill.w + 2.0 * s, pill.h + 2.0 * s}, 18.0 * s, WithAlpha(Rgb(0x10, 0x11, 0x12), 0.9));
                FillRound(img, pill, 17.0 * s, [](double, double fy) { return Mix(Rgb(0x4C, 0x50, 0x55), Rgb(0x2C, 0x2F, 0x32), fy); });
                StrokeRound(img, Box{pill.x + s, pill.y + s, pill.w - 2.0 * s, pill.h - 2.0 * s}, 16.0 * s, std::max(1.0, 0.8 * s),
                            WithAlpha(Rgb(0x7A, 0x7E, 0x82), 0.35));
                const Color segEdge = WithAlpha(Rgb(0x6E, 0x72, 0x77), 0.55);
                const Box prevSeg{X(mid - 23.0 - 44.0), Y(pc - 13.0), 50.0 * s, 26.0 * s};
                const Box nextSeg{X(mid + 17.0), Y(pc - 13.0), 50.0 * s, 26.0 * s};
                for (const Box& b : {prevSeg, nextSeg})
                {
                    FillRound(img, b, 13.0 * s, [](double, double fy) { return Mix(Rgb(0x3E, 0x41, 0x45), Rgb(0x2A, 0x2C, 0x2F), fy); });
                    StrokeRound(img, b, 13.0 * s, std::max(1.0, 0.8 * s), segEdge);
                }
                const double pvx = mid - 23.0 - 21.0;
                FillRound(img, Box{X(pvx - 8.0), Y(pc - 5.5), 2.2 * s, 11.0 * s}, 0.4 * s, white);
                Triangle(img, X(pvx - 6.0), Y(pc), X(pvx + 0.5), Y(pc - 5.5), X(pvx + 0.5), Y(pc + 5.5), white);
                Triangle(img, X(pvx + 0.0), Y(pc), X(pvx + 6.5), Y(pc - 5.5), X(pvx + 6.5), Y(pc + 5.5), white);
                const double nvx = mid + 23.0 + 21.0;
                Triangle(img, X(nvx - 6.5), Y(pc - 5.5), X(nvx - 6.5), Y(pc + 5.5), X(nvx), Y(pc), white);
                Triangle(img, X(nvx - 0.5), Y(pc - 5.5), X(nvx - 0.5), Y(pc + 5.5), X(nvx + 6.0), Y(pc), white);
                FillRound(img, Box{X(nvx + 5.8), Y(pc - 5.5), 2.2 * s, 11.0 * s}, 0.4 * s, white);
                const double stopX = mid - 23.0 - 44.0 - 18.0;
                FillRound(img, Box{X(stopX - 5.0), Y(pc - 5.0), 10.0 * s, 10.0 * s}, 1.2 * s, white);
                if (shuffle)
                {
                    const double sepX = stopX - 20.0;
                    StrokeLine(img, X(sepX), Y(pc - 9.0), X(sepX), Y(pc + 9.0), std::max(1.0, s), WithAlpha(Rgb(0x1C, 0x1D, 0x1F), 0.9));
                    StrokeLine(img, X(sepX + 1.0), Y(pc - 9.0), X(sepX + 1.0), Y(pc + 9.0), std::max(1.0, s), WithAlpha(Rgb(0x6A, 0x6E, 0x72), 0.6));
                    const double rx = sepX - 18.0;
                    StrokeArc(img, X(rx), Y(pc), 5.5 * s, -0.35 * Pi, 1.2 * Pi, 1.7 * s, white);
                    Triangle(img, X(rx + 2.6), Y(pc - 8.6), X(rx + 6.8), Y(pc - 4.4), X(rx + 1.4), Y(pc - 3.6), white);
                    const double sx = rx - 30.0;
                    StrokeLine(img, X(sx - 7.0), Y(pc - 4.0), X(sx + 4.5), Y(pc + 4.0), 1.7 * s, white);
                    StrokeLine(img, X(sx - 7.0), Y(pc + 4.0), X(sx + 4.5), Y(pc - 4.0), 1.7 * s, white);
                    Triangle(img, X(sx + 3.2), Y(pc + 1.0), X(sx + 8.2), Y(pc + 4.6), X(sx + 3.2), Y(pc + 7.4), white);
                    Triangle(img, X(sx + 3.2), Y(pc - 7.4), X(sx + 8.2), Y(pc - 4.6), X(sx + 3.2), Y(pc - 1.0), white);
                }
                if (volume)
                {
                    const double vx = mid + 23.0 + 44.0 + 18.0;
                    FillPolygon(img,
                                {{X(vx - 6.0), Y(pc - 2.6)},
                                 {X(vx - 3.0), Y(pc - 2.6)},
                                 {X(vx + 1.0), Y(pc - 6.4)},
                                 {X(vx + 1.0), Y(pc + 6.4)},
                                 {X(vx - 3.0), Y(pc + 2.6)},
                                 {X(vx - 6.0), Y(pc + 2.6)}},
                                white);
                    StrokeArc(img, X(vx + 1.0), Y(pc), 4.2 * s, -0.27 * Pi, 0.27 * Pi, 1.4 * s, white);
                    StrokeArc(img, X(vx + 1.0), Y(pc), 7.4 * s, -0.27 * Pi, 0.27 * Pi, 1.4 * s, white);
                    if (slider)
                    {
                        const double s0 = vx + 14.0;
                        const double s1 = s0 + 62.0;
                        const double knob = s0 + (s1 - s0) * 0.62;
                        StrokeLine(img, X(s0), Y(pc), X(s1), Y(pc), 2.2 * s, Rgb(0x8C, 0x91, 0x97));
                        StrokeLine(img, X(s0), Y(pc), X(knob), Y(pc), 2.2 * s, Rgb(0x3E, 0x90, 0xEA));
                        FillDisc(img, X(knob), Y(pc), 5.6 * s, Rgb(0x16, 0x46, 0x8E));
                        FillDisc(img, X(knob), Y(pc), 4.6 * s, [](double, double fy) { return Mix(Rgb(0xBE, 0xE2, 0xFF), Rgb(0x2A, 0x7A, 0xE0), fy); });
                    }
                    else
                    {
                        Triangle(img, X(vx + 12.0), Y(pc - 2.0), X(vx + 18.0), Y(pc - 2.0), X(vx + 15.0), Y(pc + 2.0), white);
                    }
                }
                const double fx = cw - 26.0;
                const double fy = ch - 34.0;
                if (mid + half + 6.0 < fx)
                {
                    StrokeRound(img, Box{X(fx), Y(fy), 12.0 * s, 12.0 * s}, 1.0 * s, 1.3 * s, WithAlpha(white, 0.85));
                    StrokeLine(img, X(fx + 4.0), Y(fy + 8.0), X(fx + 10.0), Y(fy + 2.0), 1.5 * s, WithAlpha(white, 0.85));
                    Triangle(img, X(fx + 6.0), Y(fy + 1.6), X(fx + 10.6), Y(fy + 1.6), X(fx + 10.6), Y(fy + 6.2), WithAlpha(white, 0.85));
                }
            }
            if (!live)
            {
                return;
            }
            const Box groove{X(8.0), Y(seekY), (cw - 16.0) * s, 6.0 * s};
            FillRound(img, groove, 3.0 * s, Rgb(0x05, 0x0D, 0x16));
            const double fillW = (cw - 18.0) * std::clamp(look.position, 0.0, 1.0);
            if (fillW > 0.5)
            {
                FillRoundRows(img, Box{X(9.0), Y(seekY + 1.0), fillW * s, 4.0 * s}, 2.0 * s,
                              [](double fy) { return Ramp({{0.0, Rgb(0x9C, 0xD0, 0xFF)}, {0.5, Rgb(0x2E, 0x84, 0xEE)}, {1.0, Rgb(0x16, 0x58, 0xC0)}}, fy); });
            }
            StrokeRound(img, groove, 3.0 * s, std::max(1.0, 0.9 * s), WithAlpha(Rgb(0x9A, 0xA6, 0xB2), 0.85));
            const Font timeFont = Segoe(12.0 * s);
            const double timeRight = mid - half - 12.0;
            std::wstring shown = look.total.empty() ? look.elapsed : look.elapsed + L" / " + look.total;
            if (Text().Width(shown, timeFont) > (timeRight - 6.0) * s)
            {
                shown = look.elapsed;
            }
            if (Text().Width(shown, timeFont) <= (timeRight - 6.0) * s)
            {
                const int tw = Text().Width(shown, timeFont);
                p.Text(int(X(timeRight) - tw), int(Y(pc) - Text().Height(timeFont) / 2.0), shown, timeFont, white);
            }
            FillDisc(img, dcx, dcy + 1.0 * s, 25.0 * s, WithAlpha(Rgb(0x00, 0x00, 0x00), 0.5));
            FillDisc(img, dcx, dcy, 24.0 * s, [](double, double fy) { return Mix(Rgb(0x6A, 0x6E, 0x73), Rgb(0x26, 0x28, 0x2B), fy); });
            FillDisc(img, dcx, dcy, 20.5 * s, [](double, double fy) {
                return Ramp({{0.0, Rgb(0xE2, 0xF0, 0xFF)},
                             {0.34, Rgb(0x8C, 0xC4, 0xFF)},
                             {0.5, Rgb(0x2A, 0x7E, 0xEC)},
                             {0.78, Rgb(0x0C, 0x52, 0xC2)},
                             {1.0, Rgb(0x3C, 0x9C, 0xFF)}},
                            fy);
            });
            FillRound(img, Box{dcx - 13.0 * s, dcy - 19.0 * s, 26.0 * s, 15.0 * s}, 7.5 * s, WithAlpha(Rgb(0xFF, 0xFF, 0xFF), 0.32));
            const Color glyph = Rgb(0xFA, 0xFC, 0xFF);
            const Color glyphShadow = WithAlpha(Rgb(0x04, 0x24, 0x60), 0.45);
            if (look.playing)
            {
                for (double ox : {-6.0, 2.0})
                {
                    FillRound(img, Box{dcx + (ox + 0.6) * s, dcy - 7.4 * s, 4.4 * s, 15.0 * s}, 1.0 * s, glyphShadow);
                    FillRound(img, Box{dcx + ox * s, dcy - 8.0 * s, 4.4 * s, 15.0 * s}, 1.0 * s, glyph);
                }
            }
            else
            {
                Triangle(img, dcx - 5.4 * s, dcy - 8.6 * s, dcx - 5.4 * s, dcy + 9.6 * s, dcx + 10.6 * s, dcy + 0.5 * s, glyphShadow);
                Triangle(img, dcx - 6.0 * s, dcy - 9.2 * s, dcx - 6.0 * s, dcy + 9.0 * s, dcx + 10.0 * s, dcy, glyph);
            }
        }

        Image Blank(int frame, int contentW, int contentH, double s)
        {
            int chromeW = 0;
            int chromeH = 0;
            int cx = 0;
            int cy = 0;
            Wmp12Chrome(frame, &chromeW, &chromeH, &cx, &cy);
            return Image(int((contentW + chromeW) * s), int((contentH + chromeH) * s));
        }
    }

    Image DrawWmp12(const Wmp12Look& look, int contentW, int contentH, int scale, Rect* content, Wmp12Spots* spots)
    {
        const double s = std::max(1, scale);
        Image img = Blank(look.frame, contentW, contentH, s);
        PaintWmp12(img, look, contentW, contentH, s, true, true, content, spots);
        return img;
    }

    Image DrawWmp12Base(const Wmp12Look& look, int contentW, int contentH, int scale, Rect* content, Wmp12Spots* spots)
    {
        const double s = std::max(1, scale);
        Image img = Blank(look.frame, contentW, contentH, s);
        PaintWmp12(img, look, contentW, contentH, s, true, false, content, spots);
        return img;
    }

    void DrawWmp12Live(Image& img, const Wmp12Look& look, int contentW, int contentH, int scale)
    {
        PaintWmp12(img, look, contentW, contentH, std::max(1, scale), false, true, nullptr, nullptr);
    }

    void DrawWmp12Art(Image& img, const Rect& area, int scale, const MusicFrame& m, const Image& cover, const std::wstring& title, const std::wstring& artist,
                      bool titleOn, bool bars, bool base, bool live)
    {
        const double s = std::max(1, scale);
        const double textH = titleOn ? 44.0 * s : 0.0;
        if (bars && live)
        {
            const double bw = (area.w - 40.0 * s) / SpectrumBands;
            const double bottom = area.y + area.h - textH - 16.0 * s;
            const double top = area.y + 16.0 * s;
            for (int b = 0; b < SpectrumBands; ++b)
            {
                const double h = std::clamp(double(m.bars[b]), 0.0, 1.0) * (bottom - top);
                const double x = area.x + 20.0 * s + b * bw;
                if (h > 1.0)
                {
                    FillRoundRows(img, Box{x + 1.5 * s, bottom - h, bw - 3.0 * s, h}, 1.5 * s,
                                  [](double fy) { return Ramp({{0.0, Rgb(0x9C, 0xE0, 0xFF)}, {0.4, Rgb(0x3A, 0x9A, 0xF4)}, {1.0, Rgb(0x0E, 0x3C, 0x9A)}}, fy); });
                    FillRoundRows(img, Box{x + 1.5 * s, bottom + 2.0 * s, bw - 3.0 * s, std::min(h * 0.3, 14.0 * s)}, 1.5 * s,
                                  [](double fy) { return WithAlpha(Rgb(0x2A, 0x7A, 0xD8), 0.28 * (1.0 - fy)); });
                }
                const double pk = std::clamp(double(m.peaks[b]), 0.0, 1.0) * (bottom - top);
                if (pk > 2.0)
                {
                    FillRound(img, Box{x + 1.5 * s, bottom - pk - 3.0 * s, bw - 3.0 * s, 2.0 * s}, 1.0 * s, Rgb(0xDC, 0xF0, 0xFF));
                }
            }
        }
        else if (!bars && base)
        {
            const double side = std::min(area.w * 0.62, (area.h - textH) * 0.8);
            const double ax = area.x + (area.w - side) / 2.0;
            const double ay = area.y + (area.h - textH - side) / 2.0;
            if (!cover.Empty())
            {
                const int px = int(std::lround(side));
                const int sq = std::min(cover.width, cover.height);
                const Image art = ResizeSmooth(Crop(cover, Rect{(cover.width - sq) / 2, (cover.height - sq) / 2, sq, sq}), px, px);
                Painter(img).DrawOpaque(art, int(std::lround(ax)), int(std::lround(ay)));
                StrokeRound(img, Box{ax, ay, side, side}, 0.0, std::max(1.0, s), WithAlpha(Rgb(0xFF, 0xFF, 0xFF), 0.3));
            }
            else
            {
                FillRound(img, Box{ax, ay, side, side}, 0.0,
                          [](double fx, double fy) { return WithAlpha(Mix(Rgb(0x5C, 0x86, 0xA8), Rgb(0x1E, 0x46, 0x6A), 0.5 * fx + 0.5 * fy), 0.55); });
                StrokeRound(img, Box{ax, ay, side, side}, 0.0, std::max(1.0, s), WithAlpha(Rgb(0xB8, 0xD4, 0xEA), 0.5));
                const double u = side / 100.0;
                const double hx = ax + 44.0 * u;
                const double hy = ay + 66.0 * u;
                const auto ellipse = [&](double rx, double ry) {
                    std::vector<std::pair<double, double>> pts;
                    for (int i = 0; i < 48; ++i)
                    {
                        const double a = 2.0 * Pi * i / 48.0;
                        const double ex = rx * std::cos(a);
                        const double ey = ry * std::sin(a);
                        const double t = -0.42;
                        pts.push_back({hx + ex * std::cos(t) - ey * std::sin(t), hy + ex * std::sin(t) + ey * std::cos(t)});
                    }
                    return pts;
                };
                FillPolygon(img, ellipse(17.0 * u, 11.5 * u), Rgb(0x4E, 0x70, 0x90));
                FillPolygon(img, ellipse(15.6 * u, 10.2 * u), Rgb(0xDE, 0xEC, 0xF8));
                FillPolygon(img, ellipse(10.0 * u, 5.0 * u), WithAlpha(Rgb(0xFF, 0xFF, 0xFF), 0.7));
                const double stemX = hx + 12.4 * u;
                FillRound(img, Box{stemX - 1.0 * u, ay + 16.0 * u, 7.0 * u, hy - 3.0 * u - (ay + 16.0 * u)}, 2.0 * u, Rgb(0x4E, 0x70, 0x90));
                FillRound(img, Box{stemX, ay + 17.0 * u, 5.0 * u, hy - 4.0 * u - (ay + 17.0 * u)}, 1.5 * u,
                          [](double fx, double) { return Mix(Rgb(0xF4, 0xFA, 0xFF), Rgb(0xA8, 0xC6, 0xE0), fx); });
                FillPolygon(img,
                            {{stemX + 4.0 * u, ay + 17.0 * u},
                             {stemX + 14.0 * u, ay + 26.0 * u},
                             {stemX + 22.0 * u, ay + 40.0 * u},
                             {stemX + 18.0 * u, ay + 56.0 * u},
                             {stemX + 15.0 * u, ay + 44.0 * u},
                             {stemX + 9.0 * u, ay + 35.0 * u},
                             {stemX + 4.0 * u, ay + 31.0 * u}},
                            Rgb(0xC6, 0xDC, 0xEF));
            }
        }
        Painter p(img);
        if (titleOn && base)
        {
            const Font big = Font{FontFamily::Segoe, false, int(std::lround(14.0 * s)), true};
            const Font small = Font{FontFamily::Segoe, false, int(std::lround(11.5 * s)), true};
            const int y0 = int(area.y + area.h - textH + 2.0 * s);
            const std::wstring t = Ellipsize(title, big, area.w - int(20 * s));
            const std::wstring a = Ellipsize(artist, small, area.w - int(20 * s));
            p.Text(area.x + (area.w - Text().Width(t, big)) / 2, y0, t, big, Rgb(0xF4, 0xF6, 0xF8));
            p.Text(area.x + (area.w - Text().Width(a, small)) / 2, y0 + Text().Height(big), a, small, Rgb(0xA8, 0xB8, 0xC8));
        }
        if (live && m.lyricsStrip && !m.lyric.empty())
        {
            const Font lf = Font{FontFamily::Segoe, true, int(std::lround(13.0 * s)), true};
            const std::wstring line = Ellipsize(m.lyric, lf, area.w - int(16 * s));
            const int lw = Text().Width(line, lf);
            const int ly = int(area.y + area.h - textH - Text().Height(lf) - 6.0 * s);
            const int lx = area.x + (area.w - lw) / 2;
            p.Text(lx + int(s), ly + int(s), line, lf, WithAlpha(Rgb(0, 0, 0), 0.8));
            p.Text(lx, ly, line, lf, Rgb(0xFF, 0xFF, 0xFF));
        }
    }
}
