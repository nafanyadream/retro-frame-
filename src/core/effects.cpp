#include "effects.h"

#include "glyphs.h"

#include <cmath>
#include <cstring>

namespace rf
{
    namespace
    {
        const char* const ArrowCursor[] = {"X", "XX", "X.X", "X..X", "X...X", "X....X", "X.....X", "X......X", "X.......X", "X........X", "X.....XXXXX",
                                           "X..X..X", "X.X X..X", "XX  X..X", "X    X..X", "     X..X", "      X..X", "      X..X", "       XX"};

        const char* const BusyCursor[] = {"XXXXXXXXXXXXX", "X...........X", "XXXXXXXXXXXXX", " X.........X ", " X.........X ", " X.XXXXXXX.X ", "  X.XXXXX.X  ",
                                          "   X.XXX.X   ", "    X.X.X    ", "     X.X     ", "     X.X     ", "    X...X    ", "   X..X..X   ", "  X...X...X  ",
                                          " X...XXX...X ", " X..XXXXX..X ", " X.XXXXXXX.X ", "XXXXXXXXXXXXX", "X...........X", "XXXXXXXXXXXXX"};

        const char* const SmallBusy[] = {"XXXXXXX", "X.....X", "XXXXXXX", " X.X.X ", "  X.X  ", "   X   ", "  X.X  ", " X.X.X ", "XXXXXXX", "X.....X", "XXXXXXX"};

        template <std::size_t N>
        void DrawBitmap(Image& out, const char* const (&rows)[N], int x0, int y0, int k)
        {
            for (std::size_t r = 0; r < N; ++r)
            {
                const std::size_t len = std::strlen(rows[r]);
                for (std::size_t c = 0; c < len; ++c)
                {
                    const char ch = rows[r][c];
                    if (ch == ' ')
                    {
                        continue;
                    }
                    const Color color = ch == 'X' ? Rgb(0, 0, 0) : Rgb(0xFF, 0xFF, 0xFF);
                    const int px = x0 + int(c) * k;
                    const int py = y0 + int(r) * k;
                    for (int y = std::max(0, py); y < std::min(out.height, py + k); ++y)
                    {
                        for (int x = std::max(0, px); x < std::min(out.width, px + k); ++x)
                        {
                            out.Row(y)[x] = color;
                        }
                    }
                }
            }
        }

        void Put(Image& dst, const Image& src, int x0, int y0)
        {
            const Rect r = Rect{x0, y0, src.width, src.height}.Intersect(Rect{0, 0, dst.width, dst.height});
            for (int y = r.y; y < r.Bottom(); ++y)
            {
                const Color* s = src.Row(y - y0) + (r.x - x0);
                Color* d = dst.Row(y) + r.x;
                for (int x = 0; x < r.w; ++x)
                {
                    d[x] = Alpha(s[x]) == 255 ? s[x] : Opaque(Over(d[x], s[x]));
                }
            }
        }

        void DrawPopupIcon(Painter& p, int x0, int y0, int kind)
        {
            const Color black = Rgb(0, 0, 0);
            if (kind == 1)
            {
                for (int y = 0; y < 32; ++y)
                {
                    const double fy = y + 0.5;
                    if (fy < 1.0)
                    {
                        continue;
                    }
                    const double half = (fy - 1.0) / 30.0 * 15.5;
                    for (int x = 0; x < 32; ++x)
                    {
                        const double d = std::fabs(x + 0.5 - 16.0);
                        if (d > half + 0.5)
                        {
                            continue;
                        }
                        const bool edge = d > half - 1.2 || fy > 30.0;
                        p.Pixel(x0 + x, y0 + y, edge ? black : Rgb(0xFF, 0xFF, 0x00));
                    }
                }
                p.DrawGlyph(x0 + 15, y0 + 12, glyphs::WarningMark, black);
                return;
            }
            const Color fill = kind == 0 ? Rgb(0xFF, 0x00, 0x00) : Rgb(0xFF, 0xFF, 0xFF);
            const Color edge = kind == 0 ? Rgb(0x80, 0x00, 0x00) : black;
            for (int y = 0; y < 32; ++y)
            {
                for (int x = 0; x < 32; ++x)
                {
                    const double dx = x + 0.5 - 16.0;
                    const double dy = y + 0.5 - 16.0;
                    const double r = std::hypot(dx, dy);
                    if (r > 15.5)
                    {
                        continue;
                    }
                    Color c = r > 14.3 ? edge : fill;
                    if (kind == 0 && r < 13.0 && std::fabs(dx) < 7.5 && (std::fabs(dx - dy) < 1.7 || std::fabs(dx + dy) < 1.7))
                    {
                        c = Rgb(0xFF, 0xFF, 0xFF);
                    }
                    p.Pixel(x0 + x, y0 + y, c);
                }
            }
            const Color blue = Rgb(0x00, 0x00, 0xFF);
            if (kind == 2)
            {
                p.DrawGlyph(x0 + 14, y0 + 9, glyphs::InfoMark, blue);
            }
            else if (kind == 3)
            {
                const Glyph& g = glyphs::Help;
                for (std::size_t gy = 0; gy < g.size(); ++gy)
                {
                    for (std::size_t gx = 0; g[gy][gx] != 0; ++gx)
                    {
                        if (g[gy][gx] == 'x')
                        {
                            p.Fill(x0 + 10 + int(gx) * 2, y0 + 7 + int(gy) * 2, 2, 2, blue);
                        }
                    }
                }
            }
        }

