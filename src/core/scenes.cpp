#include "scenes.h"

#include "glyphs.h"
#include "icons.h"

#include <cmath>
#include <cwchar>
#include <unordered_map>

namespace rf
{
    namespace
    {
        const double NotepadSpeeds[NotepadSpeedCount] = {6.0, 11.0, 18.0, 30.0};
        const int NotepadWidths[NotepadSizeCount] = {320, 420, 560};
        const int NotepadHeights[NotepadSizeCount] = {180, 240, 320};
        constexpr double CaretBlink = 0.53;

        class Rng
        {
        public:
            explicit Rng(std::uint32_t seed) : s_(seed == 0 ? 1u : seed) {}

            std::uint32_t Next()
            {
                s_ ^= s_ << 13;
                s_ ^= s_ >> 17;
                s_ ^= s_ << 5;
                return s_;
            }

            double Unit() { return double(Next() >> 8) / double(1u << 24); }
            double Signed() { return Unit() * 2.0 - 1.0; }

        private:
            std::uint32_t s_;
        };

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

        int CharWidth(wchar_t ch, const Font& f)
        {
            static std::unordered_map<std::uint64_t, int> cache;
            const std::uint64_t key = (std::uint64_t(ch) << 8) | (std::uint64_t(f.family) << 1) | (f.bold ? 1u : 0u);
            const auto it = cache.find(key);
            if (it != cache.end())
            {
                return it->second;
            }
            const int w = Text().Width(std::wstring(1, ch), f);
            if (cache.size() < 4096)
            {
                cache.emplace(key, w);
            }
            return w;
        }

        std::vector<std::wstring> Wrap(const std::wstring& text, const Font& f, int width)
        {
            std::vector<std::wstring> lines;
            std::size_t ps = 0;
            while (ps <= text.size())
            {
                std::size_t pe = text.find(L'\n', ps);
                if (pe == std::wstring::npos)
                {
                    pe = text.size();
                }
                const std::wstring para = text.substr(ps, pe - ps);
                if (para.empty())
                {
                    lines.push_back(std::wstring());
                }
                std::size_t s = 0;
                while (s < para.size())
                {
                    int w = 0;
                    std::size_t e = s;
                    while (e < para.size() && w + CharWidth(para[e], f) <= width)
                    {
                        w += CharWidth(para[e], f);
                        ++e;
                    }
                    if (e == s)
                    {
                        e = s + 1;
                    }
                    if (e < para.size() && para[e] != L' ')
                    {
                        std::size_t space = e;
                        while (space > s && para[space - 1] != L' ')
                        {
                            --space;
                        }
                        if (space > s)
                        {
                            e = space;
                        }
                    }
                    while (e < para.size() && para[e] == L' ')
                    {
                        ++e;
                    }
                    lines.push_back(para.substr(s, e - s));
                    s = e;
                }
                if (pe == text.size())
                {
                    break;
                }
                ps = pe + 1;
            }
            if (lines.empty())
            {
                lines.push_back(std::wstring());
            }
            return lines;
        }

        int LineWidth(const std::wstring& line, const Font& f)
        {
            int w = 0;
            for (wchar_t ch : line)
            {
                w += CharWidth(ch, f);
            }
            return w;
        }

        void XpGradientBox(Painter& p, const Rect& r, Color top, Color bottom, Color border)
        {
            for (int row = 0; row < r.h; ++row)
            {
                p.HLine(r.x + 1, r.y + row, r.w - 2, Mix(top, bottom, r.h > 1 ? double(row) / double(r.h - 1) : 0.0));
            }
            p.HLine(r.x + 1, r.y, r.w - 2, border);
            p.HLine(r.x + 1, r.Bottom() - 1, r.w - 2, border);
            p.VLine(r.x, r.y + 1, r.h - 2, border);
            p.VLine(r.Right() - 1, r.y + 1, r.h - 2, border);
        }

