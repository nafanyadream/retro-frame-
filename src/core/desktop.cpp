#include "desktop.h"

#include "glyphs.h"
#include "icons.h"

#include <ctime>

namespace rf
{
    namespace
    {
        struct Bar
        {
            Rect bar;
            Rect start;
            Rect task;
            Rect tray;
        };

        const wchar_t* StartText(Style style)
        {
            return style == Style::WinXP ? L"пуск" : L"Пуск";
        }

        Bar Measure(const FrameSettings& frame, int width, int height)
        {
            const bool xp = frame.style == Style::WinXP;
            const int h = TaskbarHeight(frame.style);
            Bar b;
            b.bar = Rect{0, height - h, width, h};
            const Font bold{FamilyFor(frame.style), true};
            const int startW = (xp ? 40 : 28) + Text().Width(StartText(frame.style), bold) + (xp ? 18 : 6);
            b.start = xp ? Rect{0, b.bar.y, startW, h} : Rect{2, b.bar.y + 4, startW, 22};
            const int trayW = Text().Width(L"00:00", Font{FamilyFor(frame.style), false}) + 44;
            b.tray = xp ? Rect{width - trayW, b.bar.y, trayW, h} : Rect{width - trayW - 2, b.bar.y + 4, trayW, 22};
            const int taskX = b.start.Right() + (xp ? 6 : 4);
            const int taskW = std::clamp(b.tray.x - 6 - taskX, 0, 160);
            b.task = Rect{taskX, b.bar.y + (xp ? 3 : 4), taskW, xp ? h - 6 : 22};
            return b;
        }

        void Shadowed(Painter& p, int x, int y, const std::wstring& text, const Font& f, Color c, Color shadow)
        {
            p.Text(x + 1, y + 1, text, f, shadow);
            p.Text(x, y, text, f, c);
        }

        void DrawIcons(Painter& p, const FrameSettings& frame, int height, int bottom)
        {
            struct Item
            {
                IconId icon;
                const wchar_t* label;
            };
            const Item items[] = {{IconId::Monitor, L"Мой компьютер"}, {IconId::Folder, L"Мои документы"}, {IconId::Globe, L"Интернет"}, {IconId::RecycleBin, L"Корзина"}};
            const Font f{FamilyFor(frame.style), false};
            int y = 10;
            for (const Item& it : items)
            {
                if (y + 32 + 30 > bottom || y + 62 > height)
                {
                    break;
                }
                const Image big = ScaleNearest(BuiltinIcon(it.icon), 2);
                p.Draw(big, 12 + (70 - 32) / 2, y);
                std::wstring first = it.label;
                std::wstring second;
                const std::size_t space = first.find(L' ');
                if (Text().Width(first, f) > 74 && space != std::wstring::npos)
                {
                    second = first.substr(space + 1);
                    first.resize(space);
                }
                int ly = y + 35;
                for (const std::wstring& line : {first, second})
                {
                    if (line.empty())
                    {
                        continue;
                    }
                    const std::wstring shown = Ellipsize(line, f, 76);
                    const int tw = Text().Width(shown, f);
                    Shadowed(p, 12 + (70 - tw) / 2, ly, shown, f, Rgb(0xFF, 0xFF, 0xFF), Rgb(0, 0, 0));
                    ly += 13;
                }
                y += 74;
            }
        }

        void DrawClassicBar(Painter& p, const FrameSettings& frame, const Bar& b, const DesktopOptions& d, const std::wstring& task, const Image& taskIcon, bool active)
        {
            const Shades sh = ShadesFor(frame.colors.face, frame.style);
            p.Fill(b.bar, sh.face);
            p.HLine(b.bar.x, b.bar.y, b.bar.w, sh.light);
            p.HLine(b.bar.x, b.bar.y + 1, b.bar.w, sh.hilite);
            const Font bold{FamilyFor(frame.style), true};
            const Font f{FamilyFor(frame.style), false};
            p.Raised(b.start, sh);
            p.Draw(BuiltinIcon(IconId::StartFlag), b.start.x + 3, b.start.y + 3);
            p.Text(b.start.x + 22, b.start.y + (b.start.h - Text().Height(bold)) / 2, StartText(frame.style), bold, frame.colors.text);
            if (b.task.w > 30)
            {
                if (active)
                {
                    p.Pressed(b.task, sh);
                    p.Dither(b.task.Inflate(-2), sh.face, sh.hilite);
                }
                else
                {
                    p.Raised(b.task, sh);
                }
                int tx = b.task.x + 5;
                if (!taskIcon.Empty())
                {
                    p.Draw(taskIcon, b.task.x + 4, b.task.y + 3);
                    tx += 20;
                }
                const Font tf = active ? bold : f;
                p.Text(tx, b.task.y + (b.task.h - Text().Height(tf)) / 2 + (active ? 1 : 0), Ellipsize(task, tf, b.task.Right() - 4 - tx), tf, frame.colors.text);
            }
            p.ThinSunken(b.tray, sh);
            p.Draw(BuiltinIcon(IconId::Speaker), b.tray.x + 3, b.tray.y + 3);
            const std::wstring clock = ClockText(d.clock);
            const int cw = Text().Width(clock, f);
            p.Text(b.tray.Right() - 6 - cw, b.tray.y + (b.tray.h - Text().Height(f)) / 2, clock, f, frame.colors.text);
        }

