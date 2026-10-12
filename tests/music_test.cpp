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

    bool Near(double a, double b, double eps)
    {
        return std::fabs(a - b) <= eps;
    }

    bool Run(rftest::TestHost& host, const std::vector<std::wstring>& args)
    {
        auto p = host.StartProcess(host.ffmpeg, args, false, false);
        int code = -1;
        return p && p->Finish(code, []() { return true; }) && code == 0;
    }

    rf::FrameSettings Song(int scheme)
    {
        rf::FrameSettings f;
        f.style = rf::Style::Win98;
        f.colors = rf::SchemeColors(scheme, f.style);
        f.title = L"Песня";
        f.kind = rf::WindowKind::Bare;
        return f;
    }

    std::vector<float> Levels(rftest::TestHost& host, const std::wstring& path)
    {
        rf::LevelReader reader(host, path);
        reader.Wait();
        return reader.Levels();
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
    rftest::TestHost host;
    host.ffmpeg = rf::FromUtf8(argv[2]);
    const std::wstring dir = rf::FromUtf8(out);

    {
        rf::MediaInfo m;
        const std::string mp3 =
            "Input #0, mp3, from 'song.mp3':\n"
            "  Metadata:\n"
            "    title           : Огни города\n"
            "    artist          : Ночной экспресс\n"
            "    encoder         : Lavf60.16.100\n"
            "  Duration: 00:03:25.12, start: 0.025057, bitrate: 320 kb/s\n"
            "  Stream #0:0: Audio: mp3, 44100 Hz, stereo, fltp, 320 kb/s\n"
            "  Stream #0:1: Video: mjpeg (Baseline), yuvj420p(pc, bt470bg/unknown/unknown), 500x500 [SAR 1:1 DAR 1:1], 90k tbr, 90k tbn (attached pic)\n"
            "    Metadata:\n"
            "      title           : Album cover\n"
            "      comment         : Cover (front)\n";
        Check(rf::ParseProbe(mp3, m) && m.hasAudio && !m.hasVideo && m.hasCover && m.coverW == 500 && m.artist == L"Ночной экспресс" && m.title == L"Огни города",
              "probe: mp3 tags and cover");
        const std::string ogg =
            "Input #0, ogg, from 'a.ogg':\n  Duration: 00:00:10.00, start: 0.000000, bitrate: 112 kb/s\n"
            "  Stream #0:0: Audio: vorbis, 44100 Hz, stereo, fltp, 112 kb/s\n    Metadata:\n      TITLE           : Song: Part 1\n      ARTIST          : Somebody\n";
        Check(rf::ParseProbe(ogg, m) && m.title == L"Song: Part 1" && m.artist == L"Somebody" && !m.hasCover, "probe: ogg stream tags, colon in value");
        const std::string plain = "Input #0, wav, from 'a.wav':\n  Duration: 00:00:02.00, bitrate: 1411 kb/s\n  Stream #0:0: Audio: pcm_s16le, 44100 Hz, 2 channels, s16, 1411 kb/s\n";
        Check(rf::ParseProbe(plain, m) && m.artist.empty() && m.title.empty() && m.hasAudio, "probe: wav without tags");
        Check(rf::FormatShort(93) == L"1:33" && rf::FormatShort(205.9) == L"3:25" && rf::FormatShort(3723) == L"1:02:03", "short time format");
    }

    {
        rf::AppSettings old = rf::DefaultSettings();
        rf::ParseSettings("version=2\nmusic.icon=6\nmusic.kind=0\nicon=3\n", old);
        Check(old.looks[rf::MusicMode].iconIndex == rf::NoIconIndex && old.looks[rf::MusicMode].kind == rf::WindowKind::Bare && old.looks[rf::PhotoMode].iconIndex == 3,
              "settings from older version: music window reset, photo kept");
        rf::AppSettings now = rf::DefaultSettings();
        now.looks[rf::MusicMode].iconIndex = 4;
        now.music.fade = 5;
        now.music.square = true;
        rf::AppSettings back = rf::DefaultSettings();
        rf::ParseSettings(rf::SerializeSettings(now), back);
        Check(back.looks[rf::MusicMode].iconIndex == 4 && back.music.fade == 5 && back.music.square, "music settings round trip");
        Check(!rf::DefaultSettings().music.square && !old.music.square, "music result is only the window by default");
    }

    const rf::MusicPlan p1080 = rf::PlanMusic(Song(0), 1080, true);
    const rf::MusicPlan p720 = rf::PlanMusic(Song(0), 720, true);
    Check(p1080.scale == 3 && p1080.windowW <= 1080 && p1080.windowW > 850 && p720.scale == 2 && p720.windowW <= 720 && p1080.outW == 1080 && p1080.outH == 1080,
          "plan: window 3x in 1080, 2x in 720");
    rf::FrameSettings manual = Song(0);
    manual.scale = 4;
    Check(rf::PlanMusic(manual, 1080, true).scale == 3, "plan: too big manual scale falls back to fitting one");
    const rf::MusicPlan w1080 = rf::PlanMusic(Song(0), 1080, false);
    Check(!w1080.square && w1080.scale == 3 && w1080.outW == w1080.windowW && w1080.outH == w1080.windowH && w1080.outW <= 1080 && w1080.outH <= 1080 &&
              w1080.windowX == 0 && w1080.windowY == 0,
          "plan: only the window, 3x");
    Check(rf::PlanMusic(manual, 1080, false).scale == 4, "plan: only the window keeps manual 4x");
    {
        bool even = true;
        for (int k = 1; k <= 4; ++k)
        {
            for (int variant = 0; variant < 8; ++variant)
            {
                rf::FrameSettings f = Song(variant % 3);
                f.scale = k;
                f.kind = variant % 2 == 0 ? rf::WindowKind::Bare : rf::WindowKind::Normal;
                f.menu = variant >= 4;
                f.status = variant % 4 >= 2;
                f.statusFields = {L"Воспроизведение", L"{время}"};
                f.menuItems = {L"Файл", L"Вид"};
                const rf::MusicPlan w = rf::PlanMusic(f, 1080, false);
                even = even && w.scale == k && w.outW % 2 == 0 && w.outH % 2 == 0 && w.clientW - rf::MusicClientW <= 1 && w.clientH - rf::MusicClientH <= 1;
            }
        }
        Check(even, "plan: only the window always has even sides for mp4");
    }

    const std::wstring track = dir + L"/песня.mp3";
    const std::wstring bare = dir + L"/bare.wav";
    const std::wstring coverPng = dir + L"/cover.png";
    Check(Run(host, {L"-v", L"error", L"-y", L"-f", L"lavfi", L"-i", L"testsrc=size=300x300:rate=1", L"-frames:v", L"1", coverPng}), "make test cover");
    Check(Run(host, {L"-v", L"error", L"-y", L"-f", L"lavfi", L"-i", L"sine=frequency=440:sample_rate=44100:duration=6", L"-i", coverPng,
                     L"-map", L"0:a", L"-map", L"1:v", L"-af", L"volume='if(lt(t,3),0.03,0.9)':eval=frame",
                     L"-c:a", L"libmp3lame", L"-c:v", L"mjpeg", L"-disposition:v", L"attached_pic", L"-metadata", L"artist=Ночной экспресс", L"-metadata",
                     L"title=Огни города", L"-id3v2_version", L"3", track}),
          "make test song with cover and tags");
    Check(Run(host, {L"-v", L"error", L"-y", L"-f", L"lavfi", L"-i", L"sine=frequency=220:sample_rate=22050:duration=3", bare}), "make plain wav");

    rf::MediaInfo info;
    std::wstring err;
    Check(rf::ProbeAudio(host, track, info, err) && info.artist == L"Ночной экспресс" && info.title == L"Огни города" && info.hasCover && Near(info.duration, 6.0, 0.15),
          "probe real song: tags, cover, duration");
    rf::MediaInfo bareInfo;
    Check(rf::ProbeAudio(host, bare, bareInfo, err) && !bareInfo.hasCover && bareInfo.artist.empty(), "probe real wav");
    rf::MediaInfo videoInfo;
    Check(!rf::ProbeAudio(host, dir + L"/missing.mp3", videoInfo, err) && !err.empty(), "probe missing track fails");
    rf::Image cover;
    Check(rf::ReadCover(host, track, info, 600, cover) && cover.width == 300 && cover.height == 300, "read cover from tags");

    const std::vector<float> levels = Levels(host, track);
    float quiet = 0.0f;
    float loud = 0.0f;
    for (std::size_t i = 0; i < levels.size(); ++i)
    {
        (i < 70 ? quiet : loud) = std::max(i < 70 ? quiet : loud, i >= 5 && i < 70 ? levels[i] : (i >= 80 && i < 140 ? levels[i] : 0.0f));
    }
    std::printf("    levels: %zu, quiet max %.2f, loud max %.2f\n", levels.size(), quiet, loud);
    Check(levels.size() >= 145 && levels.size() <= 155 && loud > quiet + 0.3f, "levels follow loudness");
    {
        rf::LevelReader poller(host, track);
        for (int spin = 0; spin < 5000 && !poller.Poll(); ++spin)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        Check(poller.Done() && poller.Levels().size() == levels.size(), "non-blocking level reading matches");
    }

    rf::MusicJob job;
    job.source = track;
    job.output = dir + L"/song_out.mp4";
    job.start = 1.0;
    job.duration = 4.0;
    job.fade = 1.0;
    job.frame = Song(2);
    job.vars.name = L"песня.mp3";
    job.plan = p1080;
    job.background = rf::SquareBackground(rf::Image(), rf::Rgb(0x2E, 0x5B, 0x88), 1080);
    job.artist = info.artist;
    job.title = info.title;
    job.cover = cover;
    job.levels = levels;
    Check(rf::ExportMusic(host, job, [](int, int) { return true; }, err) == rf::ExportResult::Done, "export music fragment");
    rf::MediaInfo o;
    Check(rf::ProbeMedia(host, job.output, o, err) && o.width == 1080 && o.height == 1080 && o.hasAudio && Near(o.duration, 4.0, 0.15), "music mp4: square with sound, fragment length");
    const std::vector<float> outLevels = Levels(host, job.output);
    Check(outLevels.size() > 90 && outLevels[1] + 0.1f < outLevels[75] && outLevels[outLevels.size() - 2] + 0.1f < outLevels[75], "fade in and out at fragment edges");

    rf::MusicJob cancel = job;
    cancel.output = dir + L"/song_cancel.mp4";
    Check(rf::ExportMusic(host, cancel, [](int i, int) { return i < 10; }, err) == rf::ExportResult::Cancelled && !host.FileExists(cancel.output), "cancel music export");

    {
        rf::MusicComposer red(Song(2), job.vars, p1080, job.background, info.artist, info.title, cover);
        rftest::WritePpm(out + "/music_square.ppm", red.Compose(rf::MusicFrameAt(93.0 - 60.0, 205.0 - 60.0, 3.5, true, true, levels)));
        rf::MusicComposer plainSong(Song(0), job.vars, p720, rf::SquareBackground(rf::Image(), rf::Rgb(0, 0x80, 0x80), 720), L"", L"bare", rf::Image());
        rftest::WritePpm(out + "/music_nocover.ppm", plainSong.Compose(rf::MusicFrameAt(0.0, 3.0, 0.0, true, true, {})));
        const rf::Image small = red.Small(rf::MusicFrameAt(10.0, 30.0, 10.0, true, true, levels), rf::SquareBackground(rf::Image(), rf::Rgb(0x2E, 0x5B, 0x88), 360));
        Check(small.width == 360 && small.height == 360, "small preview is square at 1x");
        rftest::WritePpm(out + "/music_small.ppm", small);
        rf::MusicComposer alone(Song(2), job.vars, w1080, rf::Image(), info.artist, info.title, cover);
        const rf::Image& window = alone.Compose(rf::MusicFrameAt(12.0, 40.0, 12.0, true, true, levels));
        const rf::Color corner = rf::Opaque(window.pixels[0]);
        Check(window.width == w1080.outW && window.height == w1080.outH && corner != rf::Rgb(0, 0, 0) && corner != rf::Opaque(rf::Rgb(0x2E, 0x5B, 0x88)),
              "only the window: no background around it");
        rftest::WritePpm(out + "/music_window.ppm", window);
        const rf::Image smallWindow = alone.Small(rf::MusicFrameAt(12.0, 40.0, 12.0, true, true, levels), rf::Image());
        Check(smallWindow.width * w1080.scale == w1080.outW && smallWindow.height * w1080.scale == w1080.outH, "small preview of the window is 1x");
    }
    {
        rf::MusicJob alone = job;
        alone.output = dir + L"/song_window.mp4";
        alone.start = 2.0;
        alone.duration = 1.0;
        alone.fade = 0.0;
        alone.plan = w1080;
        alone.background = rf::Image();
        rf::MediaInfo w;
        Check(rf::ExportMusic(host, alone, [](int, int) { return true; }, err) == rf::ExportResult::Done && rf::ProbeMedia(host, alone.output, w, err) &&
                  w.width == w1080.outW && w.height == w1080.outH && w.hasAudio,
              "music mp4 with only the window: window size with sound");
    }
    {
        rf::MainView main(host);
        const auto shot = [&](const std::string& name) {
            rf::Image img;
            main.root.Paint(img);
            rftest::WritePpm(out + "/" + name + ".ppm", img);
        };
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
        main.SetMode(rf::MusicMode);
        main.Refresh();
        shot("ui_music_empty");
        main.OnDropFiles({track});
        for (int i = 0; i < 200 && !main.LevelsReady(); ++i)
        {
            settle(1);
        }
        Check(main.HasTrack() && main.Settings().mode == rf::MusicMode && main.LevelsReady(), "drop song opens music tab and counts loudness");
        click(300, 426);
        settle(2);
        main.root.SetFocus(nullptr);
        click(500, 164);
        main.root.Key(rf::ui::KeyEvent{rf::ui::KeyCode::End});
        for (wchar_t ch : std::wstring(L" (live)"))
        {
            main.root.Char(ch);
        }
        settle(2);
        shot("ui_music");
        click(60, 426);
        settle(20);
        shot("ui_music_playing");
        click(60, 426);
        settle(2);
        main.MutableSettings().music.format = 3;
        main.Refresh();
        click(340, 221);
        settle(1);
        Check(main.ShownFormat() == 3 && !main.Settings().music.spin, "format list shows the music format after other changes");
        click(340, 221);
        settle(1);
        host.saveAnswer = dir + L"/still_window.png";
        click(240, 59);
        settle(2);
        const rf::Image alone = host.writtenImage;
        Check(host.written == host.saveAnswer && alone.width < 1080 && alone.height < 1080 && alone.width > 600 && alone.width != alone.height &&
                  rf::Opaque(alone.pixels[0]) != rf::Opaque(main.Settings().background),
              "save png: only the window, no background");
        rftest::WritePpm(out + "/ui_music_window.ppm", alone);
        click(440, 470);
        settle(1);
        Check(main.Settings().music.square, "square choice in the music tab");
        shot("ui_music_square");
        host.saveAnswer = dir + L"/still.png";
        click(240, 59);
        settle(2);
        Check(host.written == host.saveAnswer && host.writtenImage.width == 1080 && host.writtenImage.height == 1080, "save png frame 1080");
        rftest::WritePpm(out + "/ui_music_still.ppm", host.writtenImage);
        main.root.Key(rf::ui::KeyEvent{rf::ui::KeyCode::Letter, L'C', false, true, false});
        settle(1);
        Check(host.clipboard.width == 1080, "ctrl+c copies the frame");
        main.MutableSettings().music.format = 1;
        main.MutableSettings().music.squareSize = 720;
        main.Refresh();
        click(30, 471);
        settle(1);
        main.root.SetFocus(nullptr);
        host.saveAnswer = dir + L"/ui_song.mp4";
        click(240, 59);
        settle(2);
        rf::MediaInfo saved;
        Check(rf::ProbeMedia(host, host.saveAnswer, saved, err) && saved.width == 720 && saved.hasAudio && saved.duration > 1.0, "save mp4 720 from music tab");
        std::printf("    saved %.2f s\n", saved.duration);
    }
    std::printf(failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 2;
}