        void DrawVScroll(Painter& p, const Rect& r, const FrameSettings& frame, const Shades& sh, int total, int visible, int first)
        {
            const bool xp = frame.style == Style::WinXP;
            const Rect up{r.x, r.y, r.w, std::min(16, r.h / 2)};
            const Rect down{r.x, r.Bottom() - up.h, r.w, up.h};
            const Rect track{r.x, up.Bottom(), r.w, std::max(0, down.y - up.Bottom())};
            const bool active = total > visible;
            if (xp)
            {
                p.Fill(r, Rgb(0xF3, 0xF1, 0xEC));
                XpGradientBox(p, up, Rgb(0xE1, 0xEA, 0xFE), Rgb(0xAE, 0xC8, 0xF7), Rgb(0xBA, 0xCC, 0xF5));
                XpGradientBox(p, down, Rgb(0xE1, 0xEA, 0xFE), Rgb(0xAE, 0xC8, 0xF7), Rgb(0xBA, 0xCC, 0xF5));
                const Color arrow = active ? Rgb(0x4D, 0x61, 0x85) : Rgb(0xA0, 0xAC, 0xC8);
                p.DrawGlyph(up.x + (up.w - 7) / 2, up.y + (up.h - 4) / 2, glyphs::Up, arrow);
                p.DrawGlyph(down.x + (down.w - 7) / 2, down.y + (down.h - 4) / 2, glyphs::Down, arrow);
            }
            else
            {
                p.Dither(track, sh.face, sh.hilite);
                p.Raised(up, sh);
                p.Raised(down, sh);
                const Color arrow = active ? frame.colors.text : sh.shadow;
                p.DrawGlyph(up.x + (up.w - 7) / 2, up.y + (up.h - 4) / 2, glyphs::Up, arrow);
                p.DrawGlyph(down.x + (down.w - 7) / 2, down.y + (down.h - 4) / 2, glyphs::Down, arrow);
            }
            if (!active || track.h < 10)
            {
                return;
            }
            const int size = std::max(8, track.h * visible / std::max(1, total));
            const int pos = track.y + (track.h - size) * first / std::max(1, total - visible);
            const Rect thumb{track.x, pos, track.w, size};
            if (xp)
            {
                XpGradientBox(p, thumb, Rgb(0xC9, 0xDA, 0xFC), Rgb(0xB0, 0xC8, 0xF8), Rgb(0x9E, 0xB6, 0xEE));
                for (int i = -2; i <= 2; i += 2)
                {
                    p.HLine(thumb.x + 4, thumb.y + thumb.h / 2 + i, thumb.w - 8, Rgb(0xEE, 0xF4, 0xFE));
                }
            }
            else
            {
                p.Raised(thumb, sh);
            }
        }

        void ButtonText(Painter& p, const Rect& r, const std::wstring& text, const FrameSettings& frame, const Shades& sh, bool enabled)
        {
            DrawPushButton(p, r, frame, sh, false);
            const Font f{FamilyFor(frame.style), false};
            const int tw = Text().Width(text, f);
            const int tx = r.x + (r.w - tw) / 2;
            const int ty = r.y + (r.h - Text().Height(f)) / 2;
            if (enabled)
            {
                p.Text(tx, ty, text, f, frame.colors.text);
            }
            else
            {
                p.TextEmbossed(tx, ty, text, f, sh);
            }
        }

        void DrawBlocks(Painter& p, const Rect& r, const FrameSettings& frame, const Shades& sh, double value)
        {
            const bool xp = frame.style == Style::WinXP;
            const Rect inner = r.Inflate(-2);
            if (xp)
            {
                p.Fill(r, Rgb(0xFF, 0xFF, 0xFF));
                p.Frame(r, Rgb(0x68, 0x68, 0x68));
                p.Frame(r.Inflate(-1), Rgb(0xEC, 0xEC, 0xEC));
            }
            else
            {
                p.ThinSunken(r, sh);
                p.Fill(r.Inflate(-1), sh.face);
            }
            const int block = xp ? 8 : std::max(6, inner.h * 2 / 3);
            const int gap = 2;
            const int fill = int(std::lround(std::clamp(value, 0.0, 1.0) * inner.w));
            for (int x = 0; x < fill; x += block + gap)
            {
                const int w = std::min(block, inner.w - x);
                if (w <= 0)
                {
                    break;
                }
                if (xp)
                {
                    for (int row = 0; row < inner.h; ++row)
                    {
                        const double t = std::fabs(double(row) / std::max(1, inner.h - 1) - 0.35);
                        p.HLine(inner.x + x, inner.y + row, w, Mix(Rgb(0x9C, 0xEE, 0x9C), Rgb(0x2C, 0xB0, 0x2C), std::min(1.0, t * 1.6)));
                    }
                }
                else
                {
                    p.Fill(Rect{inner.x + x, inner.y, w, inner.h}, frame.colors.active1);
                }
            }
        }