        std::vector<std::wstring> SplitLines(const std::wstring& text)
        {
            std::vector<std::wstring> lines;
            std::wstring cur;
            for (wchar_t ch : text)
            {
                if (ch == L'\n')
                {
                    lines.push_back(cur);
                    cur.clear();
                }
                else if (ch != L'\r')
                {
                    cur.push_back(ch);
                }
            }
            lines.push_back(cur);
            while (lines.size() > 1 && lines.back().empty())
            {
                lines.pop_back();
            }
            if (lines.size() > 8)
            {
                lines.resize(8);
            }
            return lines;
        }

        void Outline(Image& out, const Rect& r, int thickness, Color c)
        {
            Painter p(out);
            for (int i = 0; i < thickness; ++i)
            {
                const Rect q = r.Inflate(-i);
                if (q.Empty())
                {
                    break;
                }
                p.Frame(q, c);
            }
        }

        std::uint32_t Next(std::uint32_t& state)
        {
            state = state * 1664525u + 1013904223u;
            return state >> 8;
        }

        int Clamp255(int v)
        {
            return v < 0 ? 0 : (v > 255 ? 255 : v);
        }

        void DrawOsd(Image& img)
        {
            const Font f{FontFamily::SansSerif, true};
            const std::wstring text = L"PLAY";
            const int tw = Text().Width(text, f);
            const int th = Text().Height(f);
            Image small(tw + 12, th + 2, Rgb(0, 0, 0));
            Painter p(small);
            p.Text(0, 1, text, f, Rgb(0xFF, 0xFF, 0xFF));
            p.DrawGlyph(tw + 5, (small.height - 9) / 2, glyphs::Play, Rgb(0xFF, 0xFF, 0xFF));
            const int s = std::max(2, img.height / 200);
            const Image big = ScaleNearest(small, s);
            const int x0 = img.width / 16;
            const int y0 = img.height / 16;
            for (int pass = 0; pass < 2; ++pass)
            {
                const int ox = x0 + (pass == 0 ? s : 0);
                const int oy = y0 + (pass == 0 ? s : 0);
                for (int y = 0; y < big.height; ++y)
                {
                    const int ty = oy + y;
                    if (ty < 0 || ty >= img.height)
                    {
                        continue;
                    }
                    for (int x = 0; x < big.width; ++x)
                    {
                        const int tx = ox + x;
                        if (tx < 0 || tx >= img.width)
                        {
                            continue;
                        }
                        const int a = Luminance(big.Row(y)[x]);
                        if (a < 8)
                        {
                            continue;
                        }
                        const Color ink = pass == 0 ? Rgb(0x10, 0x10, 0x10) : Rgb(0xF4, 0xF4, 0xF4);
                        img.Row(ty)[tx] = Opaque(Mix(img.Row(ty)[tx], ink, a / 255.0));
                    }
                }
            }
        }

