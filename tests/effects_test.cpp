#include "test_support.h"

#include "../src/core/desktop.h"
#include "../src/core/dialogs.h"
#include "../src/core/icons.h"
#include "../src/core/mainview.h"
#include "../src/core/music.h"
#include "test_host.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    int failures = 0;
    std::string outDir;

    const std::string& Out()
    {
        return outDir;
    }

    void Check(bool ok, const char* what)
    {
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
        if (!ok)
        {
            ++failures;
        }
    }

    bool Near(double a, double b, double eps)
    {
        return std::fabs(a - b) <= eps;
    }

    int Count(const rf::Image& img, rf::Color c)
    {
        int n = 0;
        for (auto p : img.pixels)
        {
            n += rf::Opaque(p) == rf::Opaque(c);
        }
        return n;
    }

    rf::FrameSettings Window(rf::Style style)
    {
        rf::FrameSettings f;
        f.style = style;
        f.colors = rf::SchemeColors(0, style);
        f.title = L"Мои фото";
        return f;
    }

    void Popups(const std::string& out)
    {
        const rf::FrameSettings f = Window(rf::Style::Win98);
        rf::EffectSettings e;
        e.popupText = L"Файл не найден.\nПовторить попытку?";
        const rf::Color colors[] = {rf::Rgb(0xFF, 0, 0), rf::Rgb(0xFF, 0xFF, 0), rf::Rgb(0, 0, 0xFF), rf::Rgb(0, 0, 0xFF)};
        bool icons = true;
        for (int icon = 0; icon < 4; ++icon)
        {
            e.popupIcon = icon;
            e.popupButtons = icon % 3;
            rf::Rect button;
            const rf::Image p = rf::RenderPopup(e, f, true, &button);
            icons = icons && p.width >= 160 && button.w == 75 && button.Right() <= p.width && button.Bottom() <= p.height && Count(p, colors[icon]) > 20;
            rftest::WritePpm(out + "/popup" + std::to_string(icon) + ".ppm", p);
        }
        Check(icons, "error windows: four icons, buttons inside");
        e.popupText = L"очень длинная строка текста ошибки, которая делает окно шире обычного";
        rf::Rect button;
        const rf::Image wide = rf::RenderPopup(e, f, true, &button);
        e.popupText = L"коротко";
        const rf::Image narrow = rf::RenderPopup(e, f, false, nullptr);
        Check(wide.width > narrow.width && narrow.width >= 160, "error window grows with the text");
    }

    void Layer(const std::string& out)
    {
        const rf::FrameSettings f = Window(rf::Style::Win98);
        const int k = 2;
        const rf::Image framed = rf::ComposeFramed(rf::Image(300, 200, rf::Rgb(0xFF, 0xFF, 0xFF)), f, rf::FrameVars{L"a.jpg", 600, 400, L""}, k);
        const rf::Stage stage{rf::Rect{0, 0, framed.width, framed.height}, k, rf::Rect{}, rf::Rect{}, rf::Rect{}, 0.0};
        rf::EffectSettings e;
        e.cursor = 1;
        rf::Image img = framed;
        rf::EffectsLayer(e, f, stage).Apply(img, 0.0, 0, false);
        const rf::Rect close = rf::CloseButtonRect(f, framed.width / k);
        const int px = close.x * k + close.w * k * 3 / 5;
        const int py = close.y * k + close.h * k * 3 / 5;
        Check(rf::Opaque(img.Get(px, py)) == rf::Rgb(0, 0, 0) && rf::Opaque(img.Get(px + k, py + 2 * k)) == rf::Rgb(0xFF, 0xFF, 0xFF),
              "cursor: arrow tip on the close button");
        e.cursorPlace = 1;
        img = framed;
        rf::EffectsLayer(e, f, stage).Apply(img, 0.0, 0, false);
        Check(rf::Opaque(img.Get(framed.width / 2, framed.height / 2)) == rf::Rgb(0, 0, 0), "cursor: in the middle of the window");
        e.cursor = 2;
        img = framed;
        rf::EffectsLayer(e, f, stage).Apply(img, 0.0, 0, false);
        Check(rf::Opaque(img.Get(framed.width / 2, framed.height / 2 - 10 * k)) == rf::Rgb(0, 0, 0), "cursor: hourglass centred");

        e = rf::EffectSettings();
        e.popups = 3;
        e.cursor = 1;
        e.cursorPlace = 3;
        const rf::EffectsLayer cascade(e, f, stage);
        Check(cascade.VisiblePopups(0.2, true) == 0 && cascade.VisiblePopups(0.6, true) == 1 && cascade.VisiblePopups(5.0, true) == rf::CascadeCount &&
                  cascade.VisiblePopups(0.0, false) == rf::CascadeCount,
              "cascade: windows appear one by one in video, all at once in a still");
        img = framed;
        cascade.Apply(img, 5.0, 0, true);
        int changed = 0;
        for (std::size_t i = 0; i < img.pixels.size(); ++i)
        {
            changed += img.pixels[i] != framed.pixels[i];
        }
        Check(changed > img.width * img.height / 4, "cascade drawn over the window");
        rftest::WritePpm(out + "/fx_cascade.ppm", img);
        const int pw = rf::RenderPopup(e, f, true, nullptr).width * k;
        const rf::EffectsLayer narrow(e, f, rf::Stage{rf::Rect{0, 0, pw + 40, 900}, k, rf::Rect{}, rf::Rect{}, rf::Rect{}, 0.0});
        const std::vector<rf::Rect> rects = narrow.PopupRects(pw + 40, 900);
        bool inside = rects.size() == std::size_t(rf::CascadeCount);
        for (const auto& r : rects)
        {
            inside = inside && r.x >= 0 && r.Right() <= pw + 40 && r.y >= 0 && r.Bottom() <= 900;
        }
        const std::vector<rf::Rect> roomy = cascade.PopupRects(4000, 4000);
        Check(inside && roomy.size() == std::size_t(rf::CascadeCount) && roomy[1].x - roomy[0].x == 22 * k, "cascade fits a narrow frame");
        e.popups = 1;
        img = framed;
        rf::EffectsLayer(e, f, stage).Apply(img, 0.0, 0, false);
        rftest::WritePpm(out + "/fx_popup_cursor.ppm", img);

        rf::Image gray(64, 64, rf::Rgb(128, 128, 128));
        rf::ApplyScreen(gray, 1, 0, 2);
        Check(rf::Red(gray.Get(32, 30)) > rf::Red(gray.Get(32, 31)) && rf::Luminance(gray.Get(32, 32)) > rf::Luminance(gray.Get(1, 1)),
              "crt: scanlines and darker corners");
        rf::Image a(120, 90, rf::Rgb(90, 140, 200));
        rf::Image b = a;
        rf::Image c = a;
        rf::ApplyScreen(a, 2, 7, 1);
        rf::ApplyScreen(b, 2, 7, 1);
        rf::ApplyScreen(c, 2, 8, 1);
        Check(a.pixels == b.pixels && a.pixels != c.pixels && a.pixels != rf::Image(120, 90, rf::Rgb(90, 140, 200)).pixels, "vhs: noise changes from frame to frame");
        rf::Image vhs = framed;
        rf::ApplyScreen(vhs, 2, 3, k);
        rftest::WritePpm(out + "/fx_vhs.ppm", vhs);
        rf::Image crt = framed;
        rf::ApplyScreen(crt, 1, 0, k);
        rftest::WritePpm(out + "/fx_crt.ppm", crt);

        rf::EffectSettings tv;
        tv.screen = 1;
        rf::Image only = framed;
        const rf::Rect inner{40, 50, 120, 80};
        rf::EffectsLayer(tv, f, rf::Stage{rf::Rect{0, 0, framed.width, framed.height}, k, inner, rf::Rect{}, rf::Rect{}, 0.0}).Apply(only, 0.0, 0, false);
        bool outsideSame = true;
        bool insideChanged = false;
        for (int y = 0; y < framed.height; ++y)
        {
            for (int x = 0; x < framed.width; ++x)
            {
                const bool in = x >= inner.x && y >= inner.y && x < inner.x + inner.w && y < inner.y + inner.h;
                const bool same = only.Get(x, y) == framed.Get(x, y);
                outsideSame = outsideSame && (in || same);
                insideChanged = insideChanged || (in && !same);
            }
        }
        Check(outsideSame && insideChanged, "crt only inside the picture, title and frame stay clean");
        rf::Image smooth(96, 64);
        for (int y = 0; y < 64; ++y)
        {
            for (int x = 0; x < 96; ++x)
            {
                smooth.Set(x, y, rf::Rgb(x * 255 / 95, y * 255 / 63, 128));
            }
        }
        rf::Image p256 = smooth;
        rf::ApplyPalette(p256, 1, 1);
        bool cube = true;
        for (auto c : p256.pixels)
        {
            cube = cube && rf::Red(c) % 51 == 0 && rf::Green(c) % 51 == 0 && rf::Blue(c) % 51 == 0;
        }
        rf::Image p16 = smooth;
        rf::ApplyPalette(p16, 2, 2);
        std::vector<rf::Color> seen;
        for (auto c : p16.pixels)
        {
            if (std::find(seen.begin(), seen.end(), c) == seen.end())
            {
                seen.push_back(c);
            }
        }
        Check(cube && p256.pixels != smooth.pixels && seen.size() <= 16 && seen.size() >= 4, "palette: 256 colors cube and 16 VGA colors");
        rftest::WritePpm(out + "/fx_256.ppm", p256);
        rftest::WritePpm(out + "/fx_16.ppm", p16);
        rf::FrameSettings pf = Window(rf::Style::Win98);
        pf.player = true;
        const rf::VideoPlan pp = rf::PlanVideo(pf, 320, 240, rf::VideoTarget{});
        const rf::Image clipFrame(pp.videoW, pp.videoH, rf::Rgb(120, 120, 120));
        rf::PlayerComposer clean(pf, rf::FrameVars{L"a.mp4", 320, 240, L""}, pp, rf::Image());
        rf::PlayerComposer tube(pf, rf::FrameVars{L"a.mp4", 320, 240, L""}, pp, rf::Image());
        tube.SetEffects(tv);
        const rf::Image plainFrame = clean.Compose(clipFrame, 0.5, L"00:01");
        const rf::Image tubeFrame = tube.Compose(clipFrame, 0.5, L"00:01");
        const rf::Rect vc = rf::MeasureFrame(pf, pp.clientW, pp.clientH).client;
        const int midX = (vc.x + vc.w / 2) * pp.scale;
        const int midY = (vc.y + vc.h / 2) * pp.scale;
        bool titleSame = true;
        for (int x = 0; x < plainFrame.width; ++x)
        {
            titleSame = titleSame && plainFrame.Get(x, 10 * pp.scale) == tubeFrame.Get(x, 10 * pp.scale) &&
                        plainFrame.Get(x, plainFrame.height - 3) == tubeFrame.Get(x, plainFrame.height - 3);
        }
        const bool videoChanged = plainFrame.Get(midX, midY) != tubeFrame.Get(midX, midY) || plainFrame.Get(midX, midY + 1) != tubeFrame.Get(midX, midY + 1);
        Check(titleSame && videoChanged, "video export: crt on the video, title bar and controls clean");

        rf::EffectSettings intro;
        intro.intro = true;
        const rf::Image bg(400, 400, rf::Rgb(0x20, 0x40, 0x80));
        const rf::Stage centre{rf::Rect{50, 60, 300, 260}, 1, rf::Rect{}, rf::Rect{}, rf::Rect{}, 0.0};
        rf::Image first(400, 400);
        rf::EffectsLayer(intro, Window(rf::Style::Win95), centre).DrawIntro(first, bg, 0.0);
        rf::Image late(400, 400);
        rf::EffectsLayer(intro, Window(rf::Style::Win95), centre).DrawIntro(late, bg, rf::IntroSeconds);
        Check(Count(first, rf::Rgb(0x20, 0x40, 0x80)) > 400 * 400 * 9 / 10 && rf::Opaque(late.Get(50, 200)) != rf::Rgb(0x20, 0x40, 0x80) &&
                  rf::Opaque(late.Get(200, 200)) == rf::Rgb(0x20, 0x40, 0x80),
              "intro: outline grows to the window edges");
        rf::Image caption(400, 400);
        rf::EffectsLayer(intro, Window(rf::Style::Win2000), centre).DrawIntro(caption, bg, rf::IntroSeconds * 0.5);
        Check(Count(caption, rf::Rgb(0x20, 0x40, 0x80)) < 400 * 400 - 200, "intro: caption bar in Windows 2000 style");
    }

    int Differ(const rf::Image& a, const rf::Image& b, const rf::Rect& r)
    {
        int n = 0;
        for (int y = std::max(0, r.y); y < std::min({a.height, b.height, r.Bottom()}); ++y)
        {
            for (int x = std::max(0, r.x); x < std::min({a.width, b.width, r.Right()}); ++x)
            {
                n += rf::Opaque(a.Get(x, y)) != rf::Opaque(b.Get(x, y));
            }
        }
        return n;
    }

    void Desktop(const std::string& out)
    {
        const rf::Color teal = rf::Rgb(0x00, 0x80, 0x80);
        for (rf::Style style : {rf::Style::Win98, rf::Style::WinXP})
        {
            rf::FrameSettings f = Window(style);
            f.icon = rf::BuiltinIcon(rf::IconId::Folder);
            rf::DesktopOptions d;
            d.icons = true;
            d.clock = L"12:34";
            const int k = 2;
            const rf::Image wall(800, 600, teal);
            const rf::Image bg = rf::DesktopBackground(wall, f, d, 800, 600, k, L"Мои фото", f.icon);
            const int bar = rf::TaskbarHeight(style) * k;
            int barPixels = 0;
            for (int x = 0; x < 800; ++x)
            {
                barPixels += rf::Opaque(bg.Get(x, 600 - 3)) != teal;
            }
            const rf::Rect task = rf::TaskTarget(f, 800, 600, k);
            const rf::Image gone = rf::DesktopBackground(wall, f, d, 800, 600, k, L"Мои фото", f.icon, rf::TaskButton::None);
            const rf::Image idle = rf::DesktopBackground(wall, f, d, 800, 600, k, L"Мои фото", f.icon, rf::TaskButton::Inactive);
            Check(barPixels == 800 && task.y >= 600 - bar && task.Bottom() <= 600 && task.w > 100 && Differ(bg, gone, task) > task.w * task.h / 4 &&
                      Differ(bg, idle, task) > 50 && Differ(bg, gone, rf::Rect{0, 0, 800, 600 - bar}) == 0,
                  style == rf::Style::WinXP ? "desktop xp: taskbar, window button active, idle or gone" : "desktop 98: taskbar, window button active, idle or gone");
            const rf::Rect iconArea{0, 0, 90 * k, 140 * k};
            d.icons = false;
            const rf::Image bare = rf::DesktopBackground(wall, f, d, 800, 600, k, L"Мои фото", f.icon);
            d.taskbar = false;
            d.icons = true;
            const rf::Image iconsOnly = rf::DesktopBackground(wall, f, d, 800, 600, k, L"", rf::Image());
            Check(Differ(bg, wall, iconArea) > 1000 && Differ(bare, wall, iconArea) == 0 && Differ(iconsOnly, wall, rf::Rect{100 * k, 600 - bar, 800 - 100 * k, bar}) == 0 &&
                      Differ(iconsOnly, wall, iconArea) > 1000,
                  "desktop: icons and taskbar switch separately");
            rftest::WritePpm(out + (style == rf::Style::WinXP ? "/fx_desktop_xp.ppm" : "/fx_desktop_98.ppm"), bg);
        }
        Check(rf::ClockText(L"9:41") == L"9:41" && rf::ClockText(L"").find(L':') != std::wstring::npos, "desktop clock: own text or the time now");

        rf::FrameSettings pf = Window(rf::Style::Win98);
        pf.player = true;
        rf::VideoTarget sq;
        sq.square = true;
        sq.canvasW = 1080;
        sq.canvasH = 1080;
        sq.reserve = rf::TaskbarHeight(rf::Style::Win98);
        const rf::VideoPlan vp = rf::PlanVideo(pf, 1920, 1080, sq);
        sq.reserve = 0;
        const rf::VideoPlan free = rf::PlanVideo(pf, 1920, 1080, sq);
        Check(vp.reserve == 28 * vp.scale && vp.windowY + vp.windowH <= 1080 - vp.reserve && free.reserve == 0 && vp.windowY <= free.windowY,
              "video plan leaves room for the taskbar");
        const rf::FrameSettings mfr = Window(rf::Style::WinXP);
        const rf::MusicPlan mp = rf::PlanMusic(mfr, 1080, 1080, true, false, false, rf::TaskbarHeight(rf::Style::WinXP));
        const rf::MusicPlan mw = rf::PlanMusic(mfr, 1080, 1080, false, false, false, 30);
        Check(mp.reserve == 30 * mp.scale && mp.windowY + mp.windowH <= 1080 - mp.reserve && mw.reserve == 0, "music plan leaves room for the taskbar");
    }

    void Motion(const std::string& out)
    {
        rf::EffectSettings shake;
        shake.motion = 1;
        const rf::Rect window{100, 100, 200, 160};
        const rf::Rect area{0, 0, 600, 500};
        bool small = true;
        bool varied = false;
        const rf::Spot first = rf::WindowMotion(shake, 0.0, 0, 2, window, area);
        for (int seed = 0; seed < 60; ++seed)
        {
            const rf::Spot s = rf::WindowMotion(shake, 0.0, seed, 2, window, area);
            small = small && std::abs(s.x) <= 4 && std::abs(s.y) <= 4 && s.x % 2 == 0 && s.y % 2 == 0;
            varied = varied || s.x != first.x || s.y != first.y;
        }
        Check(small && varied, "window shakes by a couple of pixels");
        rf::EffectSettings drift;
        drift.motion = 2;
        bool inside = true;
        int reach = 0;
        for (int i = 0; i < 300; ++i)
        {
            const rf::Spot s = rf::WindowMotion(drift, i * 0.05, i, 1, window, area);
            const rf::Rect r = window.Offset(s.x, s.y);
            inside = inside && r.x >= 0 && r.y >= 0 && r.Right() <= 600 && r.Bottom() <= 500;
            reach = std::max(reach, std::abs(s.x));
        }
        const rf::Spot still = rf::WindowMotion(rf::EffectSettings(), 3.0, 5, 1, window, area);
        Check(inside && reach > 50 && still.x == 0 && still.y == 0, "window floats around but stays on the screen");

        const rf::FrameSettings f = Window(rf::Style::Win98);
        rf::EffectSettings live;
        live.cursor = 1;
        live.cursorPlace = rf::LiveCursor;
        live.popups = 3;
        live.outro = 2;
        const rf::Rect win{100, 100, 400, 300};
        const rf::Stage st{win, 1, rf::Rect{110, 130, 380, 200}, rf::Rect{120, 350, 30, 20}, rf::Rect{160, 340, 300, 10}, 10.0};
        const rf::EffectsLayer layer(live, f, st);
        int closed = -1;
        const rf::Spot begin = layer.CursorAt(0.0, 0.0, 800, 600, &closed);
        const rf::Spot play = layer.CursorAt(0.8, 0.08, 800, 600, nullptr);
        Check(begin.y > win.Bottom() && closed == 0 && play.x == 135 && play.y == 360, "live cursor: comes in and clicks Play");
        const std::vector<rf::Rect> rects = layer.PopupRects(800, 600);
        const double c0 = 0.8 + 0.5 + (rf::CascadeCount - 1) * rf::PopupStep + 0.6;
        const rf::Spot onOk = layer.CursorAt(c0 + 0.5, 0.4, 800, 600, &closed);
        const bool hitsTop = rects.size() == std::size_t(rf::CascadeCount) && rects.back().Contains(onOk.x, onOk.y);
        int later = 0;
        layer.CursorAt(c0 + 0.6 * 2 + 0.01, 0.45, 800, 600, &later);
        int all = 0;
        layer.CursorAt(c0 + 0.6 * rf::CascadeCount + 0.1, 0.6, 800, 600, &all);
        Check(hitsTop && closed == 0 && later == 2 && all == rf::CascadeCount, "live cursor: clicks OK on the error windows one by one");
        const double tc = c0 + 0.6 * rf::CascadeCount;
        const rf::Spot ride = layer.CursorAt(tc + 1.5, 0.5, 800, 600, nullptr);
        Check(ride.x == 160 + 150 && ride.y == 345, "live cursor: drags the seek bar thumb");
        const rf::Rect mini = rf::ScaleRect(rf::MinimizeButtonRect(f, 400), 1, 100, 100);
        const rf::Spot end = layer.CursorAt(9.9, 0.99, 800, 600, nullptr);
        Check(mini.Contains(end.x, end.y), "live cursor: goes to the minimize button at the end");
        rf::Image busy(800, 600, rf::Rgb(0x30, 0x60, 0x90));
        layer.Apply(busy, 2.9, 0, true, 0.29);
        rf::Image calm(800, 600, rf::Rgb(0x30, 0x60, 0x90));
        layer.Apply(calm, 7.0, 0, true, 0.7);
        Check(Count(calm, rf::Rgb(0x30, 0x60, 0x90)) > 800 * 600 - 400 && Count(busy, rf::Rgb(0x30, 0x60, 0x90)) < 800 * 600 - 20000,
              "live cursor: error windows go away after the clicks");
        rftest::WritePpm(out + "/fx_live_popups.ppm", busy);

        const rf::Color sky = rf::Rgb(0x20, 0x40, 0x80);
        const rf::Image ending(400, 400, sky);
        rf::EffectSettings bye;
        bye.outro = 1;
        bye.cursor = 1;
        const rf::Stage centre{rf::Rect{50, 60, 300, 260}, 1, rf::Rect{}, rf::Rect{}, rf::Rect{}, 0.0};
        const rf::EffectsLayer closing(bye, Window(rf::Style::Win95), centre);
        rf::Image a(400, 400);
        closing.DrawOutro(a, ending, 0.0, rf::Rect{});
        rf::Image z(400, 400);
        closing.DrawOutro(z, ending, rf::OutroAnim + 0.1, rf::Rect{});
        Check(rf::Opaque(a.Get(50, 200)) != sky && rf::Opaque(a.Get(200, 200)) == sky && Count(z, sky) > 400 * 400 - 300, "outro: window closes, desktop stays");
        bye.outro = 2;
        const rf::EffectsLayer folding(bye, Window(rf::Style::Win98), centre);
        rf::Image m(400, 400);
        const rf::Rect target{10, 370, 100, 22};
        folding.DrawOutro(m, ending, rf::OutroAnim * 0.98, target);
        rf::Image m0(400, 400);
        folding.DrawOutro(m0, ending, 0.0, target);
        Check(rf::Opaque(m.Get(60, 380)) != sky && rf::Opaque(m.Get(200, 200)) == sky && rf::Opaque(m0.Get(200, 70)) != sky, "outro: window folds into the taskbar button");
        rftest::WritePpm(out + "/fx_outro_min.ppm", m);
    }

    void Settings()
    {
        rf::AppSettings s = rf::DefaultSettings();
        s.effects.intro = true;
        s.effects.cursor = 3;
        s.effects.cursorPlace = 2;
        s.effects.popups = 2;
        s.effects.popupIcon = 3;
        s.effects.popupButtons = 2;
        s.effects.popupTitle = L"Внимание | важно";
        s.effects.popupText = L"Строка один\nСтрока два";
        s.effects.screen = 2;
        s.effects.palette = 2;
        s.effects.motion = 2;
        s.effects.outro = 1;
        s.effects.desktop = true;
        s.effects.desktopIcons = true;
        s.effects.clock = L"23:59";
        s.effects.cursorPlace = rf::LiveCursor;
        rf::AppSettings back = rf::DefaultSettings();
        rf::ParseSettings(rf::SerializeSettings(s), back);
        const rf::EffectSettings& b = back.effects;
        Check(b.intro && b.cursor == 3 && b.cursorPlace == rf::LiveCursor && b.motion == 2 && b.outro == 1 && b.desktop && b.desktopIcons && b.clock == L"23:59" &&
                  b.popups == 2 && b.popupIcon == 3 && b.popupButtons == 2 && b.popupTitle == s.effects.popupTitle &&
                  b.popupText == s.effects.popupText && b.screen == 2 && b.palette == 2,
              "effects settings round trip");
        const rf::EffectSettings d = rf::DefaultSettings().effects;
        Check(!d.intro && d.cursor == 0 && d.popups == 0 && d.screen == 0 && d.motion == 0 && d.outro == 0 && !d.desktop && !d.desktopIcons && !rf::HasOverlays(d),
              "effects are off by default");
        rf::EffectsView view(s.effects);
        rf::Image dialog;
        view.root.Paint(dialog);
        rftest::WritePpm(Out() + "/ui_effects_dialog.ppm", dialog);
        const rf::EffectSettings r = view.Result();
        Check(r.intro && r.cursor == 3 && r.popups == 2 && r.popupText == s.effects.popupText && r.popupTitle == s.effects.popupTitle && r.screen == 2 &&
                  r.cursorPlace == rf::LiveCursor && r.motion == 2 && r.outro == 1 && r.desktop && r.desktopIcons && r.clock == L"23:59",
              "effects dialog keeps the values");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        return 1;
    }
    rftest::TestTextEngine engine;
    rf::SetTextEngine(&engine);
    const std::string out = argv[1];
    outDir = out;
    Popups(out);
    Layer(out);
    Desktop(out);
    Motion(out);
    Settings();

    rftest::TestHost host;
    {
        rf::MainView main(host);
        main.AddImage(rf::Image(320, 240, rf::Rgb(0x30, 0xA0, 0x30)), L"", L"луг.jpg", true);
        host.saveAnswer = L"plain.png";
        const auto click = [&](int x, int y) {
            main.root.MouseDown(x, y, false, false, 1);
            main.root.MouseUp(x, y);
        };
        click(240, 59);
        const rf::Image plain = host.writtenImage;
        host.onModal = [](rf::View& v) {
            auto* fx = dynamic_cast<rf::EffectsView*>(&v);
            return fx != nullptr ? 1 : 0;
        };
        main.MutableSettings().effects.popups = 1;
        main.MutableSettings().effects.cursor = 1;
        main.MutableSettings().effects.screen = 1;
        click(330, 59);
        Check(host.modalCalls == 1, "effects button opens the dialog");
        host.saveAnswer = L"fx.png";
        click(240, 59);
        const rf::Image fx = host.writtenImage;
        Check(plain.width == fx.width && plain.height == fx.height && plain.pixels != fx.pixels, "photo saved with effects");
        rftest::WritePpm(out + "/fx_photo.ppm", fx);
        rf::Image shot;
        main.root.Paint(shot);
        rftest::WritePpm(out + "/ui_fx.ppm", shot);
        host.onModal = nullptr;
    }

    {
        rf::AppSettings s = rf::DefaultSettings();
        Check(s.presets.size() == 3 && s.presets[0].style == rf::Style::Win95 && s.presets[2].look.kind == rf::WindowKind::Dialog, "three starter presets");
        rf::LookPreset mine;
        mine.name = L"Мой | набор";
        mine.style = rf::Style::Win2000;
        mine.scheme = -1;
        mine.colors.active1 = rf::Rgb(0x12, 0x34, 0x56);
        mine.look.iconIndex = rf::CustomIconIndex;
        mine.look.customTitle = L"Привет";
        mine.look.titleFromName = false;
        mine.icon = rf::Image(16, 16, rf::Argb(0x80, 0xFF, 0x00, 0x00));
        mine.scale = 3;
        s.presets.push_back(mine);
        rf::AppSettings back = rf::DefaultSettings();
        rf::ParseSettings(rf::SerializeSettings(s), back);
        const rf::LookPreset& b = back.presets.back();
        Check(back.presets.size() == 4 && b.name == mine.name && b.style == rf::Style::Win2000 && b.colors.active1 == mine.colors.active1 &&
                  b.look.iconIndex == rf::CustomIconIndex && b.look.customTitle == L"Привет" && b.icon.pixels == mine.icon.pixels && b.scale == 3,
              "presets round trip with custom icon");
        rf::AppSettings none = rf::DefaultSettings();
        rf::ParseSettings("presets=0\n", none);
        rf::AppSettings old = rf::DefaultSettings();
        rf::ParseSettings("style=1\n", old);
        Check(none.presets.empty() && old.presets.size() == 3, "deleted presets stay deleted, old settings get starters");

        rftest::TestHost ph;
        rf::MainView main(ph);
        const auto click = [&](rf::View& v, int x, int y) {
            v.root.MouseDown(x, y, false, false, 1);
            v.root.MouseUp(x, y);
        };
        main.MutableSettings().frame.style = rf::Style::Win98;
        int step = 0;
        ph.onModal = [&](rf::View& v) {
            if (dynamic_cast<rf::PresetsView*>(&v) == nullptr)
            {
                return 0;
            }
            rf::Image dialog;
            v.root.Paint(dialog);
            rftest::WritePpm(out + "/ui_presets.ppm", dialog);
            click(v, 30, 46 + 2 + 9);
            click(v, 350, 57);
            step = 1;
            click(v, 150, 252);
            for (wchar_t ch : std::wstring(L"Для мемов"))
            {
                v.root.Char(ch);
            }
            click(v, 358, 252);
            return 0;
        };
        click(main, 420, 59);
        Check(step == 1 && main.Settings().frame.style == rf::Style::Win95 && main.Settings().looks[rf::PhotoMode].status, "apply a preset from the dialog");
        Check(main.Settings().presets.size() == 4 && main.Settings().presets.back().name == L"Для мемов" &&
                  main.Settings().presets.back().style == rf::Style::Win95,
              "save the current look as a new preset");
        ph.onModal = [&](rf::View& v) {
            if (dynamic_cast<rf::PresetsView*>(&v) == nullptr)
            {
                return 0;
            }
            click(v, 30, 46 + 2 + 18 * 3 + 9);
            click(v, 350, 85);
            return 0;
        };
        click(main, 420, 59);
        Check(main.Settings().presets.size() == 3, "delete a preset");
        ph.onModal = nullptr;
    }

    if (argc >= 3)
    {
        host.ffmpeg = rf::FromUtf8(argv[2]);
        const std::wstring dir = rf::FromUtf8(out);
        const std::wstring song = dir + L"/fx_song.wav";
        const std::wstring clip = dir + L"/fx_clip.mp4";
        const auto run = [&](const std::vector<std::wstring>& args) {
            auto p = host.StartProcess(host.ffmpeg, args, false, false);
            int code = -1;
            return p && p->Finish(code, []() { return true; }) && code == 0;
        };
        Check(run({L"-v", L"error", L"-y", L"-f", L"lavfi", L"-i", L"sine=frequency=440:sample_rate=44100:duration=2", L"-af", L"volume=0.8", song}) &&
                  run({L"-v", L"error", L"-y", L"-f", L"lavfi", L"-i", L"testsrc=size=320x240:rate=25", L"-f", L"lavfi", L"-i", L"sine=frequency=330:sample_rate=44100",
                       L"-t", L"1", L"-c:v", L"libx264", L"-pix_fmt", L"yuv420p", L"-c:a", L"aac", L"-shortest", clip}),
              "make test media");
        rf::FrameSettings f = Window(rf::Style::Win98);
        f.kind = rf::WindowKind::Bare;
        rf::MusicJob job;
        job.source = song;
        job.output = dir + L"/fx_song.mp4";
        job.duration = 2.0;
        job.frame = f;
        job.plan = rf::PlanMusic(f, 720, 720, true);
        job.background = rf::CanvasBackground(rf::Image(), rf::Rgb(0x2E, 0x5B, 0x88), 720, 720);
        job.effects.intro = true;
        job.effects.screen = 2;
        job.effects.cursor = 1;
        std::wstring err;
        rf::MediaInfo m;
        Check(rf::ExportMusic(host, job, [](int, int) { return true; }, err) == rf::ExportResult::Done && rf::ProbeMedia(host, job.output, m, err) &&
                  Near(m.duration, 2.0 + rf::IntroSeconds, 0.12) && m.hasAudio,
              "music with intro: longer by the animation");
        rf::LevelReader levels(host, job.output);
        levels.Wait();
        const std::vector<float>& lv = levels.Levels();
        Check(lv.size() > 40 && lv[5] < 0.05f && lv[30] > 0.3f, "music with intro: sound starts after the animation");
        rf::Image frame0;
        {
            rf::FrameStream first(host, job.output, 0.0, 0.0, 0.0, 720, 720);
            first.Next(frame0);
        }
        Check(!frame0.Empty() && rf::Luminance(frame0.Get(360, 360)) < 160, "music with intro: first frame is the background");

        rf::ExportJob video;
        video.source = clip;
        video.output = dir + L"/fx_clip_out.mp4";
        video.duration = 1.0;
        video.fps = 25.0;
        rf::FrameSettings player = Window(rf::Style::Win95);
        player.player = true;
        video.frame = player;
        video.plan = rf::PlanVideo(player, 320, 240, rf::VideoTarget{});
        video.effects.intro = true;
        video.effects.popups = 3;
        video.effects.screen = 1;
        rf::MediaInfo v;
        Check(rf::ExportVideo(host, video, [](int, int) { return true; }, err) == rf::ExportResult::Done && rf::ProbeMedia(host, video.output, v, err) &&
                  Near(v.duration, 1.0 + rf::IntroSeconds, 0.12) && v.hasAudio,
              "video with intro and effects");

        rf::ExportJob bye = video;
        bye.output = dir + L"/fx_clip_bye.mp4";
        rf::VideoTarget sq;
        sq.square = true;
        sq.canvasW = 480;
        sq.canvasH = 480;
        sq.reserve = rf::TaskbarHeight(player.style);
        bye.plan = rf::PlanVideo(player, 320, 240, sq);
        rf::DesktopOptions d;
        d.clock = L"10:00";
        const rf::Image wall = rf::CanvasBackground(rf::Image(), rf::Rgb(0x00, 0x80, 0x80), 480, 480);
        bye.background = rf::DesktopBackground(wall, player, d, 480, 480, bye.plan.scale, L"ролик", rf::Image());
        bye.ending = rf::DesktopBackground(wall, player, d, 480, 480, bye.plan.scale, L"ролик", rf::Image(), rf::TaskButton::Inactive);
        bye.taskTarget = rf::TaskTarget(player, 480, 480, bye.plan.scale);
        bye.effects = rf::EffectSettings();
        bye.effects.outro = 2;
        bye.effects.motion = 2;
        bye.effects.cursor = 1;
        bye.effects.cursorPlace = rf::LiveCursor;
        rf::MediaInfo bv;
        Check(rf::ExportVideo(host, bye, [](int, int) { return true; }, err) == rf::ExportResult::Done && rf::ProbeMedia(host, bye.output, bv, err) &&
                  Near(bv.duration, 1.0 + rf::OutroSeconds, 0.12) && bv.hasAudio,
              "video with outro: longer by the animation, sound kept");
        rf::Image last;
        {
            rf::FrameStream tail(host, bye.output, bv.duration - 0.15, 0.0, 0.0, 480, 480);
            tail.Next(last);
        }
        const int cy = (480 - sq.reserve * bye.plan.scale) / 2;
        Check(!last.Empty() && std::abs(int(rf::Green(last.Get(240, cy))) - 0x80) < 24 && int(rf::Red(last.Get(240, cy))) < 24,
              "video with outro: the window is gone at the end");
        rftest::WritePpm(out + "/fx_outro_last.ppm", last);
        rf::ExportJob gifBye = bye;
        gifBye.gif = true;
        gifBye.output = dir + L"/fx_clip_bye.gif";
        rf::MediaInfo gv;
        Check(rf::ExportVideo(host, gifBye, [](int, int) { return true; }, err) == rf::ExportResult::Done && rf::ProbeMedia(host, gifBye.output, gv, err) &&
                  gv.duration < 1.0 + rf::OutroSeconds / 2,
              "gif keeps looping without the outro");

        rf::MusicJob closing = job;
        closing.output = dir + L"/fx_song_bye.mp4";
        closing.effects = rf::EffectSettings();
        closing.effects.outro = 1;
        closing.effects.cursor = 1;
        closing.effects.cursorPlace = rf::LiveCursor;
        closing.effects.motion = 1;
        rf::MediaInfo cm;
        Check(rf::ExportMusic(host, closing, [](int, int) { return true; }, err) == rf::ExportResult::Done && rf::ProbeMedia(host, closing.output, cm, err) &&
                  Near(cm.duration, 2.0 + rf::OutroSeconds, 0.12) && cm.hasAudio,
              "music with outro: longer by the animation");
    }
    std::printf(failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 2;
}
