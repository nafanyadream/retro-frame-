#pragma once

#include "frame.h"
#include "host.h"

namespace rf
{
    struct MediaInfo
    {
        double duration = 0.0;
        int width = 0;
        int height = 0;
        double fps = 0.0;
        bool hasVideo = false;
        bool hasAudio = false;
    };

    bool ParseProbe(const std::string& log, MediaInfo& info);
    std::wstring FormatClock(double seconds);
    std::wstring FormatPrecise(double seconds);
    bool ParseClock(const std::wstring& text, double& seconds);
    bool IsVideoFile(const std::wstring& path);
    bool IsAudioFile(const std::wstring& path);
    std::wstring FfmpegMissingText();
    std::string LastLines(const std::string& log, int count);

    bool ProbeMedia(Host& host, const std::wstring& path, MediaInfo& info, std::wstring& error);

    class FrameStream
    {
    public:
        FrameStream(Host& host, const std::wstring& path, double start, double duration, double fps, int width, int height);
        FrameStream(const FrameStream&) = delete;
        FrameStream& operator=(const FrameStream&) = delete;
        ~FrameStream();

        bool Started() const { return proc_ != nullptr; }
        bool Poll(Image& out);
        bool Next(Image& out);
        bool Ended() const { return ended_; }
        void Stop();
        std::string Log();

        const int width;
        const int height;

    private:
        bool Finish(Image& out);

        std::unique_ptr<Process> proc_;
        std::vector<unsigned char> buf_;
        std::size_t have_ = 0;
        bool ended_ = false;
    };

    struct VideoTarget
    {
        bool square = false;
        int squareSize = 1080;
        int width = 0;
    };

    struct VideoPlan
    {
        bool square = false;
        int scale = 1;
        int clientW = 0;
        int clientH = 0;
        int videoW = 0;
        int videoH = 0;
        int windowW = 0;
        int windowH = 0;
        int windowX = 0;
        int windowY = 0;
        int outW = 0;
        int outH = 0;
    };

    VideoPlan PlanVideo(const FrameSettings& frame, int sourceW, int sourceH, const VideoTarget& target);
    Image SquareBackground(const Image& picture, Color color, int size);

    class PlayerComposer
    {
    public:
        PlayerComposer(const FrameSettings& frame, const FrameVars& vars, const VideoPlan& plan, const Image& background);
        const Image& Compose(const Image& video, double position, const std::wstring& time);

    private:
        FrameSettings frame_;
        FrameVars vars_;
        VideoPlan plan_;
        Image background_;
        Image out_;
    };

    struct ExportJob
    {
        std::wstring source;
        std::wstring output;
        bool gif = false;
        int quality = 0;
        double start = 0.0;
        double duration = 0.0;
        double fps = 30.0;
        bool audio = true;
        FrameSettings frame;
        FrameVars vars;
        VideoPlan plan;
        Image background;
    };

    enum class ExportResult
    {
        Done,
        Cancelled,
        Failed
    };

    using ExportStep = std::function<bool(int frame, int total)>;
    ExportResult ExportVideo(Host& host, const ExportJob& job, const ExportStep& step, std::wstring& error);
}
