#include "music.h"

#include "glyphs.h"
#include "icons.h"

#include <cmath>
#include <cstring>

namespace rf
{
    namespace
    {
        constexpr int CoverX = 8;
        constexpr int CoverY = 8;
        constexpr int CoverSize = 122;
        constexpr int CoverInner = CoverSize - 4;

        FrameSettings MusicWindow(const FrameSettings& frame)
        {
            FrameSettings f = frame;
            f.player = false;
            f.sunken = false;
            return f;
        }

        Image SquareCrop(const Image& src, int side)
        {
            if (src.Empty())
            {
                return Image();
            }
            const int s = std::min(src.width, src.height);
            return ResizeSmooth(Crop(src, Rect{(src.width - s) / 2, (src.height - s) / 2, s, s}), side, side);
        }

        void Blit(Image& dst, const Image& src, int x0, int y0)
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

        void Glyphed(Painter& p, const Rect& r, const Glyph& g, Color ink, int offset)
        {
            p.DrawGlyph(r.x + (r.w - GlyphWidth(g)) / 2 + offset, r.y + (r.h - GlyphHeight(g)) / 2 + offset, g, ink);
        }

        void DrawDisc(Painter& p, int x0, int y0, double angle, const Shades& sh)
        {
            for (int y = 0; y < 32; ++y)
            {
                for (int x = 0; x < 32; ++x)
                {
                    const double dx = x - 15.5;
                    const double dy = y - 15.5;
                    const double r = std::hypot(dx, dy);
                    if (r > 15.6 || r < 2.6)
                    {
                        continue;
                    }
                    Color c;
                    if (r > 14.7)
                    {
                        c = dx + dy < 0 ? sh.shadow : sh.dark;
                    }
                    else if (r < 4.2)
                    {
                        c = sh.shadow;
                    }
                    else if (r < 6.6)
                    {
                        c = Rgb(0xF0, 0xF0, 0xF0);
                    }
                    else
                    {
                        double a = std::atan2(dy, dx) * 180.0 / 3.14159265358979 - angle;
                        a = std::fmod(std::fmod(a, 180.0) + 180.0, 180.0);
                        if (a >= 14.0 && a < 32.0)
                        {
                            c = Rgb(0xFF, 0xB8, 0xD8);
                        }
                        else if (a >= 32.0 && a < 48.0)
                        {
                            c = Rgb(0xFF, 0xF4, 0x9C);
                        }
                        else if (a >= 48.0 && a < 64.0)
                        {
                            c = Rgb(0xA8, 0xE8, 0xFF);
                        }
                        else if (a >= 112.0 && a < 120.0)
                        {
                            c = Rgb(0xFF, 0xFF, 0xFF);
                        }
                        else
                        {
                            c = Rgb(0xD4, 0xD4, 0xD4);
                        }
                    }
                    p.Pixel(x0 + x, y0 + y, c);
                }
            }
        }

        void DrawCombo(Painter& p, const Rect& r, const std::wstring& text, const Font& f, const FrameColors& colors, const Shades& sh)
        {
            const Color fill = colors.window;
            p.Sunken(r, sh, &fill);
            const Rect b{r.Right() - 2 - 16, r.y + 2, 16, r.h - 4};
            p.Raised(b, sh);
            Glyphed(p, b, glyphs::Down, colors.text, 0);
            p.Text(r.x + 4, r.y + (r.h - Text().Height(f)) / 2, Ellipsize(text, f, r.w - 26), f, colors.text);
        }

