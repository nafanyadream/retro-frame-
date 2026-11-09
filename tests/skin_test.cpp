#include "test_support.h"

#include "../src/core/mainview.h"
#include "../src/core/smooth.h"
#include "../src/core/wmp12.h"
#include "test_host.h"

#include <cmath>
#include <csignal>
#include <cstdio>
#include <filesystem>

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

    bool Close(int a, int b, int eps)
    {
        return std::abs(a - b) <= eps;
    }

    int BlueIn(const rf::Image& img, const rf::Rect& r)
    {
        int n = 0;
        for (int y = std::max(0, r.y); y < std::min(img.height, r.Bottom()); ++y)
        {
            for (int x = std::max(0, r.x); x < std::min(img.width, r.Right()); ++x)
            {
                const rf::Color c = img.Get(x, y);
                n += rf::Blue(c) > 150 && rf::Blue(c) > rf::Red(c) + 60 ? 1 : 0;
            }
        }
        return n;
    }

    rf::FrameSettings Frame()
    {
        rf::FrameSettings f;
        f.style = rf::Style::Win98;
        f.colors = rf::SchemeColors(0, f.style);
        f.title = L"ролик";
        f.player = true;
        return f;
    }

    void Smooth()
    {
        rf::Image img(40, 40, rf::Rgb(0, 0, 0));
        rf::FillDisc(img, 20.0, 20.0, 10.0, rf::Rgb(0xFF, 0xFF, 0xFF));
        bool partial = false;
        for (const rf::Color c : img.pixels)
        {
            const int v = rf::Red(c);
            partial = partial || (v > 20 && v < 235);
        }
        Check(rf::Red(img.Get(20, 20)) == 255 && rf::Red(img.Get(2, 2)) == 0 && partial, "smooth disc: solid inside, soft edge");
        rf::Image tri(30, 30, rf::Rgb(0, 0, 0));
        rf::FillPolygon(tri, {{5.0, 5.0}, {25.0, 5.0}, {5.0, 25.0}}, rf::Rgb(0xFF, 0, 0));
        Check(rf::Red(tri.Get(8, 8)) == 255 && rf::Red(tri.Get(22, 22)) == 0 && rf::Red(tri.Get(15, 14)) > 0, "smooth triangle");
        Check(rf::Ramp({{0.0, rf::Rgb(0, 0, 0)}, {1.0, rf::Rgb(200, 0, 0)}}, 0.5) == rf::Mix(rf::Rgb(0, 0, 0), rf::Rgb(200, 0, 0), 0.5), "colour ramp");
    }

    void Layout(const std::string& out)
    {
        rf::Wmp12Look look;
        look.title = L"Alarm01";
        look.elapsed = L"00:01";
        look.total = L"02:30";
        look.position = 0.4;
        for (int frame = 0; frame < 2; ++frame)
        {
            look.frame = frame;
            int cw = 0;
            int ch = 0;
            int cx = 0;
            int cy = 0;
            rf::Wmp12Chrome(frame, &cw, &ch, &cx, &cy);
            rf::Rect c1;
            rf::Rect c2;
            rf::Wmp12Spots s1;
            rf::Wmp12Spots s2;
            const rf::Image one = rf::DrawWmp12(look, 640, 360, 1, &c1, &s1);
            const rf::Image two = rf::DrawWmp12(look, 640, 360, 2, &c2, &s2);
            const bool sizes = one.width == 640 + cw && one.height == 360 + ch && two.width == 2 * one.width && two.height == 2 * one.height;
            const bool content = c1.x == cx && c1.y == cy && c1.w == 640 && c2.x == 2 * cx && c2.w == 1280;
            const bool spots = Close(s2.play.x, 2 * s1.play.x, 2) && Close(s2.seek.w, 2 * s1.seek.w, 2) && Close(s2.close.x, 2 * s1.close.x, 2) &&
                               s1.play.Bottom() <= one.height && s1.close.Right() <= one.width && s1.minimize.x < s1.close.x;
            const bool corners = rf::Alpha(one.Get(0, 0)) < 128 && rf::Alpha(one.Get(one.width / 2, one.height / 2)) == 255;
            Check(sizes && content && spots && corners, frame == 0 ? "media player 12, Windows 7 frame: sizes, content, buttons scale with the frame"
                                                                   : "media player 12, Windows 10/11 frame: sizes, content, buttons scale with the frame");
            rftest::WritePpm(out + "/skin_wmp12_" + std::to_string(frame) + ".ppm", two);
        }
        look.frame = 0;
        look.position = 0.1;
        const rf::Image early = rf::DrawWmp12(look, 640, 360, 1, nullptr, nullptr);
        look.position = 0.9;
        rf::Wmp12Spots sp;
        const rf::Image late = rf::DrawWmp12(look, 640, 360, 1, nullptr, &sp);
        Check(BlueIn(late, sp.seek) > BlueIn(early, sp.seek) * 4, "seek bar fills with the position");
        look.playing = true;
        const rf::Image pause = rf::DrawWmp12(look, 640, 360, 1, nullptr, nullptr);
        int diff = 0;
        for (int y = sp.play.y; y < sp.play.Bottom(); ++y)
        {
            for (int x = sp.play.x; x < sp.play.Right(); ++x)
            {
                diff += pause.Get(x, y) != late.Get(x, y);
            }
        }
        Check(diff > 30, "round button shows pause while playing");
        rf::Wmp12Spots narrow;
        const rf::Image thin = rf::DrawWmp12(look, rf::Wmp12MinWidth, 400, 1, nullptr, &narrow);
        Check(narrow.play.x > 0 && narrow.play.Right() < thin.width && narrow.seek.Right() < thin.width, "narrow window: buttons still inside");
        rftest::WritePpm(out + "/skin_wmp12_narrow.ppm", thin);
    }

    void Video(rftest::TestHost& host, const std::string& out)
    {
        rf::VideoTarget plain;
        rf::VideoTarget seven;
        seven.skin = 1;
        const rf::VideoPlan p0 = rf::PlanVideo(Frame(), 640, 360, plain);
        const rf::VideoPlan p1 = rf::PlanVideo(Frame(), 640, 360, seven);
        int cw = 0;
        int ch = 0;
        int cx = 0;
        int cy = 0;
        rf::Wmp12Chrome(0, &cw, &ch, &cx, &cy);
        Check(p0.skin == 0 && p1.skin == 1 && p1.windowW == (p1.clientW + cw) * p1.scale && p1.windowH == (p1.clientH + ch) * p1.scale && p1.outW % 2 == 0 &&
                  p1.outH % 2 == 0,
              "video plan: media player chrome around the video");
        rf::VideoTarget tall = seven;
        tall.square = true;
        tall.canvasW = 1080;
        tall.canvasH = 1920;
        const rf::VideoPlan pt = rf::PlanVideo(Frame(), 720, 1280, tall);
        Check(pt.clientW >= rf::Wmp12MinWidth && pt.windowH <= 1920 && pt.windowW <= 1080, "vertical video: player not narrower than its buttons");
        rf::VideoTarget sq = seven;
        sq.square = true;
        const rf::VideoPlan ps = rf::PlanVideo(Frame(), 640, 360, sq);
        rf::PlayerComposer composer(Frame(), rf::FrameVars{L"Alarm01.mp4", 640, 360, L""}, ps, rf::CanvasBackground(rf::Image(), rf::Rgb(0x30, 0x60, 0x30), 1080, 1080));
        composer.SetEffects(rf::EffectSettings(), 10.0);
        const rf::Image clip(ps.videoW, ps.videoH, rf::Rgb(0xE0, 0x20, 0x20));
        const rf::Image f = composer.Compose(clip, 0.25, L"00:02 / 00:10");
        const int vx = ps.windowX + (cx + ps.clientW / 2) * ps.scale;
        const int vy = ps.windowY + (cy + ps.clientH / 2) * ps.scale;
        Check(f.width == 1080 && f.height == 1080 && rf::Opaque(f.Get(vx, vy)) == rf::Rgb(0xE0, 0x20, 0x20) && rf::Opaque(f.Get(10, 10)) == rf::Rgb(0x30, 0x60, 0x30) &&
                  rf::Blue(f.Get(ps.windowX + ps.windowW / 2, ps.windowY + 4 * ps.scale)) > 150,
              "video frame: Windows 7 player with the clip on the background");
        rftest::WritePpm(out + "/skin_video_frame.ppm", f);
        rf::EffectSettings fx;
        fx.cursor = 1;
        fx.cursorPlace = rf::LiveCursor;
        rf::PlayerComposer live(Frame(), rf::FrameVars{L"Alarm01.mp4", 640, 360, L""}, ps, rf::Image());
        live.SetEffects(fx, 10.0);
        const rf::Image clicked = live.Compose(clip, 0.08, L"00:00 / 00:10", 2.0, 0, true);
        rftest::WritePpm(out + "/skin_video_cursor.ppm", clicked);
        Check(clicked.width == 1080, "live cursor works on the media player");

        if (host.ffmpeg.empty())
        {
            return;
        }
        const std::wstring dir = rf::FromUtf8(out);
        const std::wstring src = dir + L"/skin_clip.mp4";
        auto p = host.StartProcess(host.ffmpeg, {L"-v", L"error", L"-y", L"-f", L"lavfi", L"-i", L"testsrc=size=320x180:rate=25", L"-f", L"lavfi", L"-i",
                                                L"sine=frequency=440:sample_rate=44100", L"-t", L"1", L"-c:v", L"libx264", L"-pix_fmt", L"yuv420p", L"-c:a", L"aac",
                                                L"-shortest", src},
                                    false, false);
        int code = -1;
        Check(p && p->Finish(code, []() { return true; }) && code == 0, "make test clip");
        rf::ExportJob job;
        job.source = src;
        job.output = dir + L"/skin_out.mp4";
        job.duration = 1.0;
        job.fps = 25.0;
        job.audio = true;
        job.frame = Frame();
        job.vars = rf::FrameVars{L"skin_clip.mp4", 320, 180, L""};
        rf::VideoTarget modern;
        modern.skin = 2;
        job.plan = rf::PlanVideo(job.frame, 320, 180, modern);
        std::wstring err;
        rf::MediaInfo m;
        Check(rf::ExportVideo(host, job, [](int, int) { return true; }, err) == rf::ExportResult::Done && rf::ProbeMedia(host, job.output, m, err) &&
                  m.width == job.plan.outW && m.height == job.plan.outH && m.hasAudio && std::fabs(m.duration - 1.0) < 0.15,
              "MP4 in the Windows 10/11 media player");
    }

    void Music(const std::string& out)
    {
        rf::FrameSettings f = Frame();
        f.player = false;
        rf::MusicShape shape;
        shape.skin = rf::PlayerSkin::Wmp12;
        const rf::MusicPlan plan = rf::PlanMusic(f, 1080, 1080, true, shape);
        int cw = 0;
        int ch = 0;
        int cx = 0;
        int cy = 0;
        rf::Wmp12Chrome(0, &cw, &ch, &cx, &cy);
        rf::MusicShape modern = shape;
        modern.options.wmp12Modern = true;
        const rf::MusicPlan pm = rf::PlanMusic(f, 1080, 1080, true, modern);
        Check(plan.clientW == rf::Wmp12MusicW + cw && plan.clientH == rf::Wmp12MusicH + ch && plan.scale == 2 && pm.clientW < plan.clientW,
              "music plan: media player 12 window, 2x in 1080");
        const rf::Image cover(50, 50, rf::Rgb(0xF0, 0xA0, 0x10));
        rf::MusicComposer composer(f, rf::FrameVars{L"песня.mp3", 0, 0, L""}, plan, rf::Image(), L"Кино", L"Группа крови", cover);
        rf::MusicFrame mf = rf::MusicFrameAt(10.0, 60.0, 10.0, false, false, {});
        const rf::Image img = composer.Compose(mf);
        const int midX = plan.windowX + (cx + rf::Wmp12MusicW / 2) * plan.scale;
        const int midY = plan.windowY + (cy + rf::Wmp12MusicH / 2 - 22) * plan.scale;
        Check(img.width == 1080 && rf::Opaque(img.Get(midX, midY)) == rf::Rgb(0xF0, 0xA0, 0x10), "music: album art in the middle of the player, sharp");
        rftest::WritePpm(out + "/skin_music.ppm", img);
        rf::MusicShape bars = shape;
        bars.options.wmp12Bars = true;
        const rf::MusicPlan pb = rf::PlanMusic(f, 1080, 1080, true, bars);
        rf::MusicComposer bc(f, rf::FrameVars{L"песня.mp3", 0, 0, L""}, pb, rf::Image(), L"Кино", L"Группа крови", cover);
        for (int b = 0; b < rf::SpectrumBands; ++b)
        {
            mf.bars[b] = 0.7f;
        }
        const rf::Image barred = bc.Compose(mf);
        Check(rf::Opaque(barred.Get(midX, midY)) != rf::Rgb(0xF0, 0xA0, 0x10) && BlueIn(barred, rf::Rect{plan.windowX, plan.windowY, plan.windowW, plan.windowH}) > 20000,
              "music: bars instead of the album art");
        rf::Rect play;
        rf::Rect seek;
        rf::MusicHotspots(f, plan, false, &play, &seek);
        Check(play.w > 30 && seek.w > 300, "music: buttons for the live cursor");
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
    rftest::TestHost host;
    if (argc >= 3)
    {
        host.ffmpeg = rf::FromUtf8(argv[2]);
    }
    Smooth();
    Layout(out);
    Video(host, out);
    Music(out);
    {
        rf::AppSettings s = rf::DefaultSettings();
        s.video.skin = 2;
        s.music.player = 3;
        s.music.wmp12Title = false;
        s.music.wmp12Bars = true;
        s.music.wmp12Modern = true;
        rf::AppSettings back = rf::DefaultSettings();
        rf::ParseSettings(rf::SerializeSettings(s), back);
        Check(back.video.skin == 2 && back.music.player == 3 && !back.music.wmp12Title && back.music.wmp12Bars && back.music.wmp12Modern && rf::DefaultSettings().video.skin == 0,
              "player choices round trip, plain window by default");
    }
    {
        rf::MainView main(host);
        main.SetMode(rf::VideoMode);
        main.MutableSettings().video.skin = 1;
        main.Refresh();
        rf::Image shot;
        main.root.Paint(shot);
        rftest::WritePpm(out + "/ui_video_skin.ppm", shot);
        Check(main.Settings().video.skin == 1, "video tab keeps the player choice");
    }
    std::printf(failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 2;
}