        void Crt(Image& img, int k)
        {
            const int w = img.width;
            const int h = img.height;
            std::vector<float> col(static_cast<std::size_t>(w));
            std::vector<float> row(static_cast<std::size_t>(h));
            for (int x = 0; x < w; ++x)
            {
                const float dx = (x + 0.5f) / float(w) * 2.0f - 1.0f;
                col[std::size_t(x)] = 1.0f - 0.24f * dx * dx;
            }
            const int period = std::max(2, k);
            for (int y = 0; y < h; ++y)
            {
                const float dy = (y + 0.5f) / float(h) * 2.0f - 1.0f;
                row[std::size_t(y)] = (1.0f - 0.24f * dy * dy) * (y % period == period - 1 ? 0.62f : 1.0f) * 1.12f;
            }
            const int unit = std::max(1, k / 2);
            for (int y = 0; y < h; ++y)
            {
                Color* r = img.Row(y);
                const float gy = row[std::size_t(y)];
                for (int x = 0; x < w; ++x)
                {
                    const float g = gy * col[std::size_t(x)];
                    const int phase = (x / unit) % 3;
                    const Color c = r[x];
                    const int red = Clamp255(int(Red(c) * g * (phase == 0 ? 1.0f : 0.86f)));
                    const int green = Clamp255(int(Green(c) * g * (phase == 1 ? 1.0f : 0.86f)));
                    const int blue = Clamp255(int(Blue(c) * g * (phase == 2 ? 1.0f : 0.86f)));
                    r[x] = Rgb(red, green, blue);
                }
            }
        }

        void Vhs(Image& img, int seed, int k)
        {
            const int w = img.width;
            const int h = img.height;
            const int shift = std::max(2, (3 * k) / 2);
            std::uint32_t state = 2166136261u ^ (std::uint32_t(seed) * 16777619u);
            const double phase = seed * 0.07;
            const int bandH = std::max(2, h / 40);
            const int bandY = int(h * (0.80 + 0.06 * std::sin(phase)));
            std::vector<Color> line(static_cast<std::size_t>(w));
            for (int y = 0; y < h; ++y)
            {
                Color* r = img.Row(y);
                std::memcpy(line.data(), r, std::size_t(w) * sizeof(Color));
                const bool band = y >= bandY && y < bandY + bandH;
                int offset = 0;
                if (band)
                {
                    offset = int(Next(state) % std::uint32_t(12 * k + 1)) - 6 * k;
                }
                for (int x = 0; x < w; ++x)
                {
                    const int sx = std::clamp(x - offset, 0, w - 1);
                    const int red = Red(line[std::size_t(std::clamp(sx + shift, 0, w - 1))]);
                    const int green = Green(line[std::size_t(sx)]);
                    const int blue = Blue(line[std::size_t(std::clamp(sx - shift, 0, w - 1))]);
                    const int lum = (red * 30 + green * 59 + blue * 11) / 100;
                    const int noise = band ? int(Next(state) % 121u) - 40 : int(Next(state) % 29u) - 14;
                    r[x] = Rgb(Clamp255((red * 85 + lum * 15) / 100 * 104 / 100 + noise), Clamp255((green * 85 + lum * 15) / 100 + noise),
                               Clamp255((blue * 85 + lum * 15) / 100 * 94 / 100 + noise));
                }
            }
            DrawOsd(img);
        }
    }

    bool HasOverlays(const EffectSettings& e)
    {
        return e.cursor > 0 || e.popups > 0 || e.screen > 0;
    }