        void DrawXpBar(Painter& p, const FrameSettings& frame, const Bar& b, const DesktopOptions& d, const std::wstring& task, const Image& taskIcon, bool active)
        {
            const Rect& r = b.bar;
            for (int row = 0; row < r.h; ++row)
            {
                Color c = Mix(Rgb(0x24, 0x5E, 0xDC), Rgb(0x1C, 0x4D, 0xC0), double(row) / double(r.h));
                if (row == 0)
                {
                    c = Rgb(0x0C, 0x3E, 0xB8);
                }
                else if (row < 3)
                {
                    c = Rgb(0x3D, 0x8B, 0xF8);
                }
                p.HLine(r.x, r.y + row, r.w, c);
            }
            const Rect& s = b.start;
            for (int row = 0; row < s.h; ++row)
            {
                const double t = double(row) / double(s.h);
                const Color c = row < 3 ? Rgb(0x6E, 0xC7, 0x6E) : Mix(Rgb(0x3C, 0xA8, 0x3C), Rgb(0x2A, 0x86, 0x2A), t);
                const int dy = std::min(row, s.h - 1 - row);
                const int cut = dy < 6 ? (6 - dy) * (6 - dy) / 6 : 0;
                p.HLine(s.x, s.y + row, s.w - cut, c);
            }
            p.Draw(BuiltinIcon(IconId::StartFlag), s.x + 10, s.y + (s.h - 16) / 2);
            const Font bold{FontFamily::Trebuchet, true};
            Shadowed(p, s.x + 30, s.y + (s.h - Text().Height(bold)) / 2, StartText(frame.style), bold, Rgb(0xFF, 0xFF, 0xFF), Rgb(0x1E, 0x5A, 0x1E));
            const Font f{FamilyFor(frame.style), false};
            if (b.task.w > 30)
            {
                const Rect& t = b.task;
                const Color top = active ? Rgb(0x1E, 0x4F, 0xB8) : Rgb(0x3C, 0x81, 0xF3);
                const Color bottom = active ? Rgb(0x23, 0x5C, 0xD6) : Rgb(0x2C, 0x6C, 0xE6);
                for (int row = 0; row < t.h; ++row)
                {
                    const int cut = row == 0 || row == t.h - 1 ? 2 : row == 1 || row == t.h - 2 ? 1 : 0;
                    p.HLine(t.x + cut, t.y + row, t.w - 2 * cut, Mix(top, bottom, double(row) / double(t.h)));
                }
                p.HLine(t.x + 2, t.y, t.w - 4, active ? Rgb(0x13, 0x3B, 0x9A) : Rgb(0x6E, 0xA6, 0xFF));
                int tx = t.x + 6;
                if (!taskIcon.Empty())
                {
                    p.Draw(taskIcon, t.x + 5, t.y + (t.h - 16) / 2);
                    tx += 20;
                }
                p.Text(tx, t.y + (t.h - Text().Height(f)) / 2, Ellipsize(task, f, t.Right() - 6 - tx), f, Rgb(0xFF, 0xFF, 0xFF));
            }
            const Rect& y = b.tray;
            for (int row = 0; row < y.h; ++row)
            {
                p.HLine(y.x, y.y + row, y.w, row < 2 ? Rgb(0x1B, 0x9B, 0xF0) : Mix(Rgb(0x10, 0x8A, 0xEA), Rgb(0x0B, 0x63, 0xC9), double(row) / double(y.h)));
            }
            p.VLine(y.x, y.y, y.h, Rgb(0x09, 0x45, 0x9E));
            p.VLine(y.x + 1, y.y, y.h, Rgb(0x40, 0xB6, 0xF8));
            p.Draw(BuiltinIcon(IconId::Speaker), y.x + 8, y.y + (y.h - 16) / 2);
            const std::wstring clock = ClockText(d.clock);
            const int cw = Text().Width(clock, f);
            p.Text(y.Right() - 10 - cw, y.y + (y.h - Text().Height(f)) / 2, clock, f, Rgb(0xFF, 0xFF, 0xFF));
        }
    }

    int TaskbarHeight(Style style)
    {
        return style == Style::WinXP ? 30 : 28;
    }

    std::wstring ClockText(const std::wstring& custom)
    {
        if (!custom.empty())
        {
            return custom;
        }
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        wchar_t buf[16];
        std::swprintf(buf, 16, L"%d:%02d", local.tm_hour, local.tm_min);
        return buf;
    }

    Rect TaskButtonRect(const FrameSettings& frame, int width, int height)
    {
        return Measure(frame, width, height).task;
    }

    Image DesktopLayer(const FrameSettings& frame, const DesktopOptions& d, int width, int height, const std::wstring& task, const Image& taskIcon, bool taskActive)
    {
        Image img(std::max(1, width), std::max(1, height));
        Painter p(img);
        const Bar b = Measure(frame, img.width, img.height);
        if (d.icons)
        {
            DrawIcons(p, frame, img.height, d.taskbar ? b.bar.y : img.height);
        }
        if (d.taskbar)
        {
            if (frame.style == Style::WinXP)
            {
                DrawXpBar(p, frame, b, d, task, taskIcon, taskActive);
            }
            else
            {
                DrawClassicBar(p, frame, b, d, task, taskIcon, taskActive);
            }
        }
        return img;
    }

    Image DesktopBackground(const Image& wallpaper, const FrameSettings& frame, const DesktopOptions& d, int outW, int outH, int scale, const std::wstring& task,
                            const Image& taskIcon, bool taskActive)
    {
        const int k = std::max(1, scale);
        Image out = wallpaper.width == outW && wallpaper.height == outH ? wallpaper : Image(outW, outH, Rgb(0, 0x80, 0x80));
        const int w = (outW + k - 1) / k;
        const int h = outH / k;
        const Image layer = ScaleNearest(DesktopLayer(frame, d, w, h, task, taskIcon, taskActive), k);
        Painter(out).Draw(layer, 0, outH - layer.height);
        return out;
    }
}
