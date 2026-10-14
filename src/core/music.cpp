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

        void DrawAnalyzer(Painter& p, const Rect& box, const MusicFrame& m, const Shades& sh)
        {
            const Color black = Rgb(0, 0, 0);
            p.Sunken(box, sh, &black);
            const int innerH = box.h - 6;
            const int segments = innerH / 3;
            const int barW = std::max(2, (box.w - 6 + 1) / SpectrumBands - 1);
            const int total = SpectrumBands * (barW + 1) - 1;
            const int x0 = box.x + (box.w - total) / 2;
            const int bottom = box.y + 3 + segments * 3;
            for (int b = 0; b < SpectrumBands; ++b)
            {
                const int x = x0 + b * (barW + 1);
                const int lit = int(std::lround(std::clamp(double(m.bars[b]), 0.0, 1.0) * segments));
                for (int j = 0; j < lit; ++j)
                {
                    const double h = double(j) / std::max(1, segments - 1);
                    const Color c = h < 0.55 ? Rgb(0x28, 0xD8, 0x28) : (h < 0.82 ? Rgb(0xE8, 0xE0, 0x20) : Rgb(0xF0, 0x38, 0x20));
                    p.Fill(Rect{x, bottom - (j + 1) * 3 + 1, barW, 2}, c);
                }
                const int peak = int(std::lround(std::clamp(double(m.peaks[b]), 0.0, 1.0) * segments));
                if (peak > 0)
                {
                    p.Fill(Rect{x, bottom - peak * 3 + 1, barW, 1}, Rgb(0xC8, 0xC8, 0xC8));
                }
            }
        }

        void CentredText(Painter& p, const Rect& r, int y, const std::wstring& text, const Font& f, Color ink)
        {
            p.Text(r.x + (r.w - Text().Width(text, f)) / 2, y, text, f, ink);
        }

        void DrawWave(Painter& p, const Rect& inner, const std::vector<std::int16_t>* samples, double seconds)
        {
            const Color green = Rgb(0x00, 0xFF, 0x00);
            const int mid = inner.y + inner.h / 2;
            const int half = inner.h / 2 - 1;
            p.Fill(Rect{inner.x, mid, inner.w, 1}, green);
            if (samples == nullptr || samples->empty())
            {
                return;
            }
            const double span = RecorderWaveSeconds * AnalysisRate;
            const double first = seconds * AnalysisRate - span / 2.0;
            const long long total = (long long)samples->size();
            for (int x = 0; x < inner.w; ++x)
            {
                const long long a = (long long)std::floor(first + span * x / inner.w);
                const long long b = std::max(a + 1, (long long)std::floor(first + span * (x + 1) / inner.w));
                int lo = 0;
                int hi = 0;
                for (long long i = std::max(0LL, a); i < std::min(total, b); ++i)
                {
                    const int v = (*samples)[std::size_t(i)];
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
                const int top = mid - int(std::lround(hi / 32768.0 * half * 0.92));
                const int bottom = mid - int(std::lround(lo / 32768.0 * half * 0.92));
                p.Fill(Rect{inner.x + x, std::min(top, mid), 1, std::max(bottom, mid) - std::min(top, mid) + 1}, green);
            }
        }

        void DrawRecorder(Painter& p, int w, int h, const FrameSettings& s, const Shades& sh, const MusicFrame& m, bool russian, double length,
                          const std::vector<std::int16_t>* samples)
        {
            const Font f{FamilyFor(s.style), false};
            const Color ink = s.colors.text;
            p.Fill(Rect{0, 0, w, h}, sh.face);
            const Rect posBox{6, 6, 64, 36};
            const Rect lenBox{w - 6 - 64, 6, 64, 36};
            const Rect wave{posBox.Right() + 4, 4, lenBox.x - 4 - (posBox.Right() + 4), 40};
            const int th = Text().Height(f);
            for (const Rect* box : {&posBox, &lenBox})
            {
                p.ThinSunken(*box, sh);
            }
            CentredText(p, posBox, posBox.y + 4, russian ? L"Позиция:" : L"Position:", f, ink);
            CentredText(p, posBox, posBox.Bottom() - 5 - th, RecorderSeconds(m.absolute, russian), f, ink);
            CentredText(p, lenBox, lenBox.y + 4, russian ? L"Длина:" : L"Length:", f, ink);
            CentredText(p, lenBox, lenBox.Bottom() - 5 - th, RecorderSeconds(length, russian), f, ink);
            const Color black = Rgb(0, 0, 0);
            p.Sunken(wave, sh, &black);
            DrawWave(p, wave.Inflate(-2), samples, m.wave >= 0.0 ? m.wave : m.absolute);
            const Rect channel{10, 56, w - 20, 4};
            p.Sunken(channel, sh);
            const double pos = length > 0.0 ? std::clamp(m.absolute / length, 0.0, 1.0) : 0.0;
            const int thumbW = 11;
            const int thumbX = channel.x + int(std::lround(pos * (channel.w - thumbW)));
            p.Raised(Rect{thumbX, 47, thumbW, 21}, sh);
            const int bw = (w - 12 - 4 * 2) / 5;
            for (int i = 0; i < 5; ++i)
            {
                const Rect b{6 + i * (bw + 2), 72, i == 4 ? w - 6 - (6 + i * (bw + 2)) : bw, 24};
                if (i == 3)
                {
                    p.Frame(b, black);
                    p.Raised(b.Inflate(-1), sh);
                    p.Dotted(b.Inflate(-4), ink);
                }
                else
                {
                    p.Raised(b, sh);
                }
                if (i < 2 && m.recording)
                {
                    const Glyph& g = i == 0 ? glyphs::BigRewind : glyphs::BigForward;
                    Glyphed(p, b, g, sh.hilite, 1);
                    Glyphed(p, b, g, sh.shadow, 0);
                }
                else if (i == 0)
                {
                    Glyphed(p, b, glyphs::BigRewind, ink, 0);
                }
                else if (i == 1)
                {
                    Glyphed(p, b, glyphs::BigForward, ink, 0);
                }
                else if (i == 2)
                {
                    Glyphed(p, b, glyphs::BigPlay, sh.hilite, 1);
                    Glyphed(p, b, glyphs::BigPlay, sh.shadow, 0);
                }
                else if (i == 3)
                {
                    Glyphed(p, b, glyphs::BigStop, ink, 0);
                }
                else
                {
                    const int cx = b.x + b.w / 2;
                    const int cy = b.y + b.h / 2;
                    for (int y = -5; y <= 5; ++y)
                    {
                        for (int x = -5; x <= 5; ++x)
                        {
                            const double r = std::hypot(x, y);
                            if (r <= 4.6 && m.recording)
                            {
                                p.Pixel(cx + x + 1, cy + y + 1, sh.hilite);
                            }
                        }
                    }
                    for (int y = -5; y <= 5; ++y)
                    {
                        for (int x = -5; x <= 5; ++x)
                        {
                            const double r = std::hypot(x, y);
                            if (r <= 4.6)
                            {
                                const Color lit = r > 3.8 ? Rgb(0x80, 0x00, 0x00) : Rgb(0xD0, 0x00, 0x00);
                                p.Pixel(cx + x, cy + y, m.recording ? sh.shadow : lit);
                            }
                        }
                    }
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
            const Rect meter{right - 14, 8, 14, 58};
            if (m.equalizer)
            {
                DrawAnalyzer(p, Rect{left, 8, meter.x - 26 - 6 - left, 58}, m, sh);
            }
            else
            {
                DrawDisc(p, left + 4, 18, m.angle, sh);
                const Rect display{left + 44, 25, 40, 18};
                p.Raised(display, sh);
                p.Fill(Rect{display.x + 4, display.y + 5, 2, 2}, Rgb(0xFF, 0x00, 0x00));
                p.ThinSunken(Rect{display.x + 9, display.y + 8, 26, 4}, sh);
            }
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
            if (m.lyricsStrip)
            {
                const Rect strip{CoverX, MusicClientH, right - CoverX, LyricsStripH - 4};
                p.Sunken(strip, sh, &black);
                const Rect lcd = strip.Inflate(-3);
                Painter q = p.Clipped(lcd);
                const int lw = Text().Width(m.lyric, f);
                const int ly = lcd.y + (lcd.h - Text().Height(f)) / 2;
                const Color green = Rgb(0x40, 0xF0, 0x40);
                if (lw <= lcd.w)
                {
                    q.Text(lcd.x + (lcd.w - lw) / 2, ly, m.lyric, f, green);
                }
                else
                {
                    const int gap = 48;
                    const int shift = int(std::fmod(std::max(0.0, m.lyricAge - 0.8) * 34.0, double(lw + gap)));
                    q.Text(lcd.x - shift, ly, m.lyric, f, green);
                    q.Text(lcd.x - shift + lw + gap, ly, m.lyric, f, green);
                }
            }
        }
    }

    std::wstring RecorderSeconds(double seconds, bool russian)
    {
        const long long hundredths = std::max(0LL, (long long)std::llround(seconds * 100.0));
        const long long cents = hundredths % 100;
        return std::to_wstring(hundredths / 100) + (russian ? L"," : L".") + (cents < 10 ? L"0" : L"") + std::to_wstring(cents) + (russian ? L" сек." : L" sec.");
    }

    std::wstring RecorderTitle(const std::wstring& base, bool russian)
    {
        const std::wstring app = russian ? L"Звукозапись" : L"Sound Recorder";
        if (base.find(app) != std::wstring::npos)
        {
            return base;
        }
        return (base.empty() ? std::wstring(russian ? L"Звук" : L"Sound") : base) + L" - " + app;
    }

    std::vector<std::wstring> RecorderMenu(bool russian)
    {
        if (russian)
        {
            return {L"Файл", L"Правка", L"Эффекты", L"Справка"};
        }
        return {L"File", L"Edit", L"Effects", L"Help"};
    }

    MusicPlan PlanMusic(const FrameSettings& frame, int canvasW, int canvasH, bool square, bool lyrics, bool recorder, int reserve)
    {
        MusicPlan p;
        p.square = square;
        const int baseW = recorder ? RecorderClientW : MusicClientW;
        const int baseH = recorder ? RecorderClientH : MusicClientH + (lyrics ? LyricsStripH : 0);
        p.clientW = baseW;
        p.clientH = baseH;
        const int outW = std::max(120, canvasW & ~1);
        const int outH = std::max(120, canvasH & ~1);
        const FrameGeometry g = MeasureFrame(MusicWindow(frame), baseW, baseH);
        const int roomW = square ? outW * 92 / 100 : outW;
        const int extra = square ? std::max(0, reserve) : 0;
        const auto roomH = [&](int k) { return square ? (outH - extra * k) * 92 / 100 : outH; };
        int fits = 1;
        for (int k = 4; k >= 1; --k)
        {
            if (g.width * k <= roomW && g.height * k <= roomH(k))
            {
                fits = k;
                break;
            }
        }
        const bool manual = frame.scale >= 1 && frame.scale <= 4;
        p.scale = manual && (!square || frame.scale <= fits) ? frame.scale : fits;
        if (square)
        {
            p.reserve = extra * p.scale;
            p.windowW = g.width * p.scale;
            p.windowH = g.height * p.scale;
            p.windowX = std::max(0, (outW - p.windowW) / 2);
            p.windowY = std::max(0, (outH - p.reserve - p.windowH) / 2);
            p.outW = outW;
            p.outH = outH;
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
        p.windowW = (g.width - baseW + p.clientW) * p.scale;
        p.windowH = (g.height - baseH + p.clientH) * p.scale;
        p.outW = p.windowW;
        p.outH = p.windowH;
        return p;
    }

    void MusicHotspots(const FrameSettings& frame, const MusicPlan& plan, bool recorder, Rect* play, Rect* seek)
    {
        if (recorder)
        {
            const int bw = (plan.clientW - 20) / 5;
            *play = Rect{6 + 2 * (bw + 2), 72, bw, 24};
            *seek = Rect{15, 47, plan.clientW - 31, 21};
            return;
        }
        const int left = CoverX + CoverSize + 10;
        const int right = plan.clientW - 8;
        const int width = right - left;
        const int small = (width - 8) / 5;
        *play = Rect{left, 74, width - 2 * (small + 2), 24};
        const int tw = Text().Width(L"00:00", Font{FamilyFor(frame.style), false});
        const Rect track{CoverX + tw + 6, 193, right - tw - 6 - (CoverX + tw + 6), 14};
        *seek = Rect{track.x + 2, track.y, track.w - 4, track.h};
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
        const Shades sh = ShadesFor(frame_.colors.face, frame_.style);
        if (recorder_)
        {
            DrawRecorder(c, plan_.clientW, plan_.clientH, frame_, sh, f, russian_, songLength_, samples_.get());
        }
        else
        {
            DrawPanel(c, plan_.clientW, plan_.clientH, frame_, sh, f, artist_, title_, !cover_.Empty());
        }
        return img;
    }

    void MusicComposer::SetRecorder(bool russian, double songLength, std::shared_ptr<const std::vector<std::int16_t>> samples)
    {
        recorder_ = true;
        russian_ = russian;
        songLength_ = songLength;
        samples_ = std::move(samples);
        if (fxSet_)
        {
            BuildEffects();
        }
    }

    void MusicComposer::SetEffects(const EffectSettings& effects, double duration)
    {
        effects_ = effects;
        if (!plan_.square)
        {
            effects_.motion = 0;
            effects_.outro = 0;
        }
        duration_ = duration;
        fxSet_ = true;
        BuildEffects();
    }

    void MusicComposer::BuildEffects()
    {
        const Rect window = plan_.square ? Rect{plan_.windowX, plan_.windowY, plan_.windowW, plan_.windowH} : Rect{0, 0, plan_.outW, plan_.outH};
        const int k = plan_.scale;
        const Rect client = MeasureFrame(frame_, plan_.clientW, plan_.clientH).client;
        Stage stage{window, k, Rect{window.x + client.x * k, window.y + client.y * k, plan_.clientW * k, plan_.clientH * k}, Rect{}, Rect{}, duration_};
        Rect play;
        Rect seek;
        MusicHotspots(frame_, plan_, recorder_, &play, &seek);
        stage.play = ScaleRect(play, k, window.x + client.x * k, window.y + client.y * k);
        stage.seek = ScaleRect(seek, k, window.x + client.x * k, window.y + client.y * k);
        fx_ = EffectsLayer(effects_, frame_, stage);
        fxOn_ = HasOverlays(effects_);
    }

    void MusicComposer::SetOutro(const Image& ending, const Rect& taskTarget)
    {
        ending_ = ending;
        taskTarget_ = taskTarget;
    }

    const Image& MusicComposer::Outro(double u, int seed)
    {
        out_ = Image(plan_.outW, plan_.outH, Rgb(0, 0, 0));
        fx_.Shift(0, 0);
        fx_.DrawOutro(out_, ending_.Empty() ? background_ : ending_, u, taskTarget_);
        return out_;
    }

    const Image& MusicComposer::Intro(double t, int seed)
    {
        out_ = Image(plan_.outW, plan_.outH, Rgb(0, 0, 0));
        fx_.DrawIntro(out_, plan_.square ? background_ : Image(), t);
        return out_;
    }

    const Image& MusicComposer::Compose(const MusicFrame& f, int seed, bool animate)
    {
        Rect client;
        const Image small = Window(f, client);
        const int k = plan_.scale;
        Image window = ScaleNearest(small, k);
        if (coverBig_.Empty() && !cover_.Empty() && !recorder_)
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
        Spot m;
        if (plan_.square && effects_.motion > 0)
        {
            m = WindowMotion(effects_, f.now, seed, k, Rect{plan_.windowX, plan_.windowY, plan_.windowW, plan_.windowH}, Rect{0, 0, plan_.outW, plan_.outH - plan_.reserve});
        }
        Blit(out_, window, plan_.windowX + m.x, plan_.windowY + m.y);
        if (fxOn_)
        {
            const double progress = recorder_ ? (songLength_ > 0.0 ? std::clamp(f.absolute / songLength_, 0.0, 1.0) : 0.0) : f.position;
            fx_.Shift(m.x, m.y);
            fx_.Apply(out_, f.now, seed, animate, progress);
        }
        return out_;
    }

    Image MusicComposer::Small(const MusicFrame& f, const Image& background, Rect* content, Spot shift)
    {
        Rect client;
        Image window = Window(f, client);
        if (content != nullptr)
        {
            *content = plan_.square ? client.Offset(plan_.windowX / plan_.scale, plan_.windowY / plan_.scale) : client;
        }
        if (!cover_.Empty() && !recorder_)
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
        const int w = plan_.outW / k;
        const int h = plan_.outH / k;
        Image out = background.width == w && background.height == h ? background : Image(w, h, Rgb(0, 0, 0));
        Blit(out, window, plan_.windowX / k + shift.x, plan_.windowY / k + shift.y);
        return out;
    }

    MusicFrame MusicFrameAt(double now, double length, double absolute, bool spin, bool meter, const std::vector<float>& levels)
    {
        MusicFrame f;
        f.now = std::clamp(now, 0.0, std::max(0.0, length));
        f.length = length;
        f.position = length > 0.0 ? f.now / length : 0.0;
        f.absolute = absolute;
        f.angle = spin ? absolute * DiscSpeed : 0.0;
        f.level = meter ? MeterAt(levels, absolute) : 0.0f;
        return f;
    }

    void AddSpectrum(MusicFrame& frame, const std::vector<float>& spectrum, double absolute)
    {
        frame.equalizer = true;
        SpectrumAt(spectrum, absolute, frame.bars, frame.peaks);
    }

    void AddLyrics(MusicFrame& frame, const std::vector<LyricLine>& lines, double absolute)
    {
        frame.lyricsStrip = true;
        const int i = LyricIndex(lines, absolute);
        if (i >= 0)
        {
            frame.lyric = lines[std::size_t(i)].text;
            frame.lyricAge = absolute - lines[std::size_t(i)].time;
        }
    }

    ExportResult ExportMusic(Host& host, const MusicJob& job, const ExportStep& step, std::wstring& error)
    {
        const double fps = std::clamp(job.fps, 1.0, 60.0);
        const int intro = job.effects.intro ? int(std::ceil(IntroSeconds * fps - 1e-6)) : 0;
        const int body = std::max(1, int(std::ceil(job.duration * fps - 1e-6)));
        const int outro = OutroFrames(job.effects, job.plan.square, fps);
        const int total = intro + body + outro;
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
        spec.delay = intro / fps;
        spec.tail = outro / fps;
        MusicComposer composer(job.frame, job.vars, job.plan, job.background, job.artist, job.title, job.cover);
        if (job.recorder)
        {
            composer.SetRecorder(job.recorderRussian, job.songLength, job.samples);
        }
        composer.SetEffects(job.effects, job.duration);
        composer.SetOutro(job.ending, job.taskTarget);
        return Encode(host, spec, total, [&](int i) -> const Image* {
            if (i < intro)
            {
                return &composer.Intro(i / fps, i);
            }
            if (i >= intro + body)
            {
                return &composer.Outro((i - intro - body) / fps, i);
            }
            const double t = (i - intro) / fps;
            MusicFrame mf = MusicFrameAt(t, job.duration, job.start + t, job.spin, job.meter, job.levels);
            if (job.equalizer)
            {
                AddSpectrum(mf, job.spectrum, job.start + t);
            }
            if (!job.lyrics.empty())
            {
                AddLyrics(mf, job.lyrics, job.start + t);
            }
            return &composer.Compose(mf, i, true);
        }, step, error);
    }
}