    Image RenderPopup(const EffectSettings& e, const FrameSettings& base, bool active, Rect* firstButton)
    {
        FrameSettings f;
        f.style = base.style;
        f.colors = base.colors;
        f.kind = WindowKind::Dialog;
        f.active = active;
        f.title = e.popupTitle;
        f.grip = false;
        const Font font{FamilyFor(f.style), false};
        const std::vector<std::wstring> lines = SplitLines(e.popupText);
        int textW = 0;
        for (const auto& l : lines)
        {
            textW = std::max(textW, Text().Width(l, font));
        }
        const int lineH = std::max(13, Text().Height(font) + 1);
        const int textH = int(lines.size()) * lineH;
        std::vector<std::wstring> labels = {L"OK"};
        if (e.popupButtons == 1)
        {
            labels = {L"OK", L"Отмена"};
        }
        else if (e.popupButtons == 2)
        {
            labels = {L"Да", L"Нет"};
        }
        const int bw = 75;
        const int bh = 23;
        const int gap = 6;
        const int n = int(labels.size());
        const int buttonsW = n * bw + (n - 1) * gap;
        const int bodyH = std::max(32, textH);
        const int cw = std::max({12 + 32 + 14 + textW + 16, buttonsW + 24, 160});
        const int ch = 12 + bodyH + 14 + bh + 10;
        Rect client;
        Image img = RenderFrame(f, cw, ch, FrameVars{}, &client);
        Painter p(img);
        Painter c = p.Sub(client);
        const Shades sh = ShadesFor(f.colors.face, f.style);
        c.Fill(Rect{0, 0, cw, ch}, sh.face);
        DrawPopupIcon(c, 12, 12, std::clamp(e.popupIcon, 0, 3));
        int ty = 12 + (bodyH - textH) / 2;
        for (const auto& l : lines)
        {
            c.Text(12 + 32 + 14, ty + (lineH - Text().Height(font)) / 2, l, font, f.colors.text);
            ty += lineH;
        }
        int bx = (cw - buttonsW) / 2;
        const int by = 12 + bodyH + 14;
        for (int i = 0; i < n; ++i)
        {
            const Rect r{bx, by, bw, bh};
            if (i == 0)
            {
                c.Frame(r, Rgb(0, 0, 0));
                c.Raised(r.Inflate(-1), sh);
                c.Dotted(r.Inflate(-4), f.colors.text);
                if (firstButton != nullptr)
                {
                    *firstButton = r.Offset(client.x, client.y);
                }
            }
            else
            {
                c.Raised(r, sh);
            }
            const int tw = Text().Width(labels[std::size_t(i)], font);
            c.Text(r.x + (bw - tw) / 2, r.y + (bh - Text().Height(font)) / 2, labels[std::size_t(i)], font, f.colors.text);
            bx += bw + gap;
        }
        return img;
    }

    void ApplyScreen(Image& img, int kind, int seed, int scale)
    {
        if (img.Empty())
        {
            return;
        }
        const int k = std::max(1, scale);
        if (kind == 1)
        {
            Crt(img, k);
        }
        else if (kind == 2)
        {
            Vhs(img, seed, k);
        }
    }

    EffectsLayer::EffectsLayer(const EffectSettings& e, const FrameSettings& frame, const Stage& stage)
        : e_(e), frame_(frame), stage_(stage)
    {
        const int k = std::max(1, stage.scale);
        stage_.scale = k;
        const Rect cb = CloseButtonRect(frame, stage.window.w / k);
        close_ = Rect{stage.window.x + cb.x * k, stage.window.y + cb.y * k, cb.w * k, cb.h * k};
        if (e.popups > 0)
        {
            Rect button;
            popupActive_ = ScaleNearest(RenderPopup(e, frame, true, &button), k);
            popupInactive_ = ScaleNearest(RenderPopup(e, frame, false, nullptr), k);
            popupButton_ = Rect{button.x * k, button.y * k, button.w * k, button.h * k};
            popupCount_ = e.popups == 1 ? 1 : CascadeCount;
        }
    }

    int EffectsLayer::VisiblePopups(double t, bool animate) const
    {
        const int n = popupCount_;
        if (n == 0 || t < 0.0)
        {
            return 0;
        }
        if (!animate || e_.popups != 3)
        {
            return n;
        }
        if (t < PopupStart)
        {
            return 0;
        }
        return std::min(n, 1 + int((t - PopupStart) / PopupStep));
    }

