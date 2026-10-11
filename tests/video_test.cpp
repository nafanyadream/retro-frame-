#include "test_support.h"

#include "../src/core/icons.h"
#include "../src/core/mainview.h"
#include "../src/core/media.h"
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

    rf::FrameSettings Player(const std::wstring& title)
    {
        rf::FrameSettings f;
        f.colors = rf::SchemeColors(0, f.style);
        f.title = title;
        f.icon = rf::BuiltinIcon(rf::IconId::Film);
        f.menu = true;
        f.menuItems = {L"Файл", L"Вид", L"Воспроизведение", L"Избранное", L"Переход", L"?"};
        f.status = true;
        f.statusFields = {L"Воспроизведение", L"{время}"};
        f.statusIcon = false;
        f.grip = false;
        f.player = true;
        return f;
    }

    void ProbeSamples()
    {
        rf::MediaInfo m;
        const std::string phone =
            "Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'IMG_0001.MOV':\n"
            "  Duration: 00:01:30.50, start: 0.000000, bitrate: 8123 kb/s\n"
            "  Stream #0:0[0x1](und): Video: h264 (High) (avc1 / 0x31637661), yuv420p(tv, bt709, progressive), 1920x1080, 7900 kb/s, 29.97 fps, 29.97 tbr, 600 tbn (default)\n"
            "      Side data:\n"
            "        displaymatrix: rotation of -90.00 degrees\n"
            "  Stream #0:1[0x2](und): Audio: aac (LC) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 128 kb/s (default)\n"
            "At least one output file must be specified\n";
        Check(rf::ParseProbe(phone, m) && m.hasVideo && m.hasAudio && m.width == 1080 && m.height == 1920 && Near(m.fps, 29.97, 0.01) && Near(m.duration, 90.5, 0.01),
              "probe: phone video with rotation");
        const std::string gif = "Input #0, gif, from 'a.gif':\n  Duration: 00:00:02.00, start: 0.000000, bitrate: 66 kb/s\n  Stream #0:0: Video: gif, bgra, 160x120, 10 fps, 10 tbr, 100 tbn\n";
        Check(rf::ParseProbe(gif, m) && m.hasVideo && !m.hasAudio && m.width == 160 && Near(m.fps, 10, 0.01), "probe: animated gif");
        const std::string mp3 =
            "Input #0, mp3, from 'song.mp3':\n  Duration: 00:03:25.12, start: 0.025057, bitrate: 320 kb/s\n"
            "  Stream #0:0: Audio: mp3, 44100 Hz, stereo, fltp, 320 kb/s\n"
            "  Stream #0:1: Video: mjpeg (Baseline), yuvj420p(pc, bt470bg/unknown/unknown), 500x500 [SAR 1:1 DAR 1:1], 90k tbr, 90k tbn (attached pic)\n";
        Check(rf::ParseProbe(mp3, m) && !m.hasVideo && m.hasAudio && Near(m.duration, 205.12, 0.01), "probe: mp3 cover is not video");
        const std::string pal = "  Duration: 00:00:10.00, start: 0.0\n  Stream #0:0: Video: mpeg2video (Main), yuv420p(tv, top first), 720x576 [SAR 16:15 DAR 4:3], 25 fps, 25 tbr, 1k tbn\n";
        Check(rf::ParseProbe(pal, m) && m.width == 768 && m.height == 576, "probe: anamorphic width corrected");
        const std::string old = "  Duration: N/A, bitrate: N/A\n  Stream #0:0: Video: vp8, yuv420p(tv, progressive), 640x360, SAR 1:1 DAR 16:9, 30 fps, 30 tbr, 1k tbn\n    Metadata:\n      rotate          : 90\n";
        Check(rf::ParseProbe(old, m) && m.width == 360 && m.height == 640 && m.duration == 0.0, "probe: old rotate tag, unknown duration");
        Check(!rf::ParseProbe("garbage\nmore garbage", m), "probe: garbage rejected");
    }

    void TimeFormats()
    {
        double v = 0;
        Check(rf::FormatClock(90.5) == L"01:30" && rf::FormatPrecise(90.55) == L"01:30.5" && rf::FormatClock(3723.4) == L"1:02:03", "time formatting");
        Check(rf::ParseClock(L"1:30.5", v) && Near(v, 90.5, 1e-9), "parse mm:ss.d");
        Check(rf::ParseClock(L"75", v) && Near(v, 75, 1e-9), "parse plain seconds");
        Check(rf::ParseClock(L" 0:05,5 ", v) && Near(v, 5.5, 1e-9), "parse comma decimal");
        Check(rf::ParseClock(L"1:02:03", v) && Near(v, 3723, 1e-9), "parse hours");
        Check(!rf::ParseClock(L"abc", v) && !rf::ParseClock(L"1::2", v) && !rf::ParseClock(L"", v), "reject bad time");
    }

    void Plans()
    {
        const rf::FrameSettings f = Player(L"ролик.mp4");
        rf::VideoTarget window;
        const rf::VideoPlan a = rf::PlanVideo(f, 1920, 1080, window);
        Check(a.scale == 2 && a.videoW == 1920 && a.videoH == 1080 && a.outW % 2 == 0 && a.outH % 2 == 0 && a.outW > 1920, "plan: full hd window, 2x");
        const rf::VideoPlan b = rf::PlanVideo(f, 320, 240, window);
        Check(b.scale == 1 && b.clientW >= rf::PlayerMinWidth && b.outW % 2 == 0 && b.outH % 2 == 0, "plan: small video, even size");
        rf::VideoTarget sq;
        sq.square = true;
        const rf::VideoPlan c = rf::PlanVideo(f, 1920, 1080, sq);
        Check(c.outW == 1080 && c.outH == 1080 && c.scale == 3 && c.windowX >= 0 && c.windowY >= 0 && c.windowW <= 1080 && c.windowH <= 1080 && c.windowW > 900,
              "plan: square 1080, window 3x fills most of it");
        sq.squareSize = 720;
        const rf::VideoPlan d = rf::PlanVideo(f, 1920, 1080, sq);
        Check(d.outW == 720 && d.scale == 2 && d.windowW <= 720, "plan: square 720, 2x");
        sq.squareSize = 1080;
        const rf::VideoPlan e = rf::PlanVideo(f, 1080, 1920, sq);
        Check(e.clientW >= rf::PlayerMinWidth && e.videoW < e.clientW * e.scale && e.windowH <= 1080, "plan: portrait video letterboxed in square");
        rf::VideoTarget gif;
        gif.width = 480;
        const rf::VideoPlan g = rf::PlanVideo(f, 1920, 1080, gif);
        Check(g.outW <= 480 && g.scale == 1 && g.outW % 2 == 0, "plan: gif width limit");
        rf::FrameSettings manual = f;
        manual.scale = 2;
        const rf::VideoPlan h = rf::PlanVideo(manual, 1920, 1080, sq);
        Check(h.scale == 2 && h.windowW <= 1080, "plan: manual scale in square");
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
    ProbeSamples();
    TimeFormats();
    Plans();

    const std::wstring dir = rf::FromUtf8(out);
    const std::wstring clip = dir + L"/clip.mp4";
    const std::wstring gifSrc = dir + L"/anim.gif";
    Check(Run(host, {L"-v", L"error", L"-y", L"-f", L"lavfi", L"-i", L"testsrc=size=320x240:rate=25", L"-f", L"lavfi", L"-i", L"sine=frequency=440:sample_rate=44100",
                     L"-t", L"3", L"-c:v", L"libx264", L"-pix_fmt", L"yuv420p", L"-c:a", L"aac", L"-shortest", clip}),
          "make test clip");
    Check(Run(host, {L"-v", L"error", L"-y", L"-f", L"lavfi", L"-i", L"testsrc=size=160x120:rate=10", L"-t", L"2", gifSrc}), "make test gif");

    rf::MediaInfo info;
    std::wstring err;
    Check(rf::ProbeMedia(host, clip, info, err) && info.width == 320 && info.height == 240 && info.hasAudio && Near(info.duration, 3.0, 0.1) && Near(info.fps, 25, 0.01),
          "probe real clip");
    rf::MediaInfo gifInfo;
    Check(rf::ProbeMedia(host, gifSrc, gifInfo, err) && gifInfo.width == 160 && !gifInfo.hasAudio, "probe real gif");
    rf::MediaInfo none;
    Check(!rf::ProbeMedia(host, dir + L"/missing.mp4", none, err) && !err.empty(), "probe missing file fails with message");

    {
        rf::FrameStream one(host, clip, 1.0, 0.0, 0.0, 160, 120);
        rf::Image img;
        const bool got = one.Next(img);
        int bright = 0;
        for (auto c : img.pixels)
        {
            bright += rf::Luminance(c) > 40;
        }
        Check(got && img.width == 160 && img.height == 120 && bright > 1000, "single frame at 1s");
    }
    {
        rf::FrameStream stream(host, clip, 0.5, 1.0, 10.0, 80, 60);
        rf::Image img;
        int frames = 0;
        for (int spin = 0; spin < 4000 && !stream.Ended(); ++spin)
        {
            if (stream.Poll(img))
            {
                ++frames;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
        Check(frames >= 9 && frames <= 11, "non-blocking stream gives ~10 frames for 1s at 10 fps");
    }

    rf::ExportJob job;
    job.source = clip;
    job.frame = Player(L"clip.mp4");
    job.vars.name = L"clip.mp4";
    job.start = 0.5;
    job.duration = 2.0;
    job.fps = 25.0;
    job.audio = true;
    job.plan = rf::PlanVideo(job.frame, 320, 240, rf::VideoTarget{});
    job.output = dir + L"/out.mp4";
    int lastFrame = 0;
    const auto result = rf::ExportVideo(host, job, [&](int i, int) {
        lastFrame = i;
        return true;
    }, err);
    rf::MediaInfo o;
    Check(result == rf::ExportResult::Done && rf::ProbeMedia(host, job.output, o, err) && o.hasAudio && o.width == job.plan.outW && o.height == job.plan.outH &&
              Near(o.duration, 2.0, 0.15),
          "export mp4 window with audio and trim");

    rf::ExportJob gif = job;
    gif.gif = true;
    gif.fps = 10.0;
    gif.duration = 1.5;
    gif.audio = false;
    rf::VideoTarget sq;
    sq.square = true;
    sq.width = 480;
    gif.plan = rf::PlanVideo(gif.frame, 320, 240, sq);
    gif.background = rf::SquareBackground(rf::Image(), rf::Rgb(0x3A, 0x6E, 0xA5), gif.plan.outW);
    gif.output = dir + L"/out.gif";
    rf::MediaInfo og;
    Check(rf::ExportVideo(host, gif, [](int, int) { return true; }, err) == rf::ExportResult::Done && rf::ProbeMedia(host, gif.output, og, err) && og.width == 480 &&
              og.height == 480 && Near(og.duration, 1.5, 0.15),
          "export gif square 480");

    rf::ExportJob fromGif = job;
    fromGif.source = gifSrc;
    fromGif.start = 0.0;
    fromGif.duration = 2.0;
    fromGif.fps = 10.0;
    fromGif.audio = false;
    fromGif.plan = rf::PlanVideo(fromGif.frame, 160, 120, rf::VideoTarget{});
    fromGif.output = dir + L"/fromgif.mp4";
    rf::MediaInfo of;
    Check(rf::ExportVideo(host, fromGif, [](int, int) { return true; }, err) == rf::ExportResult::Done && rf::ProbeMedia(host, fromGif.output, of, err) && !of.hasAudio,
          "export from animated gif to mp4 without audio");

    rf::ExportJob cancel = job;
    cancel.output = dir + L"/cancel.mp4";
    Check(rf::ExportVideo(host, cancel, [](int i, int) { return i < 5; }, err) == rf::ExportResult::Cancelled && !host.FileExists(cancel.output),
          "cancel stops and removes partial file");

    rf::ExportJob broken = job;
    broken.source = dir + L"/missing.mp4";
    broken.output = dir + L"/broken.mp4";
    Check(rf::ExportVideo(host, broken, [](int, int) { return true; }, err) == rf::ExportResult::Failed && !err.empty() && !host.FileExists(broken.output),
          "export from missing source fails cleanly");

    {
        rf::Image frame;
        rf::FrameStream one(host, clip, 1.2, 0.0, 0.0, job.plan.videoW, job.plan.videoH);
        one.Next(frame);
        rf::PlayerComposer w(job.frame, job.vars, job.plan, rf::Image());
        rftest::WritePpm(out + "/player_window.ppm", w.Compose(frame, 0.4, L"00:01 / 00:03"));
        rf::VideoTarget s1080;
        s1080.square = true;
        const rf::VideoPlan sp = rf::PlanVideo(job.frame, 320, 240, s1080);
        rf::Image big;
        rf::FrameStream two(host, clip, 1.2, 0.0, 0.0, sp.videoW, sp.videoH);
        two.Next(big);
        rf::PlayerComposer s(job.frame, job.vars, sp, rf::SquareBackground(rf::Image(), rf::Rgb(0x3A, 0x6E, 0xA5), sp.outW));
        rftest::WritePpm(out + "/player_square.ppm", s.Compose(big, 0.4, L"00:01 / 00:03"));
    }
    {
        rf::MainView main(host);
        const auto shot = [&](const std::string& name) {
            rf::Image img;
            main.root.Paint(img);
            rftest::WritePpm(out + "/" + name + ".ppm", img);
        };
        const auto settle = [&](unsigned long long& ms, int ticks) {
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
        unsigned long long ms = 1000;
        main.SetMode(rf::VideoMode);
        main.Refresh();
        shot("ui_video_empty");
        main.OnDropFiles({clip});
        settle(ms, 25);
        Check(main.HasClip() && main.Settings().mode == rf::VideoMode, "drop video opens video tab");
        shot("ui_video");
        click(330 + 100 + 10, 471);
        settle(ms, 15);
        Check(main.Settings().video.square, "square radio switches result");
        shot("ui_video_square");
        click(30, 471);
        settle(ms, 2);
        main.root.SetFocus(nullptr);
        click(230, 497);
        settle(ms, 2);
        click(300, 426);
        settle(ms, 15);
        click(230, 523);
        settle(ms, 2);
        shot("ui_video_trim");
        click(60, 426);
        settle(ms, 30);
        shot("ui_video_playing");
        click(60, 426);
        settle(ms, 2);
        host.saveAnswer = dir + L"/ui_saved.mp4";
        click(240, 59);
        settle(ms, 2);
        rf::MediaInfo saved;
        Check(rf::ProbeMedia(host, host.saveAnswer, saved, err) && saved.width == 1080 && saved.height == 1080 && saved.hasAudio && saved.duration > 0.3 &&
                  saved.duration < 2.9,
              "save from video tab: square mp4 of the trimmed fragment");
        std::printf("    saved duration %.2f\n", saved.duration);
        main.SetMode(rf::PhotoMode);
        Check(!main.HasClip() || main.Settings().mode == rf::PhotoMode, "back to photo tab");
    }
    std::printf(failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 2;
}