        void DrawSmoothBar(Painter& p, const Rect& r, const FrameSettings& frame, const Shades& sh, double value)
        {
            const Color white = Rgb(0xFF, 0xFF, 0xFF);
            p.Sunken(r, sh, &white);
            const Rect inner = r.Inflate(-2);
            const int fill = int(std::lround(std::clamp(value, 0.0, 1.0) * inner.w));
            const Color bar = frame.style == Style::WinXP ? Rgb(0x31, 0x6A, 0xC5) : frame.colors.active1;
            p.Fill(Rect{inner.x, inner.y, fill, inner.h}, bar);
            const Font f{FamilyFor(frame.style), true};
            const std::wstring text = std::to_wstring(int(std::floor(std::clamp(value, 0.0, 1.0) * 100.0 + 1e-6))) + L"%";
            const int tx = inner.x + (inner.w - Text().Width(text, f)) / 2;
            const int ty = inner.y + (inner.h - Text().Height(f)) / 2;
            p.Clipped(Rect{inner.x + fill, inner.y, inner.w - fill, inner.h}).Text(tx, ty, text, f, bar);
            p.Clipped(Rect{inner.x, inner.y, fill, inner.h}).Text(tx, ty, text, f, white);
        }

        double Smooth(double q)
        {
            q = std::clamp(q, 0.0, 1.0);
            return q * q * (3.0 - 2.0 * q);
        }

        double Jerky(double f, std::uint32_t seed)
        {
            f = std::clamp(f, 0.0, 1.0);
            Rng rng(seed);
            constexpr int Steps = 14;
            double times[Steps];
            double gains[Steps];
            double tSum = 0.0;
            double gSum = 0.0;
            for (int i = 0; i < Steps; ++i)
            {
                times[i] = 0.3 + 1.4 * rng.Unit();
                gains[i] = 0.2 + 1.6 * rng.Unit();
                tSum += times[i];
                gSum += gains[i];
            }
            double t0 = 0.0;
            double p = 0.0;
            for (int i = 0; i < Steps; ++i)
            {
                const double len = times[i] / tSum;
                const double gain = gains[i] / gSum;
                if (f <= t0 + len)
                {
                    const double local = (f - t0) / len;
                    const double stall = 0.35 + 0.3 * std::fmod(gains[i] * 7.31, 1.0);
                    return p + gain * Smooth((local - stall) / (1.0 - stall));
                }
                t0 += len;
                p += gain;
            }
            return 1.0;
        }

        std::wstring Remaining(double seconds)
        {
            const long long s = std::max(1LL, (long long)std::llround(seconds));
            if (s < 60)
            {
                return L"Осталось " + std::to_wstring(s) + L" сек.";
            }
            return L"Осталось " + std::to_wstring((s + 30) / 60) + L" мин.";
        }

        std::wstring FileAt(const SetupOptions& o, double progress)
        {
            const std::vector<std::wstring> files = SetupFiles(o);
            const int i = std::clamp(int(progress * double(files.size())), 0, int(files.size()) - 1);
            return files[std::size_t(i)];
        }

        void Mix16(std::vector<std::int16_t>& out, std::size_t at, double v)
        {
            if (at >= out.size())
            {
                return;
            }
            const double s = out[at] + v * 32767.0;
            out[at] = std::int16_t(std::lround(std::clamp(s, -32768.0, 32767.0)));
        }
    }

