#include "test_support.h"

#include "../src/core/icons.h"
#include "../src/core/mainview.h"
#include "../src/core/music.h"
#include "../src/core/voice_view.h"
#include "test_host.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

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

    const std::string recDir = out + "/rec/";
    const std::string tmpDir = out + "/tmp/";
    std::filesystem::remove_all(recDir);
    std::filesystem::remove_all(tmpDir);
    std::filesystem::create_directories(recDir);
    std::filesystem::create_directories(tmpDir);
    host.recordingsFolder = rf::FromUtf8(recDir);
    host.tempFolder = rf::FromUtf8(tmpDir);
    main.MutableSettings().recorder.square = false;
    main.MutableSettings().recorder.format = 0;
    main.Refresh();
    const auto previewGreens = [&]() {
        rf::Image img;
        main.root.Paint(img);
        int n = 0;
        for (int y = 112; y < 408; ++y)
        {
            for (int x = 16; x < 316; ++x)
            {
                n += rf::Opaque(img.pixels[std::size_t(y * img.width + x)]) == rf::Rgb(0x00, 0xFF, 0x00);
            }
        }
        return n;
    };
    const auto fileBits = [](const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        unsigned char h[44] = {};
        f.read(reinterpret_cast<char*>(h), 44);
        return f ? int(h[34]) : -1;
    };
    click(386, 252);
    Check(main.Recording() && host.micOpen == 1 && host.micRate == 22050 && host.micName.empty(), "record button opens the microphone (Windows 98 quality)");
    settle(30);
    shot("ui_recording");
    Check(previewGreens() > 50, "live green wave while recording");
    click(386, 252);
    const std::string first = recDir + "Запись 1.wav";
    Check(!main.Recording() && host.micOpen == 0 && std::filesystem::exists(first) && fileBits(first) == 8, "stop saves «Запись 1.wav», 8 bit");
    Check(main.HasRecording() && main.RecordingIsVoice() && std::fabs(main.RecordingLength() - 1.2) < 0.15 && main.LevelsReady(),
          "recording opens in the tab with its wave");
    shot("ui_recorded");
    const double plain = main.RecordingLength();

    host.onModal = [&](rf::View& v) {
        auto* fx = dynamic_cast<rf::VoiceView*>(&v);
        if (fx == nullptr)
        {
            return 0;
        }
        rf::Image img;
        fx->root.Paint(img);
        rftest::WritePpm(out + "/ui_voice_dialog_empty.ppm", img);
        fx->ApplyPreset(2);
        const long long before = host.audioBytes;
        fx->Listen();
        for (int i = 1; i <= 5; ++i)
        {
            fx->OnTick(1000 + i * 40);
        }
        const bool heard = fx->Listening() && host.audioBytes > before && host.audioOpen == 1;
        fx->Select(1);
        fx->SetValue(0, 80);
        fx->Move(1);
        const bool moved = fx->Selected() == 2 && fx->Chain()[2].kind == rf::VoiceMetal && fx->Chain()[2].a == 80 && !fx->Listening();
        std::printf("     heard %d moved %d presets %zu\n", int(heard), int(moved), fx->UserPresets().size());
        fx->SavePreset(L"Мой голос");
        fx->root.Paint(img);
        rftest::WritePpm(out + "/ui_voice_dialog.ppm", img);
        Check(heard && moved && fx->UserPresets().size() == 1, "effects dialog: preset, listen, slider, order, own preset");
        return 1;
    };
    click(398, 282);
    const std::wstring withFx = main.RecordingSound();
    std::printf("     sound %s length %.3f plain %.3f summary %s\n", rf::ToUtf8(withFx).c_str(), main.RecordingLength(), plain,
                rf::ToUtf8(rf::VoiceSummary(main.Settings().voice)).c_str());
    Check(main.Settings().voice[0].kind == rf::VoicePitch && main.Settings().voice[0].on && main.Settings().voicePresets.size() == 1 &&
              withFx.find(L"/tmp/voice") != std::wstring::npos && host.FileExists(withFx) && std::fabs(main.RecordingLength() - plain) < 0.01,
          "effects applied: own sound file, same length");
    shot("ui_recorded_fx");

    host.onModal = [&](rf::View& v) {
        if (auto* fx = dynamic_cast<rf::VoiceView*>(&v))
        {
            fx->ApplyPreset(9);
            return 1;
        }
        return 0;
    };
    click(398, 282);
    Check(main.RecordingLength() > plain + 0.3 && !host.FileExists(withFx) && host.FileExists(main.RecordingSound()), "cave preset: longer, old file removed");
    const double cave = main.RecordingLength();

    host.saveAnswer = dir + L"/voice_out.mp4";
    click(240, 59);
    settle(2);
    rf::MediaInfo vm;
    Check(rf::ProbeMedia(host, host.saveAnswer, vm, err) && vm.hasAudio && std::fabs(vm.duration - cave) < 0.2, "MP4 with the voice and effects");
    main.MutableSettings().recorder.format = 4;
    main.Refresh();
    Check(main.ShownFormat() == 4, "WAV format in the list");
    host.soundAnswer = dir + L"/voice_out.wav";
    click(240, 59);
    settle(2);
    rf::MediaInfo wm;
    Check(rf::ProbeAudio(host, host.soundAnswer, wm, err) && wm.hasAudio && !wm.hasVideo && std::fabs(wm.duration - cave) < 0.1 && fileBits(out + "/voice_out.wav") == 8,
          "WAV only sound, 8 bit like Windows 98");
    main.MutableSettings().recorder.format = 5;
    main.Refresh();
    host.soundAnswer = dir + L"/voice_out.mp3";
    click(240, 59);
    settle(2);
    rf::MediaInfo mm;
    Check(rf::ProbeAudio(host, host.soundAnswer, mm, err) && mm.hasAudio && std::fabs(mm.duration - cave) < 0.2, "MP3 only sound");
    main.MutableSettings().recorder.format = 0;
    main.Refresh();

    host.onModal = [&](rf::View&) { return 0; };
    main.MutableSettings().micQuality = 1;
    host.micSilent = true;
    const int modalsBefore = host.modalCalls;
    click(386, 252);
    Check(host.micRate == 44100, "normal quality records at 44 kHz");
    settle(10);
    click(386, 252);
    Check(host.modalCalls == modalsBefore + 1 && fileBits(recDir + "Запись 2.wav") == 16, "silence warning, 16 bit file");
    host.micSilent = false;
    click(386, 252);
    settle(10);
    main.SetMode(rf::MusicMode);
    Check(!main.Recording() && std::filesystem::exists(recDir + "Запись 3.wav") && main.ShownTab() == rf::MusicMode && main.Settings().mode == rf::MusicMode,
          "switching tabs stops and keeps the recording");
    main.SetMode(rf::RecorderMode);
    Check(main.RecordingIsVoice() && main.HasTrack(), "recorder tab shows the new recording");
    click(340, 307);
    const long long heardBefore = host.audioBytes;
    click(386, 252);
    settle(20);
    const bool monitoring = host.audioOpen == 1 && host.audioBytes > heardBefore + 4000;
    shot("ui_recording_monitor");
    click(386, 252);
    Check(main.Settings().micMonitor && monitoring && host.audioOpen == 0, "hear yourself while recording, sound stops with the recording");
    click(340, 307);
    Check(!main.Settings().micMonitor, "hear yourself is switchable");
    host.micWorks = false;
    const int modalsMic = host.modalCalls;
    click(386, 252);
    Check(!main.Recording() && host.modalCalls == modalsMic + 1, "broken microphone: message, no recording");
    host.micWorks = true;
    main.OnDropFiles({song});
    Check(main.HasRecording() && !main.RecordingIsVoice() && main.RecordingSound() == song, "songs from elsewhere get no voice effects");
    shot("ui_recorder_song_no_fx");
    std::printf(failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 2;
}
