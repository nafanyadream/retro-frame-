#include "players.h"

#include "glyphs.h"
#include "music.h"

#include <cmath>
#include <tuple>
#include <utility>

namespace rf
{
    namespace
    {
        const Color WaBody = Rgb(0x38, 0x38, 0x50);
        const Color WaBodyLight = Rgb(0x5E, 0x5E, 0x80);
        const Color WaBodyDark = Rgb(0x1C, 0x1C, 0x2A);
        const Color WaEdge = Rgb(0x0A, 0x0A, 0x12);
        const Color WaFace = Rgb(0xB5, 0xB5, 0xC6);
        const Color WaFaceLight = Rgb(0xEF, 0xEF, 0xF7);
        const Color WaFaceDark = Rgb(0x4A, 0x4A, 0x63);
        const Color WaInk = Rgb(0x29, 0x29, 0x39);
        const Color WaGold = Rgb(0xDE, 0xBD, 0x5A);
        const Color WaGoldLight = Rgb(0xFF, 0xEF, 0xA5);
        const Color WaGoldDark = Rgb(0x8C, 0x6B, 0x21);
        const Color WaGreen = Rgb(0x00, 0xE8, 0x00);
        const Color WaGreenDim = Rgb(0x00, 0x4A, 0x00);
        const Color WaLabel = Rgb(0xA5, 0xA5, 0xC6);
        const Color VisDots = Rgb(24, 33, 41);
        const Color VisPeak = Rgb(150, 150, 150);
        const Color VisColors[16] = {Rgb(239, 49, 16),  Rgb(206, 41, 16),  Rgb(214, 90, 0),   Rgb(214, 102, 0), Rgb(214, 115, 0), Rgb(198, 123, 8),
                                     Rgb(222, 165, 24), Rgb(214, 181, 33), Rgb(189, 222, 41), Rgb(148, 222, 33), Rgb(41, 206, 16), Rgb(50, 190, 16),
                                     Rgb(57, 181, 16),  Rgb(49, 156, 8),   Rgb(41, 148, 0),   Rgb(24, 132, 8)};
        const Color ScopeColors[5] = {Rgb(255, 255, 255), Rgb(214, 214, 222), Rgb(181, 189, 189), Rgb(160, 170, 175), Rgb(148, 156, 165)};

        constexpr int WmpBorder = 4;
        constexpr int WmpTitle = 26;
        constexpr int WmpInfo = 22;
        constexpr int WmpVis = 188;
        constexpr int WmpControls = 58;
        constexpr int WmpTasks = 100;
        constexpr int WmpList = 118;
        constexpr int WmpVisW = 268;

        struct TinyChar
        {
            wchar_t ch;
            Glyph glyph;
        };

        const std::vector<TinyChar>& TinyFont()
        {
            static const std::vector<TinyChar> font = {
                {L'A', {".x.", "x.x", "xxx", "x.x", "x.x"}},       {L'B', {"xx.", "x.x", "xx.", "x.x", "xx."}},
                {L'C', {".xx", "x..", "x..", "x..", ".xx"}},       {L'D', {"xx.", "x.x", "x.x", "x.x", "xx."}},
                {L'E', {"xxx", "x..", "xx.", "x..", "xxx"}},       {L'F', {"xxx", "x..", "xx.", "x..", "x.."}},
                {L'G', {".xx", "x..", "x.x", "x.x", ".xx"}},       {L'H', {"x.x", "x.x", "xxx", "x.x", "x.x"}},
                {L'I', {"xxx", ".x.", ".x.", ".x.", "xxx"}},       {L'J', {"..x", "..x", "..x", "x.x", ".x."}},
                {L'K', {"x.x", "x.x", "xx.", "x.x", "x.x"}},       {L'L', {"x..", "x..", "x..", "x..", "xxx"}},
                {L'M', {"x...x", "xx.xx", "x.x.x", "x...x", "x...x"}}, {L'N', {"x..x", "xx.x", "x.xx", "x..x", "x..x"}},
                {L'O', {".x.", "x.x", "x.x", "x.x", ".x."}},       {L'P', {"xx.", "x.x", "xx.", "x..", "x.."}},
                {L'Q', {".x.", "x.x", "x.x", "xx.", ".xx"}},       {L'R', {"xx.", "x.x", "xx.", "x.x", "x.x"}},
                {L'S', {".xx", "x..", ".x.", "..x", "xx."}},       {L'T', {"xxx", ".x.", ".x.", ".x.", ".x."}},
                {L'U', {"x.x", "x.x", "x.x", "x.x", "xxx"}},       {L'V', {"x.x", "x.x", "x.x", "x.x", ".x."}},
                {L'W', {"x...x", "x...x", "x.x.x", "xx.xx", "x...x"}}, {L'X', {"x.x", "x.x", ".x.", "x.x", "x.x"}},
                {L'Y', {"x.x", "x.x", ".x.", ".x.", ".x."}},       {L'Z', {"xxx", "..x", ".x.", "x..", "xxx"}},
                {L'0', {"xxx", "x.x", "x.x", "x.x", "xxx"}},       {L'1', {".x.", "xx.", ".x.", ".x.", "xxx"}},
                {L'2', {"xx.", "..x", ".x.", "x..", "xxx"}},       {L'3', {"xx.", "..x", ".x.", "..x", "xx."}},
                {L'4', {"x.x", "x.x", "xxx", "..x", "..x"}},       {L'5', {"xxx", "x..", "xx.", "..x", "xx."}},
                {L'6', {".xx", "x..", "xxx", "x.x", "xxx"}},       {L'7', {"xxx", "..x", ".x.", ".x.", ".x."}},
                {L'8', {"xxx", "x.x", "xxx", "x.x", "xxx"}},       {L'9', {"xxx", "x.x", "xxx", "..x", "xx."}},
                {L':', {".", "x", ".", "x", "."}},                 {L'/', {"..x", "..x", ".x.", "x..", "x.."}},
                {L'-', {"...", "...", "xxx", "...", "..."}},       {L'+', {"...", ".x.", "xxx", ".x.", "..."}},
                {L'.', {".", ".", ".", ".", "x"}},                 {L' ', {"..", "..", "..", "..", ".."}},
            };
            return font;
        }

        const Glyph* TinyGlyph(wchar_t c)
        {
            if (c >= L'a' && c <= L'z')
            {
                c = wchar_t(c - L'a' + L'A');
            }
            for (const TinyChar& t : TinyFont())
            {
                if (t.ch == c)
                {
                    return &t.glyph;
                }
            }
            return nullptr;
        }

        void VGrad(Painter& p, const Rect& r, Color top, Color bottom)
        {
            for (int row = 0; row < r.h; ++row)
            {
                p.HLine(r.x, r.y + row, r.w, Mix(top, bottom, r.h > 1 ? double(row) / double(r.h - 1) : 0.0));
            }
        }