    std::vector<Keystroke> NotepadKeys(const NotepadOptions& o)
    {
        std::vector<Keystroke> keys;
        const double cps = NotepadSpeeds[std::clamp(o.speed, 0, NotepadSpeedCount - 1)];
        Rng rng(0x7E57u);
        const auto dt = [&]() { return (0.55 + 0.9 * rng.Unit()) / cps; };
        double t = 0.6;
        std::wstring text;
        for (std::size_t i = 0; i < o.text.size(); ++i)
        {
            const wchar_t ch = o.text[i];
            if (ch == L'\r')
            {
                if (i + 1 >= o.text.size() || o.text[i + 1] != L'\n')
                {
                    text.push_back(L'\n');
                }
                continue;
            }
            text.push_back(ch);
        }
        for (wchar_t ch : text)
        {
            const bool cyr = ch >= 0x0400 && ch <= 0x04FF;
            const bool latin = (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
            if (o.typos && (cyr || latin) && rng.Unit() < 0.05)
            {
                const wchar_t* row = cyr ? L"фывапролджэйцукенгшщзхъячсмитьбю" : L"asdfghjklqwertyuiopzxcvbnm";
                const std::size_t n = std::wcslen(row);
                wchar_t wrong = row[rng.Next() % n];
                if (wrong == ch)
                {
                    wrong = row[(rng.Next() + 1) % n];
                }
                keys.push_back(Keystroke{t, wrong, false});
                t += dt() + 0.25 + 0.2 * rng.Unit();
                keys.push_back(Keystroke{t, 0, true});
                t += dt();
            }
            keys.push_back(Keystroke{t, ch, false});
            double pause = dt();
            if (ch == L'\n')
            {
                pause += 0.35;
            }
            else if (ch == L'.' || ch == L'!' || ch == L'?' || ch == L',')
            {
                pause += 0.18;
            }
            t += pause;
        }
        return keys;
    }

    double NotepadDuration(const NotepadOptions& o, const std::vector<Keystroke>& keys)
    {
        const double last = keys.empty() ? 0.6 : keys.back().t;
        return last + std::max(0.5, o.hold);
    }

    std::wstring NotepadTextAt(const std::vector<Keystroke>& keys, double t)
    {
        std::wstring s;
        for (const Keystroke& k : keys)
        {
            if (k.t > t)
            {
                break;
            }
            if (k.erase)
            {
                if (!s.empty())
                {
                    s.pop_back();
                }
            }
            else
            {
                s.push_back(k.ch);
            }
        }
        return s;
    }

    bool NotepadCaretAt(const NotepadOptions& o, const std::vector<Keystroke>& keys, double t)
    {
        if (!o.caret)
        {
            return false;
        }
        double last = 0.0;
        for (const Keystroke& k : keys)
        {
            if (k.t > t)
            {
                break;
            }
            last = k.t;
        }
        const double since = std::max(0.0, t - last);
        return since < CaretBlink || int(since / CaretBlink) % 2 == 0;
    }

    void NotepadClient(const NotepadOptions& o, int* width, int* height)
    {
        const int i = std::clamp(o.size, 0, NotepadSizeCount - 1);
        *width = NotepadWidths[i];
        *height = NotepadHeights[i];
    }

    std::wstring NotepadTitle(const std::wstring& file, bool russian)
    {
        const std::wstring name = file.empty() ? std::wstring(russian ? L"Безымянный" : L"Untitled") : file;
        return name + (russian ? L" - Блокнот" : L" - Notepad");
    }

    std::vector<std::wstring> NotepadMenu(bool russian, Style style)
    {
        if (style == Style::WinXP)
        {
            return russian ? std::vector<std::wstring>{L"Файл", L"Правка", L"Формат", L"Вид", L"Справка"}
                           : std::vector<std::wstring>{L"File", L"Edit", L"Format", L"View", L"Help"};
        }
        return russian ? std::vector<std::wstring>{L"Файл", L"Правка", L"Поиск", L"Справка"} : std::vector<std::wstring>{L"File", L"Edit", L"Search", L"Help"};
    }

    Font NotepadFont(const NotepadOptions& o, Style style)
    {
        if (o.font == 1)
        {
            return Font{FontFamily::Fixedsys, false};
        }
        if (o.font == 2)
        {
            return Font{FontFamily::Lucida, false};
        }
        return Font{style == Style::WinXP || style == Style::Win2000 ? FontFamily::Lucida : FontFamily::Fixedsys, false};
    }

    void DrawNotepad(Painter& p, const NotepadOptions& o, int width, int height, const std::wstring& shown, bool caretOn, const FrameSettings& frame)
    {
        const Shades sh = ShadesFor(frame.colors.face, frame.style);
        const Color white = Rgb(0xFF, 0xFF, 0xFF);
        if (frame.style == Style::WinXP)
        {
            p.Fill(Rect{0, 0, width, height}, white);
            p.Frame(Rect{0, 0, width, height}, Rgb(0x7F, 0x9D, 0xB9));
        }
        else
        {
            p.Sunken(Rect{0, 0, width, height}, sh, &white);
        }
        const int bar = 16;
        const Rect area{2, 2, width - 4 - bar, height - 4};
        const Font f = NotepadFont(o, frame.style);
        const int lineH = std::max(1, Text().Height(f));
        const std::vector<std::wstring> lines = Wrap(shown, f, area.w - 6);
        const int visible = std::max(1, (area.h - 2) / lineH);
        const int total = int(lines.size());
        const int first = std::max(0, total - visible);
        Painter c = p.Clipped(area);
        for (int i = first; i < total; ++i)
        {
            c.Text(area.x + 2, area.y + 1 + (i - first) * lineH, lines[std::size_t(i)], f, Rgb(0, 0, 0));
        }
        if (caretOn)
        {
            const std::wstring& last = lines.back();
            const int cx = area.x + 2 + LineWidth(last, f);
            const int cy = area.y + 1 + (total - 1 - first) * lineH;
            c.Fill(Rect{std::min(cx, area.Right() - 2), cy, f.family == FontFamily::Fixedsys ? 2 : 1, lineH}, Rgb(0, 0, 0));
        }
        DrawVScroll(p, Rect{width - 2 - bar, 2, bar, height - 4}, frame, sh, total, visible, first);
    }

    std::vector<std::wstring> SetupFiles(const SetupOptions& o)
    {
        std::vector<std::wstring> files;
        std::wstring cur;
        for (wchar_t ch : o.files + L"\n")
        {
            if (ch == L'\n' || ch == L'\r' || ch == L',')
            {
                while (!cur.empty() && cur.front() == L' ')
                {
                    cur.erase(cur.begin());
                }
                while (!cur.empty() && cur.back() == L' ')
                {
                    cur.pop_back();
                }
                if (!cur.empty())
                {
                    files.push_back(cur);
                }
                cur.clear();
                continue;
            }
            cur.push_back(ch);
        }
        if (files.empty())
        {
            if (o.kind == int(SetupKind::Install))
            {
                files = {L"setup.dll", L"retro.exe", L"readme.txt", L"help.hlp", L"sounds.cab", L"data1.cab"};
            }
            else
            {
                files = {L"реферат.doc", L"фото с дачи.jpg", L"песня.mp3", L"игра.exe"};
            }
        }
        return files;
    }

    double SetupProgress(const SetupOptions& o, double t)
    {
        const double seconds = std::max(1.0, o.seconds);
        const double f = t / seconds;
        if (f >= 1.0)
        {
            return o.pace == 2 && o.ending == 2 ? 0.99 : 1.0;
        }
        if (f <= 0.0)
        {
            return 0.0;
        }
        if (o.pace == 0)
        {
            return f;
        }
        if (o.pace == 2)
        {
            return f < 0.55 ? 0.99 * Jerky(f / 0.55, 0x51u) : 0.99;
        }
        return Jerky(f, 0xC0FFEEu);
    }

    double SetupDuration(const SetupOptions& o)
    {
        return std::max(1.0, o.seconds) + (o.ending > 0 ? 2.6 : 1.0);
    }

    void SetupClient(const SetupOptions& o, int* width, int* height)
    {
        if (o.kind == int(SetupKind::Install))
        {
            *width = 400;
            *height = 176;
            return;
        }
        *width = 360;
        *height = 156;
    }

    std::wstring SetupTitle(const SetupOptions& o)
    {
        if (o.kind == int(SetupKind::Install))
        {
            return L"Установка " + (o.program.empty() ? std::wstring(L"программы") : o.program);
        }
        return o.kind == int(SetupKind::Delete) ? L"Удаление..." : L"Копирование...";
    }

    EffectSettings SetupEndingPopup(const SetupOptions& o)
    {
        EffectSettings e;
        e.popups = 1;
        e.popupButtons = 0;
        const std::wstring file = FileAt(o, 0.999);
        const std::wstring program = o.program.empty() ? std::wstring(L"программа") : o.program;
        if (o.ending == 2)
        {
            e.popupIcon = 0;
            if (o.kind == int(SetupKind::Install))
            {
                e.popupTitle = L"Ошибка установки";
                e.popupText = L"Ошибка 1603: не удалось установить «" + program + L"».\nОбратитесь к системному администратору.";
            }
            else if (o.kind == int(SetupKind::Delete))
            {
                e.popupTitle = L"Ошибка при удалении файла";
                e.popupText = L"Не удаётся удалить «" + file + L"»:\nфайл используется другой программой.";
            }
            else
            {
                e.popupTitle = L"Ошибка при копировании файла";
                e.popupText = L"Не удаётся скопировать «" + file + L"»:\nдиск переполнен.";
            }
            return e;
        }
        e.popupIcon = 2;
        if (o.kind == int(SetupKind::Install))
        {
            e.popupTitle = L"Установка";
            e.popupText = L"Установка «" + program + L"» успешно завершена.";
        }
        else if (o.kind == int(SetupKind::Delete))
        {
            e.popupTitle = L"Удаление";
            e.popupText = L"Файлы удалены в Корзину.";
        }
        else
        {
            e.popupTitle = L"Копирование";
            e.popupText = L"Копирование файлов завершено.";
        }
        return e;
    }

    void DrawSetup(Painter& p, const SetupOptions& o, int width, int height, double t, const FrameSettings& frame)
    {
        const Shades sh = ShadesFor(frame.colors.face, frame.style);
        const Font f{FamilyFor(frame.style), false};
        const Font bold{FamilyFor(frame.style), true};
        const Color ink = frame.colors.text;
        const double seconds = std::max(1.0, o.seconds);
        const double progress = SetupProgress(o, t);
        const bool running = t < seconds;
        p.Fill(Rect{0, 0, width, height}, sh.face);
        const std::wstring file = FileAt(o, progress);
        if (o.kind == int(SetupKind::Install))
        {
            const Color white = Rgb(0xFF, 0xFF, 0xFF);
            p.Fill(Rect{0, 0, width, 54}, white);
            const std::wstring program = o.program.empty() ? std::wstring(L"программы") : o.program;
            p.Text(14, 10, Ellipsize(L"Установка «" + program + L"»", bold, width - 70), bold, Rgb(0, 0, 0));
            p.Text(24, 30, Ellipsize(L"Подождите, идёт копирование файлов программы.", f, width - 80), f, Rgb(0, 0, 0));
            p.Draw(ScaleNearest(BuiltinIcon(IconId::Disc), 2), width - 46, 11);
            p.EtchedH(0, 54, width, sh);
            p.Text(14, 66, running ? L"Копирование файлов:" : L"Копирование завершено.", f, ink);
            const std::wstring path = L"C:\\Program Files\\" + program + L"\\" + file;
            p.Text(14, 84, Ellipsize(path, f, width - 28), f, ink);
            DrawSmoothBar(p, Rect{14, 104, width - 28, 18}, frame, sh, progress);
            const Rect cancel{width - 14 - 75, 140, 75, 23};
            ButtonText(p, Rect{cancel.x - 2 * 81, cancel.y, 75, 23}, L"< Назад", frame, sh, false);
            ButtonText(p, Rect{cancel.x - 81, cancel.y, 75, 23}, L"Далее >", frame, sh, false);
            ButtonText(p, cancel, L"Отмена", frame, sh, running);
            return;
        }
        const bool remove = o.kind == int(SetupKind::Delete);
        const Image source = ScaleNearest(BuiltinIcon(IconId::Folder), 2);
        const bool floppy = !o.to.empty() && (o.to[0] == L'A' || o.to[0] == L'a' || o.to[0] == L'B' || o.to[0] == L'b');
        const Image target = ScaleNearest(BuiltinIcon(remove ? IconId::RecycleBin : floppy ? IconId::Floppy : IconId::Folder), 2);
        const int sx = 22;
        const int tx = width - 22 - 32;
        p.Draw(source, sx, 12);
        p.Draw(target, tx, 12);
        if (running)
        {
            const Image& paper = BuiltinIcon(IconId::Document);
            for (int i = 0; i < 3; ++i)
            {
                const double u = std::fmod(t / 1.2 + i / 3.0, 1.0);
                const int x = int(std::lround(sx + 24 + (tx - sx - 32) * u));
                const int y = int(std::lround(20 - std::sin(u * 3.14159265358979) * 14.0));
                p.Draw(paper, x, y);
            }
        }
        p.Text(14, 58, Ellipsize(file, f, width - 28), f, ink);
        const std::wstring from = o.from.empty() ? std::wstring(L"Мои документы") : o.from;
        const std::wstring to = o.to.empty() ? std::wstring(L"A:\\") : o.to;
        const std::wstring route = remove ? L"Из «" + from + L"»" : L"Из «" + from + L"» в «" + to + L"»";
        p.Text(14, 76, Ellipsize(route, f, width - 28), f, ink);
        DrawBlocks(p, Rect{14, 96, width - 28, 16}, frame, sh, progress);
        std::wstring left;
        if (running)
        {
            double rest = (1.0 - progress) * seconds * 1.4 + 1.0;
            if (o.pace == 2 && t > seconds * 0.55)
            {
                rest = 60.0 + (t - seconds * 0.55) * 140.0;
            }
            left = Remaining(rest);
        }
        p.Text(14, 124, left, f, ink);
        ButtonText(p, Rect{width - 14 - 75, 120, 75, 23}, L"Отмена", frame, sh, running);
    }

    std::vector<std::int16_t> KeyboardSound(const std::vector<Keystroke>& keys, double duration, int rate)
    {
        std::vector<std::int16_t> out(std::size_t(std::max(1.0, duration * rate)), 0);
        Rng rng(0xC11CC1u);
        const double pi = 3.14159265358979;
        for (const Keystroke& k : keys)
        {
            const bool big = k.ch == L' ' || k.ch == L'\n' || k.erase;
            const double amp = (big ? 0.42 : 0.3) * (0.75 + 0.5 * rng.Unit());
            const double tone = (big ? 120.0 : 170.0) + 70.0 * rng.Unit();
            const double click = 2200.0 + 1800.0 * rng.Unit();
            const std::size_t at = std::size_t(std::max(0.0, k.t * rate));
            const int len = int(0.045 * rate);
            double low = 0.0;
            for (int i = 0; i < len; ++i)
            {
                const double tt = double(i) / rate;
                low += (rng.Signed() - low) * 0.35;
                const double noise = low * std::exp(-tt / 0.0035);
                const double tick = std::sin(2.0 * pi * click * tt) * std::exp(-tt / 0.0015);
                const double thump = std::sin(2.0 * pi * tone * tt) * std::exp(-tt / 0.012);
                Mix16(out, at + std::size_t(i), amp * (0.55 * noise + 0.25 * tick + 0.45 * thump));
            }
        }
        return out;
    }

    void AddChime(std::vector<std::int16_t>& samples, double at, bool error, int rate)
    {
        const double pi = 3.14159265358979;
        const std::size_t start = std::size_t(std::max(0.0, at * rate));
        const int len = int((error ? 0.9 : 0.7) * rate);
        const double notes[] = {error ? 392.0 : 880.0, error ? 523.25 : 1318.5, error ? 659.25 : 1760.0};
        const double gains[] = {0.24, error ? 0.2 : 0.12, error ? 0.16 : 0.06};
        for (int i = 0; i < len; ++i)
        {
            const double tt = double(i) / rate;
            const double env = std::min(1.0, tt / 0.004) * std::exp(-tt / (error ? 0.32 : 0.22));
            double v = 0.0;
            for (int n = 0; n < 3; ++n)
            {
                v += gains[n] * std::sin(2.0 * pi * notes[n] * tt);
            }
            Mix16(samples, start + std::size_t(i), v * env);
        }
    }

    ScenePlan PlanScene(const FrameSettings& frame, int clientW, int clientH, int canvasW, int canvasH, bool square, int reserve, int gifWidth)
    {
        ScenePlan p;
        p.square = square;
        p.clientW = std::max(16, clientW);
        p.clientH = std::max(16, clientH);
        int outW = std::max(120, canvasW & ~1);
        int outH = std::max(120, canvasH & ~1);
        if (square && gifWidth > 0 && gifWidth < outW)
        {
            outH = std::max(120, int(std::lround(double(outH) * gifWidth / outW)) & ~1);
            outW = std::max(120, gifWidth & ~1);
        }
        const FrameGeometry g = MeasureFrame(frame, p.clientW, p.clientH);
        int roomW = square ? outW * 92 / 100 : outW;
        if (!square && gifWidth > 0)
        {
            roomW = std::min(roomW, gifWidth);
        }
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
        p.scale = manual && (frame.scale <= fits || (!square && gifWidth <= 0)) ? frame.scale : fits;
        const int k = p.scale;
        if (square)
        {
            p.reserve = extra * k;
            p.windowW = g.width * k;
            p.windowH = g.height * k;
            p.outW = outW;
            p.outH = outH;
            p.windowX = std::max(0, (outW - p.windowW) / 2);
            p.windowY = std::max(0, (outH - p.reserve - p.windowH) / 2);
            return p;
        }
        if ((g.width * k) % 2 != 0)
        {
            ++p.clientW;
        }
        if ((g.height * k) % 2 != 0)
        {
            ++p.clientH;
        }
        const FrameGeometry even = MeasureFrame(frame, p.clientW, p.clientH);
        p.windowW = even.width * k;
        p.windowH = even.height * k;
        p.outW = p.windowW;
        p.outH = p.windowH;
        return p;
    }

    SceneComposer::SceneComposer(const FrameSettings& frame, const FrameVars& vars, const ScenePlan& plan, const Image& background, SceneDraw draw)
        : frame_(frame), vars_(vars), plan_(plan), background_(background), draw_(std::move(draw))
    {
    }

    void SceneComposer::SetEffects(const EffectSettings& effects, double duration)
    {
        effects_ = effects;
        if (!plan_.square)
        {
            effects_.motion = 0;
            effects_.outro = 0;
        }
        const Rect window = plan_.square ? Rect{plan_.windowX, plan_.windowY, plan_.windowW, plan_.windowH} : Rect{0, 0, plan_.outW, plan_.outH};
        const int k = plan_.scale;
        const Rect client = MeasureFrame(frame_, plan_.clientW, plan_.clientH).client;
        const Stage stage{window, k, ScaleRect(client, k, window.x, window.y), Rect{}, Rect{}, duration};
        fx_ = EffectsLayer(effects_, frame_, stage);
        fxOn_ = HasOverlays(effects_);
    }

    void SceneComposer::SetOutro(const Image& ending, const Rect& taskTarget)
    {
        ending_ = ending;
        taskTarget_ = taskTarget;
    }

    void SceneComposer::SetPopup(const EffectSettings& popup, double from)
    {
        popup_ = from >= 0.0 ? RenderPopup(popup, frame_, true, nullptr) : Image();
        popupFrom_ = from;
    }

    Image SceneComposer::Window(double t, Rect& client) const
    {
        Image img = RenderFrame(frame_, plan_.clientW, plan_.clientH, vars_, &client);
        Painter p(img);
        Painter c = p.Sub(client);
        if (draw_)
        {
            draw_(c, plan_.clientW, plan_.clientH, t);
        }
        return img;
    }

    void SceneComposer::DrawPopup(Image& out, double t, int k, const Rect& window) const
    {
        if (popup_.Empty() || popupFrom_ < 0.0 || t < popupFrom_)
        {
            return;
        }
        const Image big = k > 1 ? ScaleNearest(popup_, k) : popup_;
        const int x = std::clamp(window.x + (window.w - big.width) / 2, 0, std::max(0, out.width - big.width));
        const int y = std::clamp(window.y + (window.h - big.height) / 2, 0, std::max(0, out.height - big.height));
        Blit(out, big, x, y);
    }

    const Image& SceneComposer::Compose(double t, int seed, bool animate)
    {
        Rect client;
        const Image small = Window(t, client);
        const int k = plan_.scale;
        const Image window = k > 1 ? ScaleNearest(small, k) : small;
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
            m = WindowMotion(effects_, t, seed, k, Rect{plan_.windowX, plan_.windowY, plan_.windowW, plan_.windowH}, Rect{0, 0, plan_.outW, plan_.outH - plan_.reserve});
        }
        Blit(out_, window, plan_.windowX + m.x, plan_.windowY + m.y);
        DrawPopup(out_, t, k, Rect{plan_.windowX + m.x, plan_.windowY + m.y, window.width, window.height});
        if (fxOn_)
        {
            fx_.Shift(m.x, m.y);
            fx_.Apply(out_, t, seed, animate);
        }
        return out_;
    }

