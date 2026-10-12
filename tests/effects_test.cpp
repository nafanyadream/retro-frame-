#include "test_support.h"

#include "../src/core/dialogs.h"
#include "../src/core/icons.h"
#include "../src/core/mainview.h"
#include "../src/core/music.h"
#include "test_host.h"

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
        const rf::Stage stage{rf::Rect{0, 0, framed.width, framed.height}, k};
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

        rf::EffectSettings intro;
        intro.intro = true;
        const rf::Image bg(400, 400, rf::Rgb(0x20, 0x40, 0x80));
        const rf::Stage centre{rf::Rect{50, 60, 300, 260}, 1};
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
        rf::AppSettings back = rf::DefaultSettings();
        rf::ParseSettings(rf::SerializeSettings(s), back);
        const rf::EffectSettings& b = back.effects;
        Check(b.intro && b.cursor == 3 && b.cursorPlace == 2 && b.popups == 2 && b.popupIcon == 3 && b.popupButtons == 2 && b.popupTitle == s.effects.popupTitle &&
                  b.popupText == s.effects.popupText && b.screen == 2,
              "effects settings round trip");
        const rf::EffectSettings d = rf::DefaultSettings().effects;
        Check(!d.intro && d.cursor == 0 && d.popups == 0 && d.screen == 0 && !rf::HasOverlays(d), "effects are off by default");
        rf::EffectsView view(s.effects);
        rf::Image dialog;
        view.root.Paint(dialog);
        rftest::WritePpm(Out() + "/ui_effects_dialog.ppm", dialog);
        const rf::EffectSettings r = view.Result();
        Check(r.intro && r.cursor == 3 && r.popups == 2 && r.popupText == s.effects.popupText && r.popupTitle == s.effects.popupTitle && r.screen == 2,
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
    }
    std::printf(failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 2;
}