        void DrawPanel(Painter& p, int w, int h, const FrameSettings& s, const Shades& sh, const MusicFrame& m, const std::wstring& artist, const std::wstring& title,
                       bool hasCover)
        {
            const FrameColors& c = s.colors;
            const Font f{FamilyFor(s.style), false};
            const Color ink = c.text;
            p.Fill(Rect{0, 0, w, h}, sh.face);
            const Color black = Rgb(0, 0, 0);
            p.Sunken(Rect{CoverX, CoverY, CoverSize, CoverSize}, sh, &black);
            if (!hasCover)
            {
                p.Fill(Rect{CoverX + 2, CoverY + 2, CoverInner, CoverInner}, c.window);
                const Image note = ScaleNearest(BuiltinIcon(IconId::Music), 4);
                p.Draw(note, CoverX + 2 + (CoverInner - note.width) / 2, CoverY + 2 + (CoverInner - note.height) / 2);
            }
            const int left = CoverX + CoverSize + 10;
            const int right = w - 8;
            DrawDisc(p, left + 4, 18, m.angle, sh);
            const Rect display{left + 44, 25, 40, 18};
            p.Raised(display, sh);
            p.Fill(Rect{display.x + 4, display.y + 5, 2, 2}, Rgb(0xFF, 0x00, 0x00));
            p.ThinSunken(Rect{display.x + 9, display.y + 8, 26, 4}, sh);
            const Rect meter{right - 14, 8, 14, 58};
            const Rect plus{meter.x - 26, 10, 22, 22};
            const Rect minus{meter.x - 26, 36, 22, 22};
            p.Raised(plus, sh);
            Glyphed(p, plus, glyphs::Plus, ink, 0);
            p.Raised(minus, sh);
            Glyphed(p, minus, glyphs::Minus, ink, 0);
            p.Sunken(meter, sh, &black);
            const int segments = (meter.h - 4) / 3;
            const int lit = int(std::lround(std::clamp(double(m.level), 0.0, 1.0) * segments));
            for (int j = 0; j < segments; ++j)
            {
                const int sy = meter.Bottom() - 2 - (j + 1) * 3 + 1;
                p.Fill(Rect{meter.x + 3, sy, meter.w - 6, 2}, j < lit ? Rgb(0x30, 0xE0, 0x30) : Rgb(0x00, 0x40, 0x00));
            }
            const int width = right - left;
            const int small = (width - 8) / 5;
            const Rect pause{left, 74, width - 2 * (small + 2), 24};
            p.Pressed(pause, sh);
            Glyphed(p, pause, glyphs::Pause, ink, 1);
            const Rect shuffle{pause.Right() + 2, 74, small, 24};
            const Rect repeat{shuffle.Right() + 2, 74, right - shuffle.Right() - 2, 24};
            p.Raised(shuffle, sh);
            Glyphed(p, shuffle, glyphs::Shuffle, ink, 0);
            p.Raised(repeat, sh);
            Glyphed(p, repeat, glyphs::Repeat, ink, 0);
            const Glyph* row[] = {&glyphs::Previous, &glyphs::Rewind, &glyphs::FastForward, &glyphs::Next, &glyphs::Heart};
            for (int i = 0; i < 5; ++i)
            {
                const int bx = left + i * (small + 2);
                const Rect b{bx, 104, i == 4 ? right - bx : small, 24};
                p.Raised(b, sh);
                Glyphed(p, b, *row[i], ink, 0);
            }
            const auto label = [&](int y, const std::wstring& text) {
                const int tw = Text().Width(text, f);
                p.Text(76 - tw, y + (21 - Text().Height(f)) / 2, text, f, ink);
            };
            label(140, L"Исполнитель:");
            DrawCombo(p, Rect{80, 140, right - 80, 21}, artist, f, c, sh);
            label(166, L"Название:");
            DrawCombo(p, Rect{80, 166, right - 80, 21}, title, f, c, sh);
            const std::wstring now = FormatShort(m.now);
            const std::wstring total = FormatShort(m.length);
            const int tw = std::max(Text().Width(now, f), Text().Width(total, f));
            const int ty = 193;
            p.TextEmbossed(CoverX, ty + (14 - Text().Height(f)) / 2, now, f, sh);
            p.TextEmbossed(right - Text().Width(total, f), ty + (14 - Text().Height(f)) / 2, total, f, sh);
            const Rect track{CoverX + tw + 6, ty, right - tw - 6 - (CoverX + tw + 6), 14};
            const Color light = sh.light;
            p.Sunken(track, sh, &light);
            const int inner = track.w - 4;
            const int done = int(std::lround(std::clamp(m.position, 0.0, 1.0) * inner));
            p.Fill(Rect{track.x + 2, track.y + 2, done, track.h - 4}, sh.shadow);
            const int thumbX = std::clamp(track.x + 2 + done - 5, track.x + 2, track.Right() - 2 - 10);
            p.Raised(Rect{thumbX, track.y + 1, 10, track.h - 2}, sh);
        }
    }

    MusicPlan PlanMusic(const FrameSettings& frame, int squareSize, bool square)
    {
        MusicPlan p;
        p.square = square;
        const int side = std::max(120, squareSize & ~1);
        const FrameGeometry g = MeasureFrame(MusicWindow(frame), MusicClientW, MusicClientH);
        const int room = square ? side * 92 / 100 : side;
        int fits = 1;
        for (int k = 4; k >= 1; --k)
        {
            if (g.width * k <= room && g.height * k <= room)
            {
                fits = k;
                break;
            }
        }
        const bool manual = frame.scale >= 1 && frame.scale <= 4;
        p.scale = manual && (!square || frame.scale <= fits) ? frame.scale : fits;
        if (square)
        {
            p.windowW = g.width * p.scale;
            p.windowH = g.height * p.scale;
            p.windowX = std::max(0, (side - p.windowW) / 2);
            p.windowY = std::max(0, (side - p.windowH) / 2);
            p.outW = side;
            p.outH = side;
            return p;
        }
        if ((g.width * p.scale) % 2 != 0)
        {
            ++p.clientW;
        }
        if ((g.height * p.scale) % 2 != 0)
        {
            ++p.clientH;
        }
        p.windowW = (g.width - MusicClientW + p.clientW) * p.scale;
        p.windowH = (g.height - MusicClientH + p.clientH) * p.scale;
        p.outW = p.windowW;
        p.outH = p.windowH;
        return p;
    }