    const Image& SceneComposer::Intro(double t, int seed)
    {
        out_ = Image(plan_.outW, plan_.outH, Rgb(0, 0, 0));
        fx_.DrawIntro(out_, plan_.square ? background_ : Image(), t);
        return out_;
    }

    const Image& SceneComposer::Outro(double u, int seed)
    {
        out_ = Image(plan_.outW, plan_.outH, Rgb(0, 0, 0));
        fx_.Shift(0, 0);
        fx_.DrawOutro(out_, ending_.Empty() ? background_ : ending_, u, taskTarget_);
        return out_;
    }

    Image SceneComposer::Small(double t, const Image& background, Rect* window, Rect* content, Spot shift)
    {
        Rect client;
        Image win = Window(t, client);
        const int k = std::max(1, plan_.scale);
        if (!plan_.square)
        {
            if (window != nullptr)
            {
                *window = Rect{0, 0, win.width, win.height};
            }
            if (content != nullptr)
            {
                *content = client;
            }
            DrawPopup(win, t, 1, Rect{0, 0, win.width, win.height});
            return win;
        }
        const int w = plan_.outW / k;
        const int h = plan_.outH / k;
        Image out = background.width == w && background.height == h ? background : Image(w, h, Rgb(0, 0, 0));
        const Rect at{plan_.windowX / k, plan_.windowY / k, win.width, win.height};
        Blit(out, win, at.x + shift.x, at.y + shift.y);
        DrawPopup(out, t, 1, at.Offset(shift.x, shift.y));
        if (window != nullptr)
        {
            *window = at;
        }
        if (content != nullptr)
        {
            *content = client.Offset(at.x, at.y);
        }
        return out;
    }