        void Bevel(Painter& p, const Rect& r, Color face, Color light, Color dark)
        {
            p.Fill(r, face);
            p.HLine(r.x, r.y, r.w, light);
            p.VLine(r.x, r.y, r.h, light);
            p.HLine(r.x, r.Bottom() - 1, r.w, dark);
            p.VLine(r.Right() - 1, r.y, r.h, dark);
        }

        void SunkenBox(Painter& p, const Rect& r, Color fill)
        {
            p.Fill(r, fill);
            p.HLine(r.x, r.y, r.w, WaEdge);
            p.VLine(r.x, r.y, r.h, WaEdge);
            p.HLine(r.x, r.Bottom() - 1, r.w, WaBodyLight);
            p.VLine(r.Right() - 1, r.y, r.h, WaBodyLight);
        }

        void CentreGlyph(Painter& p, const Rect& r, const Glyph& g, Color c)
        {
            p.DrawGlyph(r.x + (r.w - GlyphWidth(g)) / 2, r.y + (r.h - GlyphHeight(g)) / 2, g, c);
        }

        void Disc(Painter& p, double cx, double cy, double radius, Color top, Color bottom, Color rim)
        {
            const int x0 = int(std::floor(cx - radius - 1));
            const int x1 = int(std::ceil(cx + radius + 1));
            const int y0 = int(std::floor(cy - radius - 1));
            const int y1 = int(std::ceil(cy + radius + 1));
            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const double d = std::hypot(x + 0.5 - cx, y + 0.5 - cy);
                    if (d > radius)
                    {
                        continue;
                    }
                    const double t = std::clamp((y + 0.5 - (cy - radius)) / (2.0 * radius), 0.0, 1.0);
                    p.Pixel(x, y, d > radius - 1.2 ? rim : Mix(top, bottom, t));
                }
            }
        }

        void Pill(Painter& p, const Rect& r, Color top, Color bottom, Color border)
        {
            for (int row = 0; row < r.h; ++row)
            {
                const int cut = row == 0 || row == r.h - 1 ? 2 : row == 1 || row == r.h - 2 ? 1 : 0;
                const Color c = row == 0 || row == r.h - 1 ? border : Mix(top, bottom, r.h > 1 ? double(row) / double(r.h - 1) : 0.0);
                p.HLine(r.x + cut, r.y + row, r.w - 2 * cut, c);
                if (row > 0 && row < r.h - 1)
                {
                    p.Pixel(r.x + cut, r.y + row, border);
                    p.Pixel(r.Right() - 1 - cut, r.y + row, border);
                }
            }
        }

        std::wstring Clock(double seconds)
        {
            const long long s = std::max(0LL, (long long)std::floor(seconds + 1e-6));
            const long long m = s / 60;
            const long long r = s % 60;
            return std::to_wstring(m) + (r < 10 ? L":0" : L":") + std::to_wstring(r);
        }

        std::wstring Clock2(double seconds)
        {
            const long long s = std::max(0LL, (long long)std::floor(seconds + 1e-6));
            const long long m = std::min(99LL, s / 60);
            const long long r = s % 60;
            return std::wstring(m < 10 ? L"0" : L"") + std::to_wstring(m) + (r < 10 ? L":0" : L":") + std::to_wstring(r);
        }

        std::wstring Entry(const std::wstring& artist, const std::wstring& title)
        {
            if (artist.empty())
            {
                return title;
            }
            if (title.empty())
            {
                return artist;
            }
            return artist + L" - " + title;
        }

        void Scrolling(Painter& p, const Rect& box, const std::wstring& text, const Font& f, Color c, double age, bool centre)
        {
            Painter q = p.Clipped(box);
            const int tw = Text().Width(text, f);
            const int ty = box.y + (box.h - Text().Height(f)) / 2;
            if (tw <= box.w)
            {
                q.Text(centre ? box.x + (box.w - tw) / 2 : box.x, ty, text, f, c);
                return;
            }
            const std::wstring sep = L"  ***  ";
            const int gap = Text().Width(sep, f);
            const int shift = int(std::fmod(std::max(0.0, age) * 24.0, double(tw + gap)));
            q.Text(box.x - shift, ty, text, f, c);
            q.Text(box.x - shift + tw, ty, sep, f, c);
            q.Text(box.x - shift + tw + gap, ty, text, f, c);
        }

        void BigDigit(Painter& p, int x, int y, int d, Color on)
        {
            static const unsigned char segments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
            const unsigned char s = segments[std::clamp(d, 0, 9)];
            if (s & 0x01)
            {
                p.Fill(Rect{x + 2, y, 5, 2}, on);
            }
            if (s & 0x02)
            {
                p.Fill(Rect{x + 7, y + 1, 2, 5}, on);
            }
            if (s & 0x04)
            {
                p.Fill(Rect{x + 7, y + 7, 2, 5}, on);
            }
            if (s & 0x08)
            {
                p.Fill(Rect{x + 2, y + 11, 5, 2}, on);
            }
            if (s & 0x10)
            {
                p.Fill(Rect{x, y + 7, 2, 5}, on);
            }
            if (s & 0x20)
            {
                p.Fill(Rect{x, y + 1, 2, 5}, on);
            }
            if (s & 0x40)
            {
                p.Fill(Rect{x + 2, y + 6, 5, 1}, on);
            }
        }

        void SmallButton(Painter& p, const Rect& r)
        {
            Bevel(p, r, WaFace, WaFaceLight, WaFaceDark);
        }

        void WaTitle(Painter& p, int y, const std::wstring& label, bool full)
        {
            const Rect bar{0, y, WinampWidth, 14};
            VGrad(p, bar, Rgb(0x56, 0x56, 0x7A), Rgb(0x22, 0x22, 0x34));
            p.HLine(0, y, WinampWidth, WaBodyLight);
            const int tw = TinyWidth(label);
            const int tx = (WinampWidth - tw) / 2;
            for (int i = 0; i < 4; ++i)
            {
                const int ry = y + 3 + i * 2;
                const int left = full ? 18 : 6;
                p.HLine(left, ry, tx - 5 - left, WaGold);
                p.HLine(left, ry + 1, tx - 5 - left, WaGoldDark);
                p.HLine(tx + tw + 4, ry, 240 - (tx + tw + 4), WaGold);
                p.HLine(tx + tw + 4, ry + 1, 240 - (tx + tw + 4), WaGoldDark);
            }
            TinyText(p, tx, y + 5, label, WaGoldLight);
            const Rect close{264, y + 3, 9, 9};
            const Rect shade{254, y + 3, 9, 9};
            SmallButton(p, close);
            p.DrawGlyph(close.x + 2, close.y + 2, Glyph{"x...x", ".x.x.", "..x..", ".x.x.", "x...x"}, WaInk);
            SmallButton(p, shade);
            p.Fill(Rect{shade.x + 2, shade.y + 2, 5, 2}, WaInk);
            if (full)
            {
                const Rect menu{6, y + 3, 9, 9};
                const Rect minimize{244, y + 3, 9, 9};
                SmallButton(p, menu);
                p.Fill(Rect{menu.x + 2, menu.y + 3, 5, 1}, WaInk);
                p.Fill(Rect{menu.x + 2, menu.y + 5, 5, 1}, WaInk);
                SmallButton(p, minimize);
                p.Fill(Rect{minimize.x + 2, minimize.y + 5, 5, 2}, WaInk);
            }
        }

        void WaBodyFill(Painter& p, int y, int h)
        {
            p.Fill(Rect{0, y, WinampWidth, h}, WaBody);
            p.Frame(Rect{0, y, WinampWidth, h}, WaEdge);
            p.VLine(1, y + 1, h - 2, WaBodyLight);
            p.VLine(WinampWidth - 2, y + 1, h - 2, WaBodyDark);
            p.HLine(1, y + h - 2, WinampWidth - 2, WaBodyDark);
        }

        void WaVis(Painter& p, const Rect& vis, const MusicFrame& m, const std::vector<std::int16_t>* samples, bool scope)
        {
            p.Fill(vis, Rgb(0, 0, 0));
            for (int y = 1; y < vis.h; y += 2)
            {
                for (int x = 1; x < vis.w; x += 2)
                {
                    p.Pixel(vis.x + x, vis.y + y, VisDots);
                }
            }
            if (scope)
            {
                if (samples == nullptr || samples->empty())
                {
                    return;
                }
                const long long centre = (long long)std::llround(m.absolute * AnalysisRate);
                const long long total = (long long)samples->size();
                int prev = -1;
                for (int x = 0; x < vis.w; ++x)
                {
                    const long long i = std::clamp(centre + (x - vis.w / 2) * 4LL, 0LL, total - 1);
                    const double v = (*samples)[std::size_t(i)] / 32768.0;
                    const int y = std::clamp(vis.h / 2 - int(std::lround(v * vis.h * 0.9)), 0, vis.h - 1);
                    const int from = prev < 0 ? y : prev;
                    for (int yy = std::min(from, y); yy <= std::max(from, y); ++yy)
                    {
                        const int dist = std::abs(yy - vis.h / 2);
                        p.Pixel(vis.x + x, vis.y + yy, ScopeColors[std::min(4, dist / 2)]);
                    }
                    prev = y;
                }
                return;
            }
            const int bars = (vis.w + 1) / 4;
            for (int b = 0; b < bars; ++b)
            {
                const double pos = bars > 1 ? double(b) * (SpectrumBands - 1) / double(bars - 1) : 0.0;
                const int i0 = int(std::floor(pos));
                const int i1 = std::min(SpectrumBands - 1, i0 + 1);
                const double t = pos - i0;
                const double v = m.bars[i0] * (1.0 - t) + m.bars[i1] * t;
                const double pk = std::max(m.peaks[i0], m.peaks[i1]);
                const int h = std::clamp(int(std::lround(v * vis.h)), 0, vis.h);
                for (int j = 0; j < h; ++j)
                {
                    const int row = vis.h - 1 - j;
                    p.HLine(vis.x + b * 4, vis.y + row, 3, VisColors[std::clamp(row * 16 / vis.h, 0, 15)]);
                }
                const int ph = std::clamp(int(std::lround(pk * vis.h)), 0, vis.h);
                if (ph > 0)
                {
                    p.HLine(vis.x + b * 4, vis.y + vis.h - ph, 3, VisPeak);
                }
            }
        }

        void WaMain(Painter& p, int y0, const MusicFrame& m, const std::wstring& artist, const std::wstring& title, const std::vector<std::int16_t>* samples,
                    const SkinOptions& o)
        {
            WaBodyFill(p, y0, WinampPart);
            WaTitle(p, y0, L"WINAMP", true);
            SunkenBox(p, Rect{9, y0 + 20, 97, 46}, Rgb(0, 0, 0));
            p.Fill(Rect{10, y0 + 21, 8, 44}, WaBodyDark);
            const wchar_t* clutter[] = {L"O", L"A", L"I", L"D", L"V"};
            for (int i = 0; i < 5; ++i)
            {
                TinyText(p, 13, y0 + 24 + i * 8, clutter[i], WaLabel);
            }
            p.DrawGlyph(26, y0 + 28, glyphs::Play, WaGreen);
            const long long secs = std::max(0LL, (long long)std::floor(m.now + 1e-6));
            const int mm = int(std::min(99LL, secs / 60));
            const int ss = int(secs % 60);
            BigDigit(p, 48, y0 + 26, mm / 10, WaGreen);
            BigDigit(p, 60, y0 + 26, mm % 10, WaGreen);
            p.Fill(Rect{73, y0 + 30, 2, 2}, WaGreen);
            p.Fill(Rect{73, y0 + 35, 2, 2}, WaGreen);
            BigDigit(p, 78, y0 + 26, ss / 10, WaGreen);
            BigDigit(p, 90, y0 + 26, ss % 10, WaGreen);
            WaVis(p, Rect{24, y0 + 43, 76, 16}, m, samples, o.winampScope);

            const Rect marquee{109, y0 + 22, 157, 15};
            SunkenBox(p, marquee, Rgb(0, 0, 0));
            const Font f{FontFamily::Tahoma, false};
            std::wstring line = L"1. " + Entry(artist, title) + L" (" + Clock(m.length) + L")";
            double age = m.absolute;
            if (m.lyricsStrip && !m.lyric.empty())
            {
                line = m.lyric;
                age = std::max(0.0, m.lyricAge - 0.8);
            }
            Scrolling(p, marquee.Inflate(-2), line, f, WaGreen, age, false);
            SunkenBox(p, Rect{109, y0 + 41, 18, 9}, Rgb(0, 0, 0));
            TinyText(p, 111, y0 + 43, L"128", WaGreen);
            TinyText(p, 130, y0 + 43, L"KBPS", WaLabel);
            SunkenBox(p, Rect{154, y0 + 41, 13, 9}, Rgb(0, 0, 0));
            TinyText(p, 156, y0 + 43, L"44", WaGreen);
            TinyText(p, 170, y0 + 43, L"KHZ", WaLabel);
            TinyText(p, 212, y0 + 43, L"MONO", WaGreenDim);
            TinyText(p, 238, y0 + 43, L"STEREO", WaGreen);

            const double volume = 0.72;
            const Rect vol{107, y0 + 59, 68, 9};
            const Color loud = volume < 0.5 ? Mix(Rgb(0x18, 0xB0, 0x18), Rgb(0xE0, 0xD0, 0x20), volume * 2.0) : Mix(Rgb(0xE0, 0xD0, 0x20), Rgb(0xD8, 0x28, 0x10), (volume - 0.5) * 2.0);
            SunkenBox(p, vol, loud);
            for (int x = vol.x + 2; x < vol.Right() - 2; x += 3)
            {
                p.VLine(x, vol.y + 2, vol.h - 4, Mix(loud, Rgb(0, 0, 0), 0.35));
            }
            const Rect volThumb{vol.x + int(std::lround(volume * (vol.w - 14))), y0 + 58, 14, 11};
            Bevel(p, volThumb, WaGold, WaGoldLight, WaGoldDark);
            p.VLine(volThumb.x + 6, volThumb.y + 3, 5, WaGoldDark);
            const Rect bal{177, y0 + 59, 38, 9};
            SunkenBox(p, bal, Rgb(0x18, 0xB0, 0x18));
            const Rect balThumb{bal.x + (bal.w - 14) / 2, y0 + 58, 14, 11};
            Bevel(p, balThumb, WaGold, WaGoldLight, WaGoldDark);
            p.VLine(balThumb.x + 6, balThumb.y + 3, 5, WaGoldDark);
            const Rect eq{219, y0 + 58, 23, 12};
            const Rect pl{242, y0 + 58, 23, 12};
            for (const auto& [r, label, on] : {std::tuple<Rect, const wchar_t*, bool>{eq, L"EQ", o.winampEq}, {pl, L"PL", o.winampList}})
            {
                SmallButton(p, r);
                p.Fill(Rect{r.x + 3, r.y + 4, 3, 3}, on ? Rgb(0x40, 0xFF, 0x40) : WaGreenDim);
                TinyText(p, r.x + 9, r.y + 4, label, WaInk);
            }

            const Rect pos{16, y0 + 72, 248, 10};
            SunkenBox(p, pos, WaBodyDark);
            const Rect posThumb{pos.x + int(std::lround(std::clamp(m.position, 0.0, 1.0) * (pos.w - 29))), pos.y, 29, 10};
            Bevel(p, posThumb, WaGold, WaGoldLight, WaGoldDark);
            for (int i = 0; i < 3; ++i)
            {
                p.VLine(posThumb.x + 11 + i * 3, posThumb.y + 3, 4, WaGoldDark);
            }

            const Rect buttons[] = {{16, y0 + 88, 23, 18}, {39, y0 + 88, 23, 18}, {62, y0 + 88, 23, 18}, {85, y0 + 88, 23, 18}, {108, y0 + 88, 22, 18}};
            const Glyph* marks[] = {&glyphs::Previous, &glyphs::Play, &glyphs::Pause, &glyphs::Stop, &glyphs::Next};
            for (int i = 0; i < 5; ++i)
            {
                SmallButton(p, buttons[i]);
                CentreGlyph(p, buttons[i], *marks[i], WaInk);
            }
            const Rect eject{136, y0 + 89, 22, 16};
            SmallButton(p, eject);
            CentreGlyph(p, eject, Glyph{"...x...", "..xxx..", ".xxxxx.", "xxxxxxx", ".......", "xxxxxxx"}, WaInk);
            const Rect shuffle{164, y0 + 89, 47, 15};
            SmallButton(p, shuffle);
            p.Fill(Rect{shuffle.x + 4, shuffle.y + 6, 3, 3}, WaGreenDim);
            TinyText(p, shuffle.x + 10, shuffle.y + 5, L"SHUFFLE", WaInk);
            const Rect repeat{210, y0 + 89, 28, 15};
            SmallButton(p, repeat);
            p.Fill(Rect{repeat.x + 4, repeat.y + 6, 3, 3}, WaGreenDim);
            p.DrawGlyph(repeat.x + 10, repeat.y + 4, glyphs::Repeat, WaInk);
            const Glyph bolt = {"....xxxx", "...xxxx.", "..xxxx..", ".xxxx...", "xxxxxxx.", "...xxx..", "..xxx...", ".xxx....", "xxx.....", "xx......"};
            p.DrawGlyph(255, y0 + 93, bolt, WaGoldDark);
            p.DrawGlyph(254, y0 + 92, bolt, WaGold);
        }

        void WaSlider(Painter& p, int x, int top, double v)
        {
            const Rect groove{x + 5, top, 4, 63};
            SunkenBox(p, groove, Rgb(0, 0, 0));
            const int mid = top + 31;
            const int ty = top + int(std::lround((1.0 - (std::clamp(v, -1.0, 1.0) + 1.0) / 2.0) * (63 - 11)));
            const int cy = ty + 5;
            const Color strip = VisColors[std::clamp(int(std::lround((1.0 - (v + 1.0) / 2.0) * 15.0)), 0, 15)];
            p.Fill(Rect{x + 6, std::min(mid, cy), 2, std::abs(mid - cy) + 1}, strip);
            const Rect thumb{x + 1, ty, 12, 11};
            Bevel(p, thumb, WaFace, WaFaceLight, WaFaceDark);
            p.HLine(thumb.x + 3, thumb.y + 5, 6, WaInk);
        }

        void WaEq(Painter& p, int y0)
        {
            WaBodyFill(p, y0, WinampPart);
            WaTitle(p, y0, L"WINAMP EQUALIZER", false);
            const Rect on{14, y0 + 18, 26, 12};
            const Rect autoBtn{40, y0 + 18, 32, 12};
            const Rect presets{217, y0 + 18, 44, 12};
            SmallButton(p, on);
            p.Fill(Rect{on.x + 3, on.y + 4, 3, 3}, Rgb(0x40, 0xFF, 0x40));
            TinyText(p, on.x + 10, on.y + 4, L"ON", WaInk);
            SmallButton(p, autoBtn);
            p.Fill(Rect{autoBtn.x + 3, autoBtn.y + 4, 3, 3}, WaGreenDim);
            TinyText(p, autoBtn.x + 10, autoBtn.y + 4, L"AUTO", WaInk);
            SmallButton(p, presets);
            TinyText(p, presets.x + (presets.w - TinyWidth(L"PRESETS")) / 2, presets.y + 4, L"PRESETS", WaInk);
            const double values[10] = {0.55, 0.42, 0.22, 0.02, -0.14, -0.1, 0.08, 0.32, 0.48, 0.6};
            const Rect graph{86, y0 + 17, 113, 19};
            SunkenBox(p, graph, Rgb(0, 0, 0));
            for (int x = graph.x + 2; x < graph.Right() - 2; x += 2)
            {
                p.Pixel(x, graph.y + 9, VisDots);
            }
            int px = -1;
            int py = 0;
            for (int i = 0; i < 10; ++i)
            {
                const int x = graph.x + 3 + i * (graph.w - 7) / 9;
                const int y = graph.y + 9 - int(std::lround(values[i] * 7.0));
                if (px >= 0)
                {
                    for (int s = 0; s <= x - px; ++s)
                    {
                        const int yy = py + int(std::lround(double(y - py) * s / std::max(1, x - px)));
                        p.Pixel(px + s, yy, VisColors[std::clamp((yy - graph.y) * 16 / graph.h, 0, 15)]);
                    }
                }
                px = x;
                py = y;
            }
            WaSlider(p, 21, y0 + 38, 0.0);
            TinyText(p, 16, y0 + 105, L"PREAMP", WaLabel);
            TinyText(p, 42, y0 + 39, L"+12DB", WaLabel);
            TinyText(p, 46, y0 + 67, L"+0DB", WaLabel);
            TinyText(p, 42, y0 + 95, L"-12DB", WaLabel);
            const wchar_t* bands[10] = {L"60", L"170", L"310", L"600", L"1K", L"3K", L"6K", L"12K", L"14K", L"16K"};
            for (int i = 0; i < 10; ++i)
            {
                const int x = 78 + i * 18;
                WaSlider(p, x, y0 + 38, values[i]);
                TinyText(p, x + 7 - TinyWidth(bands[i]) / 2, y0 + 105, bands[i], WaLabel);
            }
        }

        void WaList(Painter& p, int y0, int h, const MusicFrame& m, const std::wstring& artist, const std::wstring& title)
        {
            WaBodyFill(p, y0, h);
            WaTitle(p, y0, L"WINAMP PLAYLIST", false);
            const Rect list{12, y0 + 20, 243, h - 20 - 38};
            SunkenBox(p, list, Rgb(0, 0, 0));
            const Font f{FontFamily::Tahoma, false};
            const int rowH = Text().Height(f);
            const Rect row{list.x + 1, list.y + 1, list.w - 2, rowH};
            p.Fill(row, Rgb(0x00, 0x00, 0xC6));
            const std::wstring length = Clock(m.length);
            const int lw = Text().Width(length, f);
            Painter q = p.Clipped(list.Inflate(-1));
            q.Text(row.x + 2, row.y, Ellipsize(L"1. " + Entry(artist, title), f, row.w - lw - 10), f, Rgb(0xFF, 0xFF, 0xFF));
            q.Text(row.Right() - 2 - lw, row.y, length, f, Rgb(0xFF, 0xFF, 0xFF));
            const Rect scroll{260, list.y, 8, list.h};
            p.Fill(scroll, WaBodyDark);
            Bevel(p, Rect{scroll.x, scroll.y, scroll.w, 18}, WaFace, WaFaceLight, WaFaceDark);
            const int by = y0 + h - 30;
            const wchar_t* labels[] = {L"ADD", L"REM", L"SEL", L"MISC"};
            for (int i = 0; i < 4; ++i)
            {
                const Rect b{14 + i * 29, by, 25, 18};
                SmallButton(p, b);
                TinyText(p, b.x + (b.w - TinyWidth(labels[i])) / 2, b.y + 7, labels[i], WaInk);
            }
            const Rect listBtn{232, by, 25, 18};
            SmallButton(p, listBtn);
            TinyText(p, listBtn.x + (listBtn.w - TinyWidth(L"LIST")) / 2, listBtn.y + 7, L"LIST", WaInk);
            const Rect info{132, y0 + h - 34, 90, 10};
            SunkenBox(p, info, Rgb(0, 0, 0));
            TinyText(p, info.x + 3, info.y + 3, Clock(m.now) + L"/" + Clock(m.length), WaGreen);
            const Glyph mini[] = {{"x.x", "xxx", "x.x"}, {"x..", "xx.", "xxx", "xx.", "x.."}, {"x.x", "x.x", "x.x"}, {"xxx", "xxx", "xxx"}, {"x.x", "xxx", "x.x"}};
            for (int i = 0; i < 5; ++i)
            {
                p.DrawGlyph(134 + i * 8, y0 + h - 19, mini[i], WaLabel);
            }
            const Rect small{190, y0 + h - 21, 32, 9};
            SunkenBox(p, small, Rgb(0, 0, 0));
            TinyText(p, small.x + 3, small.y + 2, Clock(m.now), WaGreen);
        }

        int WmpWidth(const SkinOptions& o)
        {
            return 2 * WmpBorder + (o.wmpTasks ? WmpTasks : 0) + WmpVisW + (o.wmpList ? WmpList : 0);
        }

        int WmpHeight()
        {
            return WmpTitle + WmpInfo + WmpVis + WmpControls + WmpBorder;
        }

        struct WmpLayout
        {
            int w = 0;
            int h = 0;
            Rect title;
            Rect tasks;
            Rect info;
            Rect vis;
            Rect list;
            Rect controls;
            Rect play;
            Rect seek;
            Rect minimize;
            Rect maximize;
            Rect close;
        };

        WmpLayout MeasureWmp(const SkinOptions& o)
        {
            WmpLayout l;
            l.w = WmpWidth(o);
            l.h = WmpHeight();
            l.title = Rect{0, 0, l.w, WmpTitle};
            const int tasksW = o.wmpTasks ? WmpTasks : 0;
            l.tasks = Rect{WmpBorder, WmpTitle, tasksW, WmpInfo + WmpVis};
            const int mx = WmpBorder + tasksW;
            l.info = Rect{mx, WmpTitle, WmpVisW, WmpInfo};
            l.vis = Rect{mx, WmpTitle + WmpInfo, WmpVisW, WmpVis};
            l.list = Rect{mx + WmpVisW, WmpTitle, o.wmpList ? WmpList : 0, WmpInfo + WmpVis};
            l.controls = Rect{WmpBorder, l.h - WmpBorder - WmpControls, l.w - 2 * WmpBorder, WmpControls};
            l.play = Rect{l.controls.x + 8, l.controls.y + 8, 44, 44};
            l.seek = Rect{l.controls.x + 62, l.controls.y + 8, l.controls.w - 62 - 12, 6};
            l.close = Rect{l.w - 24, 5, 18, 16};
            l.maximize = Rect{l.w - 44, 5, 18, 16};
            l.minimize = Rect{l.w - 64, 5, 18, 16};
            return l;
        }

        void CutCorners(Image& img, int top, int bottom)
        {
            const auto cut = [&](int x, int y) {
                if (x >= 0 && y >= 0 && x < img.width && y < img.height)
                {
                    img.Set(x, y, Color(0));
                }
            };
            for (int y = 0; y < top; ++y)
            {
                const int n = top - int(std::lround(std::sqrt(double(top * top - (top - y) * (top - y)))));
                for (int x = 0; x < n; ++x)
                {
                    cut(x, y);
                    cut(img.width - 1 - x, y);
                }
            }
            for (int y = 0; y < bottom; ++y)
            {
                const int n = bottom - int(std::lround(std::sqrt(double(bottom * bottom - (bottom - y) * (bottom - y)))));
                for (int x = 0; x < n; ++x)
                {
                    cut(x, img.height - 1 - y);
                    cut(img.width - 1 - x, img.height - 1 - y);
                }
            }
        }

        Image DrawWmp(const SkinOptions& o, const MusicFrame& m, const std::wstring& artist, const std::wstring& title, bool hasCover)
        {
            const WmpLayout l = MeasureWmp(o);
            Image img(l.w, l.h);
            Painter p(img);
            VGrad(p, Rect{0, 0, l.w, l.h}, Rgb(0x4A, 0x5E, 0x94), Rgb(0x26, 0x34, 0x5E));
            VGrad(p, l.title, Rgb(0x6A, 0x84, 0xC4), Rgb(0x30, 0x42, 0x78));
            p.Frame(Rect{0, 0, l.w, l.h}, Rgb(0x10, 0x18, 0x30));
            p.HLine(1, 1, l.w - 2, Rgb(0x9E, 0xB6, 0xEC));
            p.VLine(1, 1, l.h - 2, Rgb(0x7E, 0x96, 0xCC));
            const Font bold{FontFamily::Tahoma, true};
            const Font f{FontFamily::Tahoma, false};
            p.Draw(SkinIcon(PlayerSkin::MediaPlayer), 8, 5);
            p.Text(30, (WmpTitle - Text().Height(bold)) / 2, Ellipsize(L"Windows Media Player", bold, l.minimize.x - 36), bold, Rgb(0xFF, 0xFF, 0xFF));
            const Glyph minGlyph = {"xxxxxx", "xxxxxx"};
            const Glyph maxGlyph = {"xxxxxxx", "xxxxxxx", "x.....x", "x.....x", "x.....x", "xxxxxxx"};
            const Glyph closeGlyph = {"xx...xx", ".xx.xx.", "..xxx..", ".xx.xx.", "xx...xx"};
            const std::pair<Rect, const Glyph*> buttons[] = {{l.minimize, &minGlyph}, {l.maximize, &maxGlyph}, {l.close, &closeGlyph}};
            for (const auto& [r, g] : buttons)
            {
                Pill(p, r, Rgb(0xDC, 0xE6, 0xFA), Rgb(0x7A, 0x92, 0xC8), Rgb(0x1E, 0x2C, 0x52));
                const int gy = g == &minGlyph ? r.y + r.h - 6 : r.y + (r.h - GlyphHeight(*g)) / 2;
                p.DrawGlyph(r.x + (r.w - GlyphWidth(*g)) / 2, gy, *g, Rgb(0x14, 0x22, 0x48));
            }
            if (o.wmpTasks)
            {
                VGrad(p, l.tasks, Rgb(0x5A, 0x74, 0xB0), Rgb(0x2C, 0x3E, 0x70));
                p.VLine(l.tasks.Right() - 1, l.tasks.y, l.tasks.h, Rgb(0x1A, 0x26, 0x4A));
                const wchar_t* items[] = {L"Now Playing", L"Media Guide", L"CD Audio", L"Media Library", L"Radio Tuner", L"Portable Device", L"Skin Chooser"};
                for (int i = 0; i < 7; ++i)
                {
                    const Rect r{l.tasks.x + 5, l.tasks.y + 10 + i * 27, l.tasks.w - 11, 22};
                    const Font& tf = i == 0 ? bold : f;
                    const std::wstring text = Ellipsize(items[i], tf, r.w - 8);
                    const int ty = r.y + (r.h - Text().Height(tf)) / 2;
                    if (i == 0)
                    {
                        Pill(p, r, Rgb(0xE6, 0xEE, 0xFF), Rgb(0x8C, 0xA8, 0xE0), Rgb(0x1E, 0x34, 0x6C));
                        p.Text(r.x + 7, ty, text, tf, Rgb(0x0A, 0x1A, 0x40));
                    }
                    else
                    {
                        p.Text(r.x + 8, ty + 1, text, tf, Rgb(0x10, 0x1A, 0x38));
                        p.Text(r.x + 7, ty, text, tf, Rgb(0xF0, 0xF4, 0xFF));
                    }
                }
            }
            p.Fill(l.info, Rgb(0x12, 0x1A, 0x30));
            p.HLine(l.info.x, l.info.Bottom() - 1, l.info.w, Rgb(0x3A, 0x4C, 0x80));
            const std::wstring visName = o.wmpCover && hasCover ? L"Album Art" : L"Bars and Waves: Bars";
            const int vnw = Text().Width(visName, f);
            p.Text(l.info.Right() - 6 - vnw, l.info.y + (l.info.h - Text().Height(f)) / 2, visName, f, Rgb(0x7C, 0x96, 0xD0));
            p.DrawGlyph(l.info.Right() - 22 - vnw, l.info.y + 7, glyphs::Left, Rgb(0x7C, 0x96, 0xD0));
            p.DrawGlyph(l.info.Right() - 14 - vnw, l.info.y + 7, glyphs::Right, Rgb(0x7C, 0x96, 0xD0));
            const int infoW = l.info.w - vnw - 36;
            if (infoW > 30)
            {
                Painter q = p.Clipped(Rect{l.info.x + 6, l.info.y, infoW, l.info.h});
                const int ty = l.info.y + (l.info.h - Text().Height(f)) / 2;
                const std::wstring a = artist.empty() ? std::wstring() : artist + L": ";
                const int aw = Text().Width(a, bold);
                q.Text(l.info.x + 6, ty, a, bold, Rgb(0xE8, 0xF0, 0xFF));
                q.Text(l.info.x + 6 + aw, ty, Ellipsize(title, f, std::max(10, infoW - aw)), f, Rgb(0xB8, 0xD0, 0xFF));
            }
            p.Fill(l.vis, Rgb(0, 0, 0));
            const Rect cover = SkinCoverRect(PlayerSkin::MediaPlayer, o);
            if (o.wmpCover && hasCover)
            {
                p.Frame(cover.Inflate(1), Rgb(0x3A, 0x4C, 0x80));
            }
            else
            {
                const Rect area{l.vis.x + 12, l.vis.y + 14, l.vis.w - 24, l.vis.h - 44};
                const int barW = (area.w + 3) / SpectrumBands - 3;
                const int x0 = area.x + (area.w - (SpectrumBands * (barW + 3) - 3)) / 2;
                for (int b = 0; b < SpectrumBands; ++b)
                {
                    const int h = std::clamp(int(std::lround(m.bars[b] * area.h)), 0, area.h);
                    const int x = x0 + b * (barW + 3);
                    for (int j = 0; j < h; ++j)
                    {
                        const double t = double(j) / std::max(1, area.h - 1);
                        p.HLine(x, area.Bottom() - 1 - j, barW, Mix(Rgb(0x1C, 0x30, 0xD8), Rgb(0x38, 0xF0, 0xFF), t));
                    }
                    const int ph = std::clamp(int(std::lround(m.peaks[b] * area.h)), 0, area.h);
                    if (ph > 0)
                    {
                        p.Fill(Rect{x, area.Bottom() - ph - 2, barW, 2}, Rgb(0xD8, 0xEC, 0xFF));
                    }
                    const int mirror = std::min(h / 4, 10);
                    for (int j = 0; j < mirror; ++j)
                    {
                        p.HLine(x, area.Bottom() + 1 + j, barW, Mix(Rgb(0x10, 0x20, 0x70), Rgb(0, 0, 0), double(j) / std::max(1, mirror)));
                    }
                }
            }
            if (m.lyricsStrip && !m.lyric.empty())
            {
                const Rect line{l.vis.x + 8, l.vis.Bottom() - 24, l.vis.w - 16, 18};
                const int lw = Text().Width(m.lyric, bold);
                if (lw <= line.w)
                {
                    const int x = line.x + (line.w - lw) / 2;
                    const int y = line.y + (line.h - Text().Height(bold)) / 2;
                    p.Text(x + 1, y + 1, m.lyric, bold, Rgb(0, 0, 0));
                    p.Text(x, y, m.lyric, bold, Rgb(0xFF, 0xFF, 0xFF));
                }
                else
                {
                    Scrolling(p, line, m.lyric, bold, Rgb(0xFF, 0xFF, 0xFF), std::max(0.0, m.lyricAge - 0.8), true);
                }
            }
            if (o.wmpList)
            {
                p.Fill(l.list, Rgb(0x1A, 0x26, 0x46));
                p.VLine(l.list.x, l.list.y, l.list.h, Rgb(0x3A, 0x4C, 0x80));
                const Rect combo{l.list.x + 6, l.list.y + 4, l.list.w - 11, 18};
                p.Fill(combo, Rgb(0x0E, 0x16, 0x2C));
                p.Frame(combo, Rgb(0x5A, 0x72, 0xB0));
                p.Text(combo.x + 4, combo.y + (combo.h - Text().Height(f)) / 2, Ellipsize(L"Now Playing", f, combo.w - 20), f, Rgb(0xC8, 0xDC, 0xFF));
                p.DrawGlyph(combo.Right() - 11, combo.y + 7, glyphs::Down, Rgb(0xC8, 0xDC, 0xFF));
                const int rowH = Text().Height(f) + 3;
                const Rect row{l.list.x + 4, combo.Bottom() + 6, l.list.w - 8, rowH};
                p.Fill(row, Rgb(0x34, 0x58, 0xA8));
                const std::wstring length = Clock(m.length);
                const int lw = Text().Width(length, f);
                p.Text(row.x + 3, row.y + 2, Ellipsize(title.empty() ? artist : title, f, row.w - lw - 10), f, Rgb(0xFF, 0xFF, 0xFF));
                p.Text(row.Right() - 3 - lw, row.y + 2, length, f, Rgb(0xFF, 0xFF, 0xFF));
                if (!artist.empty() && !title.empty())
                {
                    p.Text(row.x + 3, row.Bottom() + 2, Ellipsize(artist, f, row.w - 6), f, Rgb(0x7C, 0x96, 0xD0));
                }
                const std::wstring total = L"Total Time: " + Clock(m.length);
                p.Text(l.list.x + 6, l.list.Bottom() - 4 - Text().Height(f), Ellipsize(total, f, l.list.w - 12), f, Rgb(0x7C, 0x96, 0xD0));
            }
            const Rect& c = l.controls;
            VGrad(p, c, Rgb(0xE8, 0xEE, 0xFA), Rgb(0x8E, 0xA2, 0xCC));
            p.HLine(c.x, c.y, c.w, Rgb(0x2A, 0x3C, 0x6E));
            const Rect& s = l.seek;
            Pill(p, Rect{s.x, s.y, s.w, s.h}, Rgb(0x1C, 0x2A, 0x58), Rgb(0x34, 0x4A, 0x88), Rgb(0x1C, 0x2A, 0x58));
            const int done = int(std::lround(std::clamp(m.position, 0.0, 1.0) * s.w));
            if (done > 2)
            {
                p.Fill(Rect{s.x + 1, s.y + 1, done - 2, s.h - 2}, Rgb(0x6C, 0xA8, 0xFF));
            }
            Disc(p, s.x + done, s.y + s.h / 2.0, 6.0, Rgb(0xFF, 0xFF, 0xFF), Rgb(0x8C, 0xA8, 0xE0), Rgb(0x2A, 0x3C, 0x6E));
            const double pcx = l.play.x + l.play.w / 2.0;
            const double pcy = l.play.y + l.play.h / 2.0;
            Disc(p, pcx, pcy, 21.0, Rgb(0xB4, 0xDA, 0xFF), Rgb(0x1A, 0x4C, 0xB8), Rgb(0x10, 0x26, 0x5C));
            Disc(p, pcx, pcy - 7, 11.0, Rgb(0xF4, 0xFA, 0xFF), Rgb(0x9C, 0xCA, 0xFF), Rgb(0x9C, 0xCA, 0xFF));
            p.Fill(Rect{int(pcx) - 6, int(pcy) - 7, 4, 15}, Rgb(0xFF, 0xFF, 0xFF));
            p.Fill(Rect{int(pcx) + 2, int(pcy) - 7, 4, 15}, Rgb(0xFF, 0xFF, 0xFF));
            p.Frame(Rect{int(pcx) - 6, int(pcy) - 7, 4, 15}, Rgb(0x10, 0x26, 0x5C));
            p.Frame(Rect{int(pcx) + 2, int(pcy) - 7, 4, 15}, Rgb(0x10, 0x26, 0x5C));
            const Glyph* small[] = {&glyphs::Stop, &glyphs::Previous, &glyphs::Next};
            const int sy = c.y + 34;
            int x = c.x + 72;
            for (const Glyph* g : small)
            {
                Disc(p, x, sy, 10.0, Rgb(0xFF, 0xFF, 0xFF), Rgb(0x9C, 0xB0, 0xD8), Rgb(0x2A, 0x3C, 0x6E));
                p.DrawGlyph(x - GlyphWidth(*g) / 2, sy - GlyphHeight(*g) / 2, *g, Rgb(0x14, 0x22, 0x48));
                x += 25;
            }
            const int statusW = 104;
            const Rect status{c.Right() - 8 - statusW, c.y + 20, statusW, 32};
            const Glyph speaker = {"...x", "..xx", "xxxx", "xxxx", "xxxx", "..xx", "...x"};
            if (status.x - x > 30)
            {
                p.DrawGlyph(x - 4, sy - 3, speaker, Rgb(0x14, 0x22, 0x48));
                const Rect vol{x + 8, sy - 2, std::min(70, status.x - 12 - (x + 8)), 4};
                if (vol.w > 16)
                {
                    Pill(p, vol, Rgb(0x1C, 0x2A, 0x58), Rgb(0x34, 0x4A, 0x88), Rgb(0x1C, 0x2A, 0x58));
                    Disc(p, vol.x + vol.w * 0.75, vol.y + 2.0, 5.0, Rgb(0xFF, 0xFF, 0xFF), Rgb(0x8C, 0xA8, 0xE0), Rgb(0x2A, 0x3C, 0x6E));
                }
            }
            p.Fill(status, Rgb(0x10, 0x1C, 0x3A));
            p.Frame(status, Rgb(0x2A, 0x3C, 0x6E));
            p.Text(status.x + 6, status.y + 3, L"Playing", f, Rgb(0xA8, 0xD0, 0xFF));
            const std::wstring time = Clock2(m.now) + L" / " + Clock2(m.length);
            p.Text(status.x + 6, status.y + 3 + Text().Height(f), Ellipsize(time, f, status.w - 10), f, Rgb(0xE8, 0xF2, 0xFF));
            CutCorners(img, 8, 6);
            return img;
        }
    }

    int TinyWidth(const std::wstring& text)
    {
        int w = 0;
        for (wchar_t c : text)
        {
            const Glyph* g = TinyGlyph(c);
            w += (g != nullptr ? GlyphWidth(*g) : 3) + 1;
        }
        return std::max(0, w - 1);
    }

    void TinyText(Painter& p, int x, int y, const std::wstring& text, Color c)
    {
        for (wchar_t ch : text)
        {
            const Glyph* g = TinyGlyph(ch);
            if (g != nullptr)
            {
                p.DrawGlyph(x, y, *g, c);
                x += GlyphWidth(*g) + 1;
            }
            else
            {
                x += 4;
            }
        }
    }

    void SkinSize(PlayerSkin skin, const SkinOptions& o, int* width, int* height)
    {
        if (skin == PlayerSkin::Winamp)
        {
            *width = WinampWidth;
            *height = WinampPart * (1 + (o.winampEq ? 1 : 0) + (o.winampList ? 1 : 0));
            return;
        }
        *width = WmpWidth(o);
        *height = WmpHeight();
    }

    Image DrawSkin(PlayerSkin skin, const SkinOptions& o, const MusicFrame& m, const std::wstring& artist, const std::wstring& title,
                   const std::vector<std::int16_t>* samples, bool hasCover)
    {
        if (skin == PlayerSkin::MediaPlayer)
        {
            return DrawWmp(o, m, artist, title, hasCover);
        }
        int w = 0;
        int h = 0;
        SkinSize(skin, o, &w, &h);
        Image img(w, h, WaBody);
        Painter p(img);
        WaMain(p, 0, m, artist, title, samples, o);
        int y = WinampPart;
        if (o.winampEq)
        {
            WaEq(p, y);
            y += WinampPart;
        }
        if (o.winampList)
        {
            WaList(p, y, WinampPart, m, artist, title);
        }
        return img;
    }

    SkinSpots SkinHotspots(PlayerSkin skin, const SkinOptions& o)
    {
        SkinSpots s;
        if (skin == PlayerSkin::MediaPlayer)
        {
            const WmpLayout l = MeasureWmp(o);
            s.play = l.play;
            s.seek = l.seek;
            s.close = l.close;
            s.minimize = l.minimize;
            return s;
        }
        s.play = Rect{39, 88, 23, 18};
        s.seek = Rect{16 + 14, 72, 248 - 29, 10};
        s.close = Rect{264, 3, 9, 9};
        s.minimize = Rect{244, 3, 9, 9};
        return s;
    }

    Rect SkinCoverRect(PlayerSkin skin, const SkinOptions& o)
    {
        if (skin != PlayerSkin::MediaPlayer || !o.wmpCover)
        {
            return Rect{};
        }
        const WmpLayout l = MeasureWmp(o);
        const int side = std::min(l.vis.w, l.vis.h) - 28;
        return Rect{l.vis.x + (l.vis.w - side) / 2, l.vis.y + (l.vis.h - side) / 2 - 6, side, side};
    }

    std::wstring SkinTitle(PlayerSkin skin, const std::wstring& artist, const std::wstring& title)
    {
        if (skin == PlayerSkin::Winamp)
        {
            return L"1. " + Entry(artist, title) + L" - Winamp";
        }
        return L"Windows Media Player";
    }

    Image SkinIcon(PlayerSkin skin)
    {
        Image img(16, 16);
        Painter p(img);
        if (skin == PlayerSkin::Winamp)
        {
            const Glyph bolt = {"......xxxxx", ".....xxxxx.", "....xxxxx..", "...xxxxx...", "..xxxxx....", ".xxxxxxxxx.", "....xxxxx..", "...xxxxx...",
                                "..xxxx.....", ".xxxx......", "xxxx.......", "xxx........", "xx........."};
            p.DrawGlyph(4, 2, bolt, Rgb(0x6B, 0x4A, 0x00));
            p.DrawGlyph(3, 1, bolt, Rgb(0xF7, 0xC6, 0x31));
            p.DrawGlyph(4, 2, Glyph{".....x", "....x.", "...x..", "..x...", ".x...."}, Rgb(0xFF, 0xF7, 0xC6));
            return img;
        }
        Disc(p, 8.0, 8.0, 7.5, Rgb(0xFF, 0xC0, 0x50), Rgb(0xD8, 0x60, 0x00), Rgb(0x80, 0x30, 0x00));
        Disc(p, 8.0, 8.0, 5.0, Rgb(0x7C, 0xC0, 0xFF), Rgb(0x10, 0x48, 0xB0), Rgb(0x10, 0x30, 0x80));
        p.DrawGlyph(6, 5, Glyph{"x...", "xx..", "xxx.", "xxxx", "xxx.", "xx..", "x..."}, Rgb(0xFF, 0xFF, 0xFF));
        return img;
    }
}
