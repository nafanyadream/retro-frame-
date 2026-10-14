#include "test_support.h"

#include "../src/core/mainview.h"
#include "../src/core/scenes.h"
#include "test_host.h"

#include <chrono>
#include <csignal>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace
{
    int failures = 0;

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

    rf::FrameSettings Window(rf::Style style)
    {
        rf::FrameSettings f;
        f.style = style;
        f.colors = rf::SchemeColors(0, style);
        f.title = L"Безымянный - Блокнот";
        f.menu = true;
        f.menuItems = rf::NotepadMenu(true, style);
        return f;
    }

    int Count(const rf::Image& img, const rf::Rect& r, rf::Color c)
    {
        int n = 0;
        for (int y = std::max(0, r.y); y < std::min(img.height, r.Bottom()); ++y)
        {
            for (int x = std::max(0, r.x); x < std::min(img.width, r.Right()); ++x)
            {
                n += rf::Opaque(img.Get(x, y)) == rf::Opaque(c);
            }
        }
        return n;
    }

    double Energy(const std::vector<std::int16_t>& s, double from, double to, int rate)
    {
        double e = 0.0;
        const std::size_t a = std::size_t(std::max(0.0, from * rate));
        const std::size_t b = std::min(s.size(), std::size_t(std::max(0.0, to * rate)));
        for (std::size_t i = a; i < b; ++i)
        {
            e += double(s[i]) * double(s[i]);
        }
        return b > a ? std::sqrt(e / double(b - a)) : 0.0;
    }

    void Notepad(const std::string& out)
    {
        rf::NotepadOptions o;
        o.text = L"Привет\r\nмир!";
        o.typos = false;
        const auto keys = rf::NotepadKeys(o);
        const double length = rf::NotepadDuration(o, keys);
        Check(keys.size() == 11 && rf::NotepadTextAt(keys, length) == L"Привет\nмир!" && rf::NotepadTextAt(keys, 0.0).empty() &&
                  rf::NotepadTextAt(keys, keys[3].t) == L"Прив",
              "notepad: letters appear one by one, CR LF becomes a line break");
        rf::NotepadOptions slow = o;
        slow.speed = 0;
        rf::NotepadOptions fast = o;
        fast.speed = 3;
        Check(rf::NotepadDuration(slow, rf::NotepadKeys(slow)) > rf::NotepadDuration(fast, rf::NotepadKeys(fast)) + 0.5, "notepad: speed changes the length");
        rf::NotepadOptions typos;
        typos.typos = true;
        typos.text = L"Съешь же ещё этих мягких французских булок, да выпей чаю. The quick brown fox jumps over the lazy dog.";
        const auto tk = rf::NotepadKeys(typos);
        int erased = 0;
        bool wrongSeen = false;
        for (std::size_t i = 0; i < tk.size(); ++i)
        {
            erased += tk[i].erase ? 1 : 0;
            if (tk[i].erase)
            {
                const std::wstring before = rf::NotepadTextAt(tk, tk[i].t - 1e-4);
                wrongSeen = wrongSeen || typos.text.compare(0, before.size(), before) != 0;
            }
        }
        Check(erased > 0 && wrongSeen && rf::NotepadTextAt(tk, rf::NotepadDuration(typos, tk)) == typos.text, "notepad: typos get erased, text ends right");
        const double end = keys.back().t;
        bool on = false;
        bool off = false;
        for (int i = 0; i < 40; ++i)
        {
            const bool c = rf::NotepadCaretAt(o, keys, end + 0.6 + i * 0.05);
            on = on || c;
            off = off || !c;
        }
        rf::NotepadOptions plain = o;
        plain.caret = false;
        Check(rf::NotepadCaretAt(o, keys, end + 0.1) && on && off && !rf::NotepadCaretAt(plain, keys, end + 0.1), "notepad: caret blinks when typing stops, switchable");
        Check(rf::NotepadTitle(L"", true) == L"Безымянный - Блокнот" && rf::NotepadTitle(L"письмо", false) == L"письмо - Notepad" &&
                  rf::NotepadMenu(true, rf::Style::Win98).size() == 4 && rf::NotepadMenu(false, rf::Style::WinXP)[2] == L"Format",
              "notepad: title and menu by language and Windows version");

        for (rf::Style style : {rf::Style::Win98, rf::Style::WinXP})
        {
            const rf::FrameSettings f = Window(style);
            int w = 0;
            int h = 0;
            rf::NotepadClient(o, &w, &h);
            rf::Image client(w, h);
            rf::Painter p(client);
            std::wstring many;
            for (int i = 1; i <= 40; ++i)
            {
                many += L"строка " + std::to_wstring(i) + L"\n";
            }
            many += L"последняя";
            rf::DrawNotepad(p, o, w, h, many, true, f);
            const rf::Rect top{4, 4, w - 24, 20};
            const rf::Rect bottom{4, h - 30, w - 24, 26};
            Check(Count(client, bottom, rf::Rgb(0, 0, 0)) > 20 && Count(client, rf::Rect{2, 2, w - 20, h - 4}, rf::Rgb(0xFF, 0xFF, 0xFF)) > w * h / 2 &&
                      Count(client, top, rf::Rgb(0, 0, 0)) > 10,
                  style == rf::Style::WinXP ? "notepad xp: long text scrolls, last line visible" : "notepad 98: long text scrolls, last line visible");
            rftest::WritePpm(out + (style == rf::Style::WinXP ? "/scene_notepad_xp.ppm" : "/scene_notepad_98.ppm"), client);
        }
    }

    void Setup(const std::string& out)
    {
        rf::SetupOptions o;
        o.seconds = 10.0;
        bool monotonic = true;
        for (int pace = 0; pace < 3; ++pace)
        {
            o.pace = pace;
            double prev = -1.0;
            for (int i = 0; i <= 200; ++i)
            {
                const double p = rf::SetupProgress(o, i * 0.06);
                monotonic = monotonic && p >= prev - 1e-9 && p >= 0.0 && p <= 1.0;
                prev = p;
            }
        }
        o.pace = 1;
        const double half = rf::SetupProgress(o, 5.0);
        Check(monotonic && rf::SetupProgress(o, 0.0) == 0.0 && rf::SetupProgress(o, 10.0) == 1.0 && half > 0.1 && half < 0.9, "setup: progress only grows, ends at 100%");
        o.pace = 2;
        o.ending = 2;
        Check(Near(rf::SetupProgress(o, 7.0), 0.99, 1e-9) && Near(rf::SetupProgress(o, 30.0), 0.99, 1e-9), "setup: stuck at 99% with an error at the end");
        o.files = L"a.txt, b.txt\nc.txt\n\n";
        const auto files = rf::SetupFiles(o);
        rf::SetupOptions none;
        none.files.clear();
        Check(files.size() == 3 && files[1] == L"b.txt" && rf::SetupFiles(none).size() == 4, "setup: file names from lines and commas, examples when empty");
        Check(rf::SetupDuration(o) > o.seconds + 2.0 && rf::SetupTitle(o) == L"Копирование..." && rf::SetupEndingPopup(o).popupIcon == 0, "setup: copy window, error at the end");

        const rf::FrameSettings f = Window(rf::Style::Win98);
        rf::SetupOptions copy;
        copy.pace = 0;
        int w = 0;
        int h = 0;
        rf::SetupClient(copy, &w, &h);
        const auto blocks = [&](double t) {
            rf::Image client(w, h);
            rf::Painter p(client);
            rf::DrawSetup(p, copy, w, h, t, f);
            return std::make_pair(Count(client, rf::Rect{14, 96, w - 28, 16}, f.colors.active1), client);
        };
        const auto early = blocks(2.0);
        const auto late = blocks(8.0);
        Check(early.first > 50 && late.first > early.first * 3, "setup: blocks fill the progress bar");
        rftest::WritePpm(out + "/scene_copy.ppm", late.second);
        rf::SetupOptions install;
        install.kind = int(rf::SetupKind::Install);
        install.pace = 0;
        rf::SetupClient(install, &w, &h);
        rf::Image setup(w, h);
        rf::Painter sp(setup);
        rf::DrawSetup(sp, install, w, h, 4.5, f);
        Check(w == 400 && Count(setup, rf::Rect{0, 0, w, 50}, rf::Rgb(0xFF, 0xFF, 0xFF)) > w * 30 && Count(setup, rf::Rect{16, 106, w - 32, 14}, f.colors.active1) > 500,
              "setup: install window with banner and bar");
        rftest::WritePpm(out + "/scene_install.ppm", setup);
        rf::SetupOptions xpCopy = copy;
        xpCopy.kind = int(rf::SetupKind::Delete);
        rf::SetupClient(xpCopy, &w, &h);
        rf::Image xp(w, h);
        rf::Painter xpp(xp);
        rf::DrawSetup(xpp, xpCopy, w, h, 6.0, Window(rf::Style::WinXP));
        rftest::WritePpm(out + "/scene_delete_xp.ppm", xp);
    }

    void Sounds()
    {
        rf::NotepadOptions o;
        o.text = L"абв где";
        const auto keys = rf::NotepadKeys(o);
        const double length = rf::NotepadDuration(o, keys);
        const auto sound = rf::KeyboardSound(keys, length, 44100);
        Check(sound.size() == std::size_t(length * 44100) && Energy(sound, keys[0].t, keys[0].t + 0.02, 44100) > 800.0 && Energy(sound, 0.0, 0.5, 44100) == 0.0 &&
                  Energy(sound, keys.back().t + 0.1, length, 44100) == 0.0,
              "keyboard clicks on each letter, silence between");
        std::vector<std::int16_t> chime(44100 * 3, 0);
        rf::AddChime(chime, 1.0, true, 44100);
        Check(Energy(chime, 1.0, 1.3, 44100) > 2000.0 && Energy(chime, 0.0, 0.99, 44100) == 0.0, "chime sounds with the message");
    }

    void Plans(const std::string& out)
    {
        const rf::FrameSettings f = Window(rf::Style::Win98);
        const rf::ScenePlan sq = rf::PlanScene(f, 420, 240, 1080, 1080, true, 28, 0);
        Check(sq.square && sq.outW == 1080 && sq.scale >= 2 && sq.windowY + sq.windowH <= 1080 - sq.reserve && sq.windowX == (1080 - sq.windowW) / 2,
              "scene plan: window centred above the taskbar");
        const rf::ScenePlan alone = rf::PlanScene(f, 421, 241, 1080, 1080, false, 0, 0);
        const rf::ScenePlan gif = rf::PlanScene(f, 420, 240, 1080, 1080, true, 0, 480);
        Check(alone.outW % 2 == 0 && alone.outH % 2 == 0 && alone.outW == alone.windowW && gif.outW == 480 && gif.windowW <= 480, "scene plan: even window size, gif width");

        rf::NotepadOptions o;
        o.text = L"Пример";
        auto keys = std::make_shared<std::vector<rf::Keystroke>>(rf::NotepadKeys(o));
        const rf::SceneDraw draw = [o, keys, f](rf::Painter& p, int w, int h, double t) {
            rf::DrawNotepad(p, o, w, h, rf::NotepadTextAt(*keys, t), rf::NotepadCaretAt(o, *keys, t), f);
        };
        const rf::Image bg = rf::CanvasBackground(rf::Image(), rf::Rgb(0x10, 0x60, 0x30), 1080, 1080);
        rf::SceneComposer c(f, rf::FrameVars{L"x", 420, 240, L""}, sq, bg, draw);
        rf::EffectSettings e;
        e.popupTitle = L"Ошибка";
        e.popupText = L"Недостаточно памяти.";
        c.SetPopup(e, 2.0);
        const rf::Image before = c.Compose(1.0, 0, true);
        const rf::Image after = c.Compose(3.0, 0, true);
        const int cx = sq.windowX + sq.windowW / 2;
        const int cy = sq.windowY + sq.windowH / 2;
        int diff = 0;
        for (int y = cy - 40; y < cy + 40; ++y)
        {
            for (int x = cx - 80; x < cx + 80; ++x)
            {
                diff += before.Get(x, y) != after.Get(x, y);
            }
        }
        Check(before.width == 1080 && rf::Opaque(before.Get(5, 5)) == rf::Rgb(0x10, 0x60, 0x30) && diff > 3000, "scene composer: window on the background, message after its time");
        rftest::WritePpm(out + "/scene_frame.ppm", after);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        return 1;
    }
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    rftest::TestTextEngine engine;
    rf::SetTextEngine(&engine);
    const std::string out = argv[1];
    const std::wstring dir = rf::FromUtf8(out);
    Notepad(out);
    Setup(out);
    Sounds();
    Plans(out);

    {
        rf::AppSettings s = rf::DefaultSettings();
        s.notepad.text = L"Строка 1\nСтрока | 2";
        s.notepad.file = L"письмо";
        s.notepad.russian = false;
        s.notepad.font = 2;
        s.notepad.speed = 3;
        s.notepad.typos = false;
        s.notepad.hold = 5;
        s.notepad.out.format = 3;
        s.notepad.out.square = true;
        s.setup.kind = 2;
        s.setup.program = L"Игра 2000";
        s.setup.files = L"a.dll\nb.exe";
        s.setup.seconds = 30;
        s.setup.pace = 2;
        s.setup.ending = 2;
        s.setup.out.canvas = 3;
        s.looks[rf::NotepadMode].customTitle = L"Мой блокнот";
        rf::AppSettings back = rf::DefaultSettings();
        rf::ParseSettings(rf::SerializeSettings(s), back);
        Check(back.notepad.text == s.notepad.text && back.notepad.file == L"письмо" && !back.notepad.russian && back.notepad.font == 2 && back.notepad.speed == 3 &&
                  !back.notepad.typos && back.notepad.hold == 5 && back.notepad.out.format == 3 && back.notepad.out.square && back.setup.kind == 2 &&
                  back.setup.program == L"Игра 2000" && back.setup.files == L"a.dll\nb.exe" && back.setup.seconds == 30 && back.setup.pace == 2 &&
                  back.setup.ending == 2 && back.setup.out.canvas == 3 && back.looks[rf::NotepadMode].customTitle == L"Мой блокнот",
              "notepad and setup settings round trip");
        Check(rf::DefaultSettings().looks[rf::NotepadMode].menu && rf::DefaultSettings().looks[rf::SetupMode].kind == rf::WindowKind::Dialog,
              "notepad has a menu, the copy window is a dialog");
    }

    rftest::TestHost host;
    host.tempFolder = dir + L"/scene_tmp/";
    std::filesystem::create_directories(rftest::Narrow(host.tempFolder));
    if (argc >= 3)
    {
        host.ffmpeg = rf::FromUtf8(argv[2]);
    }
    rf::MainView main(host);
    unsigned long long ms = 1000;
    const auto settle = [&](int ticks) {
        for (int i = 0; i < ticks; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            ms += 40;
            main.OnTick(ms);
        }
    };
    const auto click = [&](int x, int y) {
        main.root.MouseDown(x, y, false, false, 1);
        main.root.MouseUp(x, y);
    };
    const auto shot = [&](const std::string& name) {
        rf::Image img;
        main.root.Paint(img);
        rftest::WritePpm(out + "/" + name + ".ppm", img);
        return img;
    };
    main.SetMode(rf::NotepadMode);
    main.Refresh();
    const rf::Image ui = shot("ui_notepad");
    Check(main.ShownTab() == rf::NotepadMode && Count(ui, rf::Rect{16, 112, 300, 296}, rf::Rgb(0xFF, 0xFF, 0xFF)) > 3000, "notepad tab shows the window in the preview");
    click(100, 500);
    main.root.Key(rf::ui::KeyEvent{rf::ui::KeyCode::Letter, L'A', false, true, false});
    for (wchar_t ch : std::wstring(L"Всем привет"))
    {
        main.root.Char(ch);
    }
    main.root.Key(rf::ui::KeyEvent{rf::ui::KeyCode::Enter});
    for (wchar_t ch : std::wstring(L"из 1998"))
    {
        main.root.Char(ch);
    }
    main.root.Flush();
    Check(main.Settings().notepad.text == L"Всем привет\nиз 1998", "typing into the text box: letters and Enter");
    const double length = main.SceneLength();
    Check(length > 2.0 && length < 6.0, "scene length follows the text");
    host.audioBytes = 0;
    click(60, 426);
    settle(30);
    const double mid = main.ScenePosition();
    Check(main.Settings().previewSound && mid > 0.8 && mid < length && host.audioOpen == 1 && host.audioBytes > 10000, "play: typing goes on with keyboard sound");
    shot("ui_notepad_playing");
    settle(200);
    Check(Near(main.ScenePosition(), length, 1e-6) && host.audioOpen == 0, "play stops at the end");

    host.saveAnswer = dir + L"/scene_still.png";
    main.MutableSettings().notepad.out.format = 4;
    main.MutableSettings().notepad.out.square = true;
    main.Refresh();
    click(240, 59);
    Check(host.written == host.saveAnswer && host.writtenImage.width == 1080 && host.writtenImage.height == 1080, "notepad: PNG frame on the background");
    rftest::WritePpm(out + "/scene_still.ppm", host.writtenImage);

    {
        const std::string txt = out + "/письмо.txt";
        std::ofstream f(txt, std::ios::binary);
        const unsigned char cp1251[] = {0xCF, 0xF0, 0xE8, 0xE2, 0xE5, 0xF2, 0x0D, 0x0A, 0xEC, 0xE8, 0xF0};
        f.write(reinterpret_cast<const char*>(cp1251), sizeof(cp1251));
    }
    main.SetMode(rf::PhotoMode);
    main.OnDropFiles({dir + L"/письмо.txt"});
    Check(main.Settings().mode == rf::NotepadMode && main.Settings().notepad.text == L"Привет\nмир" && main.Settings().notepad.file == L"письмо",
          "drop a .txt (Windows-1251): Notepad tab with its text");

    main.SetMode(rf::SetupMode);
    main.MutableSettings().setup.kind = 2;
    main.MutableSettings().setup.ending = 2;
    main.MutableSettings().setup.seconds = 5;
    main.MutableSettings().setup.out.format = 4;
    main.MutableSettings().setup.out.square = false;
    main.Refresh();
    shot("ui_setup");
    click(60, 426);
    settle(400);
    host.saveAnswer = dir + L"/setup_end.png";
    click(240, 59);
    const rf::Image end = host.writtenImage;
    Check(host.written == host.saveAnswer && Count(end, rf::Rect{0, 0, end.width, end.height}, rf::Rgb(0xFF, 0, 0)) > 30 && end.width < 1080,
          "setup: window only, error message at the end");
    rftest::WritePpm(out + "/setup_end.ppm", end);
    shot("ui_setup_end");

    if (!host.ffmpeg.empty())
    {
        main.SetMode(rf::NotepadMode);
        main.MutableSettings().notepad.text = L"Тест";
        main.MutableSettings().notepad.out.format = 1;
        main.MutableSettings().notepad.out.square = false;
        main.MutableSettings().effects.intro = true;
        main.Refresh();
        const double typed = main.SceneLength();
        host.saveAnswer = dir + L"/notepad.mp4";
        click(240, 59);
        rf::MediaInfo m;
        std::wstring err;
        Check(rf::ProbeMedia(host, host.saveAnswer, m, err) && m.hasAudio && Near(m.duration, typed + rf::IntroSeconds, 0.15), "notepad MP4 with keyboard sound");
        main.MutableSettings().effects.intro = false;
        main.MutableSettings().notepad.out.format = 3;
        main.Refresh();
        host.saveAnswer = dir + L"/notepad.gif";
        click(240, 59);
        rf::MediaInfo g;
        Check(rf::ProbeMedia(host, host.saveAnswer, g, err) && !g.hasAudio && g.hasVideo && Near(g.duration, typed, 0.25), "notepad GIF");
        main.SetMode(rf::SetupMode);
        main.MutableSettings().setup.kind = 0;
        main.MutableSettings().setup.ending = 1;
        main.MutableSettings().setup.out.format = 2;
        main.MutableSettings().setup.out.square = true;
        main.Refresh();
        host.saveAnswer = dir + L"/setup.mp4";
        click(240, 59);
        rf::MediaInfo sm;
        Check(rf::ProbeMedia(host, host.saveAnswer, sm, err) && sm.hasAudio && sm.width == 1080 && Near(sm.duration, main.SceneLength(), 0.15), "copy window MP4 with the ding");
    }
    std::printf(failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 2;
}