    ExportResult ExportScene(Host& host, const SceneJob& job, const ExportStep& step, std::wstring& error)
    {
        const double fps = std::clamp(job.fps, 1.0, 60.0);
        const int intro = job.effects.intro ? int(std::ceil(IntroSeconds * fps - 1e-6)) : 0;
        const int body = std::max(1, int(std::ceil(job.duration * fps - 1e-6)));
        const int outro = job.gif ? 0 : OutroFrames(job.effects, job.plan.square, fps);
        const int total = intro + body + outro;
        EncoderSpec spec;
        spec.output = job.output;
        spec.gif = job.gif;
        spec.quality = job.quality;
        spec.width = job.plan.outW;
        spec.height = job.plan.outH;
        spec.fps = fps;
        spec.audio = !job.gif && !job.audio.empty();
        spec.source = job.audio;
        spec.duration = job.duration;
        spec.delay = intro / fps;
        spec.tail = outro / fps;
        SceneComposer composer(job.frame, job.vars, job.plan, job.background, job.draw);
        composer.SetEffects(job.effects, job.duration);
        composer.SetOutro(job.ending, job.taskTarget);
        composer.SetPopup(job.popup, job.popupFrom);
        return Encode(host, spec, total, [&](int i) -> const Image* {
            if (i < intro)
            {
                return &composer.Intro(i / fps, i);
            }
            if (i < intro + body)
            {
                return &composer.Compose(std::min(job.duration, (i - intro) / fps), i, true);
            }
            return &composer.Outro((i - intro - body) / fps, i);
        }, step, error);
    }
}