    std::vector<Rect> EffectsLayer::PopupRects(int width, int height) const
    {
        std::vector<Rect> rects;
        const int n = popupCount_;
        if (n == 0)
        {
            return rects;
        }
        const int k = stage_.scale;
        const int pw = popupActive_.width;
        const int ph = popupActive_.height;
        int step = 22 * k;
        if (n > 1)
        {
            step = std::clamp(std::min(width - pw, height - ph) / (n - 1), 4 * k, 22 * k);
        }
        const Rect& w = stage_.window;
        int x0 = w.x + w.w / 2 - pw / 2 - (n - 1) * step / 2;
        int y0 = w.y + w.h / 2 - ph / 2 - (n - 1) * step / 2;
        const int spanW = pw + (n - 1) * step;
        const int spanH = ph + (n - 1) * step;
        x0 = spanW <= width ? std::clamp(x0, 0, width - spanW) : (width - spanW) / 2;
        y0 = spanH <= height ? std::clamp(y0, 0, height - spanH) : (height - spanH) / 2;
        for (int i = 0; i < n; ++i)
        {
            rects.push_back(Rect{x0 + i * step, y0 + i * step, pw, ph});
        }
        return rects;
    }

    void EffectsLayer::DrawIntro(Image& out, const Image& background, double t) const
    {
        if (background.width == out.width && background.height == out.height)
        {
            out.pixels = background.pixels;
        }
        else
        {
            std::fill(out.pixels.begin(), out.pixels.end(), Rgb(0, 0, 0));
        }
        const int k = stage_.scale;
        const Rect& w = stage_.window;
        const Rect from{w.x + w.w / 2 - w.w / 16, w.y + w.h / 2 - 9 * k, std::max(1, w.w / 8), 18 * k};
        const auto at = [&](double q) {
            q = std::clamp(q, 0.0, 1.0);
            q = 1.0 - (1.0 - q) * (1.0 - q);
            const auto lerp = [q](int a, int b) { return int(std::lround(a + (b - a) * q)); };
            return Rect{lerp(from.x, w.x), lerp(from.y, w.y), lerp(from.w, w.w), lerp(from.h, w.h)};
        };
        const double p = std::clamp(t / IntroSeconds, 0.0, 1.0);
        if (frame_.style == Style::Win95)
        {
            for (int j = 2; j >= 0; --j)
            {
                const double q = p - j * 0.15;
                if (q <= 0.0)
                {
                    continue;
                }
                Outline(out, at(q), 2 * k, j == 0 ? Rgb(0xC0, 0xC0, 0xC0) : Rgb(0x80, 0x80, 0x80));
            }
            return;
        }
        const Rect r = at(p);
        Outline(out, r, k, Rgb(0x80, 0x80, 0x80));
        Painter painter(out);
        const FrameColors& c = frame_.colors;
        painter.GradientH(r.x, r.y, r.w, std::min(r.h, 18 * k), c.active1, HasGradient(frame_.style) ? c.active2 : c.active1);
    }

    void EffectsLayer::Apply(Image& out, double t, int seed, bool animate) const
    {
        const int k = stage_.scale;
        const int shown = VisiblePopups(t, animate);
        const std::vector<Rect> rects = PopupRects(out.width, out.height);
        Rect button;
        bool haveButton = false;
        for (int i = 0; i < shown; ++i)
        {
            const Rect& r = rects[std::size_t(i)];
            const bool top = i == shown - 1;
            Put(out, top ? popupActive_ : popupInactive_, r.x, r.y);
            if (top)
            {
                button = popupButton_.Offset(r.x, r.y);
                haveButton = true;
            }
        }
        if (e_.cursor > 0 && t >= 0.0)
        {
            const Rect& w = stage_.window;
            int px = close_.x + close_.w * 3 / 5;
            int py = close_.y + close_.h * 3 / 5;
            if (e_.cursorPlace == 1)
            {
                px = w.x + w.w / 2;
                py = w.y + w.h / 2;
            }
            else if (e_.cursorPlace == 2)
            {
                px = w.x + w.w * 78 / 100;
                py = w.y + w.h * 80 / 100;
            }
            else if (e_.cursorPlace == 3 && haveButton)
            {
                px = button.x + button.w * 3 / 5;
                py = button.y + button.h * 3 / 5;
            }
            if (e_.cursor == 2)
            {
                DrawBitmap(out, BusyCursor, px - 6 * k, py - 10 * k, k);
            }
            else
            {
                DrawBitmap(out, ArrowCursor, px, py, k);
                if (e_.cursor == 3)
                {
                    DrawBitmap(out, SmallBusy, px + 12 * k, py + 10 * k, k);
                }
            }
        }
        if (e_.screen > 0)
        {
            ApplyScreen(out, e_.screen, seed, k);
        }
    }
}