    MusicComposer::MusicComposer(const FrameSettings& frame, const FrameVars& vars, const MusicPlan& plan, const Image& background, const std::wstring& artist,
                                 const std::wstring& title, const Image& cover)
        : frame_(MusicWindow(frame)), vars_(vars), plan_(plan), background_(background), artist_(artist), title_(title), cover_(cover)
    {
    }

    Image MusicComposer::Window(const MusicFrame& f, Rect& client) const
    {
        FrameVars v = vars_;
        v.time = FormatShort(f.now) + L" / " + FormatShort(f.length);
        Image img = RenderFrame(frame_, plan_.clientW, plan_.clientH, v, &client);
        Painter p(img);
        Painter c = p.Sub(client);
        DrawPanel(c, plan_.clientW, plan_.clientH, frame_, ShadesFor(frame_.colors.face, frame_.style), f, artist_, title_, !cover_.Empty());
        return img;
    }

    const Image& MusicComposer::Compose(const MusicFrame& f)
    {
        Rect client;
        const Image small = Window(f, client);
        const int k = plan_.scale;
        Image window = ScaleNearest(small, k);
        if (coverBig_.Empty() && !cover_.Empty())
        {
            coverBig_ = SquareCrop(cover_, CoverInner * k);
        }
        if (!coverBig_.Empty())
        {
            Blit(window, coverBig_, (client.x + CoverX + 2) * k, (client.y + CoverY + 2) * k);
        }
        if (plan_.square && background_.width == plan_.outW && background_.height == plan_.outH)
        {
            out_ = background_;
        }
        else
        {
            out_ = Image(plan_.outW, plan_.outH, Rgb(0, 0, 0));
        }
        Blit(out_, window, plan_.windowX, plan_.windowY);
        return out_;
    }

    Image MusicComposer::Small(const MusicFrame& f, const Image& background)
    {
        Rect client;
        Image window = Window(f, client);
        if (!cover_.Empty())
        {
            if (coverSmall_.Empty())
            {
                coverSmall_ = SquareCrop(cover_, CoverInner);
            }
            Blit(window, coverSmall_, client.x + CoverX + 2, client.y + CoverY + 2);
        }
        if (!plan_.square)
        {
            return window;
        }
        const int k = plan_.scale;
        const int side = plan_.outW / k;
        Image out = background.width == side && background.height == side ? background : Image(side, side, Rgb(0, 0, 0));
        Blit(out, window, plan_.windowX / k, plan_.windowY / k);
        return out;
    }

    MusicFrame MusicFrameAt(double now, double length, double absolute, bool spin, bool meter, const std::vector<float>& levels)
    {
        MusicFrame f;
        f.now = std::clamp(now, 0.0, std::max(0.0, length));
        f.length = length;
        f.position = length > 0.0 ? f.now / length : 0.0;
        f.angle = spin ? absolute * DiscSpeed : 0.0;
        f.level = meter ? MeterAt(levels, absolute) : 0.0f;
        return f;
    }

    ExportResult ExportMusic(Host& host, const MusicJob& job, const ExportStep& step, std::wstring& error)
    {
        const double fps = std::clamp(job.fps, 1.0, 60.0);
        const int total = std::max(1, int(std::ceil(job.duration * fps - 1e-6)));
        EncoderSpec spec;
        spec.output = job.output;
        spec.quality = job.quality;
        spec.width = job.plan.outW;
        spec.height = job.plan.outH;
        spec.fps = fps;
        spec.audio = true;
        spec.source = job.source;
        spec.start = job.start;
        spec.duration = job.duration;
        spec.fade = job.fade;
        MusicComposer composer(job.frame, job.vars, job.plan, job.background, job.artist, job.title, job.cover);
        return Encode(host, spec, total, [&](int i) -> const Image* {
            const double t = i / fps;
            return &composer.Compose(MusicFrameAt(t, job.duration, job.start + t, job.spin, job.meter, job.levels));
        }, step, error);
    }
}
