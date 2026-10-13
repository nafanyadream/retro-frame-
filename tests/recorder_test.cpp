#include "test_support.h"

#include "../src/core/icons.h"
#include "../src/core/mainview.h"
#include "../src/core/music.h"
#include "test_host.h"

#include <cmath>
#include <cstdio>

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

    bool Run(rftest::TestHost& host, const std::vector<std::wstring>& args)
    {
        auto p = host.StartProcess(host.ffmpeg, args, false, false);
        int code = -1;
        return p && p->Finish(code, []() { return true; }) && code == 0;
    }

    int Greens(const rf::Image& img)
    {
        int n = 0;
        for (auto px : img.pixels)
        {
            n += rf::Opaque(px) == rf::Rgb(0x00, 0xFF, 0x00);
        }
        return n;
    }

    rf::FrameSettings RecorderWindow()
    {
        rf::FrameSettings f;
        f.style = rf::Style::Win98;
        f.colors = rf::SchemeColors(0, f.style);
        f.title = rf::RecorderTitle(L"Windows 9.90", false);
        f.menu = true;
        f.menuItems = rf::RecorderMenu(false);
        f.icon = rf::BuiltinIcon(rf::IconId::Microphone);
        return f;
    }

    void Core(const std::string& out)
    {
        Check(rf::RecorderSeconds(413.96, false) == L"413.96 sec." && rf::RecorderSeconds(3668.99, true) == L"3668,99 сек." &&
                  rf::RecorderSeconds(0.5, false) == L"0.50 sec.",
              "seconds like in Windows");
        Check(rf::RecorderTitle(L"Windows 9.90", false) == L"Windows 9.90 - Sound Recorder" && rf::RecorderTitle(L"", true) == L"Звук - Звукозапись" &&
                  rf::RecorderTitle(L"Звук - Звукозапись", true) == L"Звук - Звукозапись" && rf::RecorderMenu(true)[2] == L"Эффекты" &&
                  rf::RecorderMenu(false)[2] == L"Effects",
              "title and menu in both languages");
        const rf::FrameSettings f = RecorderWindow();
        const rf::MusicPlan plan = rf::PlanMusic(f, 1080, 1080, false, false, true);
        Check(plan.clientW == rf::RecorderClientW && plan.clientH >= rf::RecorderClientH && plan.outW % 2 == 0 && plan.outH % 2 == 0 && plan.outW <= 1080 &&
                  plan.outW > plan.outH,
              "plan: small wide window");
        const rf::MusicPlan tall = rf::PlanMusic(f, 1080, 1920, true, false, true);
        Check(tall.outW == 1080 && tall.outH == 1920 && tall.windowW <= 1080 && std::abs(tall.windowY - (1920 - tall.windowH) / 2) <= 1, "plan: 9:16 frame");
        auto sine = std::make_shared<std::vector<std::int16_t>>(std::size_t(rf::AnalysisRate) * 4);
        for (std::size_t i = 0; i < sine->size(); ++i)
        {
            (*sine)[i] = std::int16_t(std::lround(20000.0 * std::sin(2.0 * 3.14159265358979 * 300.0 * double(i) / rf::AnalysisRate)));
        }
        const auto silent = std::make_shared<std::vector<std::int16_t>>(std::size_t(rf::AnalysisRate) * 4, std::int16_t(0));
        rf::MusicComposer loud(f, rf::FrameVars{L"a.wav", 0, 0, L""}, plan, rf::Image(), L"", L"", rf::Image());
        loud.SetRecorder(false, 4.0, sine);
        rf::MusicComposer quiet(f, rf::FrameVars{L"a.wav", 0, 0, L""}, plan, rf::Image(), L"", L"", rf::Image());
        quiet.SetRecorder(true, 4.0, silent);
        const rf::Image a = loud.Small(rf::MusicFrameAt(1.0, 4.0, 1.0, false, false, {}), rf::Image());
        const rf::Image b = quiet.Small(rf::MusicFrameAt(1.0, 4.0, 1.0, false, false, {}), rf::Image());
        const rf::Image late = loud.Small(rf::MusicFrameAt(3.0, 4.0, 3.0, false, false, {}), rf::Image());
        Check(Greens(b) > 80 && Greens(b) < 140 && Greens(a) > Greens(b) * 3 && a.pixels != late.pixels, "live green wave, flat line in silence");
        rftest::WritePpm(out + "/recorder_en.ppm", a);
        rftest::WritePpm(out + "/recorder_ru.ppm", b);
        const rf::Image big = loud.Compose(rf::MusicFrameAt(1.0, 4.0, 1.0, false, false, {}));
        Check(big.width == plan.outW && big.height == plan.outH && Greens(big) > Greens(a) * 4, "full size frame");
    }

    void Settings()
    {
        rf::AppSettings s = rf::DefaultSettings();
        Check(s.looks[rf::RecorderMode].iconIndex == 10 && rf::FrameIconAt(10) == rf::IconId::Microphone && s.looks[rf::RecorderMode].menu &&
                  s.looks[rf::RecorderMode].kind == rf::WindowKind::Normal && !s.recorder.russian,
              "recorder window defaults: microphone, menu, normal window");
        s.recorder.russian = true;
        s.recorder.format = 3;
        s.recorder.square = true;
        s.recorder.canvas = 3;
        s.recorder.fade = 5;
        s.recorder.fps = 30;
        s.looks[rf::RecorderMode].customTitle = L"Звук";
        s.mode = rf::RecorderMode;
        rf::AppSettings back = rf::DefaultSettings();
        rf::ParseSettings(rf::SerializeSettings(s), back);
        Check(back.recorder.russian && back.recorder.format == 3 && back.recorder.square && back.recorder.canvas == 3 && back.recorder.fade == 5 &&
                  back.recorder.fps == 30 && back.mode == rf::RecorderMode && back.music.format == 0 && !back.music.square,
              "recorder settings are separate from music and saved");
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        return 1;
    }
    rftest::TestTextEngine engine;
    rf::SetTextEngine(&engine);
    const std::string out = argv[1];
    Core(out);
    Settings();

    rftest::TestHost host;
    host.ffmpeg = rf::FromUtf8(argv[2]);
    const std::wstring dir = rf::FromUtf8(out);
    const std::wstring song = dir + L"/запись.wav";
    const std::wstring other = dir + L"/другая.wav";
    Check(Run(host, {L"-v", L"error", L"-y", L"-f", L"lavfi", L"-i", L"sine=frequency=440:sample_rate=44100:duration=4", L"-af", L"volume=0.7", song}) &&
              Run(host, {L"-v", L"error", L"-y", L"-f", L"lavfi", L"-i", L"sine=frequency=220:sample_rate=44100:duration=2", other}),
          "make test songs");

    rf::MainView main(host);
    unsigned long long ms = 1000;
    const auto settle = [&](int ticks) {
        for (int i = 0; i < ticks; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
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
    };
    main.SetMode(rf::MusicMode);
    main.OnDropFiles({other});
    main.SetMode(rf::RecorderMode);
    main.Refresh();
    shot("ui_recorder_empty");
    Check(main.HasTrack() && !main.HasRecording(), "recorder tab starts empty, music keeps its song");
    main.OnDropFiles({song});
    for (int i = 0; i < 200 && !main.LevelsReady(); ++i)
    {
        settle(1);
    }
    Check(main.HasRecording() && main.Settings().mode == rf::RecorderMode && main.LevelsReady(), "drop a song into the recorder tab");
    main.SetMode(rf::MusicMode);
    main.SetMode(rf::RecorderMode);
    Check(main.HasTrack() && main.HasRecording(), "tabs keep their own songs");
    click(300, 426);
    settle(2);
    shot("ui_recorder");
    main.MutableSettings().recorder.format = 3;
    main.Refresh();
    host.saveAnswer = dir + L"/recorder_still.png";
    click(240, 59);
    settle(2);
    const rf::Image still = host.writtenImage;
    Check(host.written == host.saveAnswer && still.width > still.height && Greens(still) > 500, "save a PNG frame with the wave");
    rftest::WritePpm(out + "/ui_recorder_still.ppm", still);
    main.root.Wheel(500, 138, -120);
    settle(1);
    Check(main.Settings().recorder.russian, "language list switches to Russian");
    host.saveAnswer = dir + L"/recorder_still_ru.png";
    click(240, 59);
    settle(2);
    Check(host.writtenImage.width == still.width && host.writtenImage.pixels != still.pixels, "Russian labels change the frame");
    rftest::WritePpm(out + "/ui_recorder_still_ru.ppm", host.writtenImage);
    shot("ui_recorder_ru");
    ms = 0;
    click(60, 426);
    for (int i = 0; i < 60 && host.audioBytes == 0; ++i)
    {
        settle(1);
    }
    Check(host.audioOpen == 1 && host.audioBytes > 0, "preview plays with sound");
    click(60, 426);
    settle(2);
    main.MutableSettings().recorder.format = 2;
    main.MutableSettings().recorder.square = true;
    main.MutableSettings().recorder.canvas = 4;
    main.Refresh();
    host.saveAnswer = dir + L"/recorder_out.mp4";
    click(240, 59);
    settle(2);
    rf::MediaInfo m;
    std::wstring err;
    Check(rf::ProbeMedia(host, host.saveAnswer, m, err) && m.width == 720 && m.height == 1280 && m.hasAudio && std::fabs(m.duration - 4.0) < 0.2,
          "save MP4 9:16 with sound from the recorder tab");
    Check(main.Settings().music.format == 0 && !main.Settings().music.square, "music tab settings untouched");
    std::printf(failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 2;
}
