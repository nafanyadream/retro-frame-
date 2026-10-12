#pragma once

#include "media.h"

namespace rf
{
    constexpr int MusicClientW = 306;
    constexpr int MusicClientH = 214;
    constexpr double MusicFps = 25.0;
    constexpr double DiscSpeed = 120.0;

    struct MusicPlan
    {
        int scale = 1;
        int size = 1080;
        int windowW = 0;
        int windowH = 0;
        int windowX = 0;
        int windowY = 0;
    };

    struct MusicFrame
    {
        double position = 0.0;
        double now = 0.0;
        double length = 0.0;
        double angle = 0.0;
        float level = 0.0f;
    };

    MusicPlan PlanMusic(const FrameSettings& frame, int squareSize);

    class MusicComposer
    {
    public:
        MusicComposer(const FrameSettings& frame, const FrameVars& vars, const MusicPlan& plan, const Image& background, const std::wstring& artist,
                      const std::wstring& title, const Image& cover);
        const Image& Compose(const MusicFrame& f);
        Image Small(const MusicFrame& f, const Image& background);

    private:
        Image Window(const MusicFrame& f, Rect& client) const;

        FrameSettings frame_;
        FrameVars vars_;
        MusicPlan plan_;
        Image background_;
        std::wstring artist_;
        std::wstring title_;
        Image cover_;
        Image coverBig_;
        Image coverSmall_;
        Image out_;
    };

    struct MusicJob
    {
        std::wstring source;
        std::wstring output;
        int quality = 0;
        double start = 0.0;
        double duration = 0.0;
        double fade = 0.0;
        bool spin = true;
        bool meter = true;
        FrameSettings frame;
        FrameVars vars;
        MusicPlan plan;
        Image background;
        std::wstring artist;
        std::wstring title;
        Image cover;
        std::vector<float> levels;
    };

    MusicFrame MusicFrameAt(double now, double length, double absolute, bool spin, bool meter, const std::vector<float>& levels);
    ExportResult ExportMusic(Host& host, const MusicJob& job, const ExportStep& step, std::wstring& error);
}
