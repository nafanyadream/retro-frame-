#pragma once

#include "effects.h"
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
        bool hasCover = false;
        int coverW = 0;
        int coverH = 0;
        std::wstring artist;
        std::wstring title;
    };

    bool ParseProbe(const std::string& log, MediaInfo& info);
    std::wstring FormatClock(double seconds);
    std::wstring FormatPrecise(double seconds);
    std::wstring FormatShort(double seconds);
    bool ParseClock(const std::wstring& text, double& seconds);
    bool IsVideoFile(const std::wstring& path);
    bool IsAudioFile(const std::wstring& path);
    std::wstring FfmpegMissingText();
    std::string LastLines(const std::string& log, int count);
    std::string WrapText(const std::string& text, std::size_t width);
    std::wstring ProbeFailure(const std::wstring& ffmpeg, const std::string& log, int code);

    bool ProbeMedia(Host& host, const std::wstring& path, MediaInfo& info, std::wstring& error);
    bool ProbeAudio(Host& host, const std::wstring& path, MediaInfo& info, std::wstring& error);
    bool ReadCover(Host& host, const std::wstring& path, const MediaInfo& info, int maxSide, Image& out);

    constexpr int LevelRate = 25;
    constexpr int AnalysisRate = 16000;
    constexpr int SpectrumBands = 16;

    std::vector<float> LevelsFromSamples(const std::vector<std::int16_t>& samples, int sampleRate);
    std::vector<float> SpectrumFromSamples(const std::vector<std::int16_t>& samples, int sampleRate);
    void SpectrumAt(const std::vector<float>& spectrum, double seconds, float* bars, float* peaks);

    class LevelReader
    {
    public:
        LevelReader(Host& host, const std::wstring& path);
        LevelReader(const LevelReader&) = delete;
        LevelReader& operator=(const LevelReader&) = delete;

        bool Poll();
        bool Wait();
        bool Done() const { return done_; }
        std::vector<float> Levels() const;
        std::vector<float> Spectrum() const;

    private:
        std::vector<std::int16_t> Samples() const;
        void Take(std::size_t n);
        std::unique_ptr<Process> proc_;
        std::vector<unsigned char> bytes_;
        bool done_ = false;
    };

    float MeterAt(const std::vector<float>& levels, double seconds);

    class SoundPreview
    {
    public:
        SoundPreview(Host& host, const std::wstring& path, double start, double duration);
        SoundPreview(const SoundPreview&) = delete;
        SoundPreview& operator=(const SoundPreview&) = delete;

        bool Active() const { return out_ != nullptr; }
        void Prime();
        void Pump(bool release);

        static constexpr int Rate = 44100;
        static constexpr int Channels = 2;

    private:
        void Fill(std::size_t limit);

        std::unique_ptr<Process> proc_;
        std::unique_ptr<AudioOut> out_;
        std::vector<unsigned char> buf_;
        bool ended_ = false;
    };

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
        int canvasW = 1080;
        int canvasH = 1080;
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
    Image CanvasBackground(const Image& picture, Color color, int width, int height);

    class PlayerComposer
    {
    public:
        PlayerComposer(const FrameSettings& frame, const FrameVars& vars, const VideoPlan& plan, const Image& background);
        void SetEffects(const EffectSettings& effects);
        const Image& Compose(const Image& video, double position, const std::wstring& time, double t = 0.0, int seed = 0, bool animate = false);
        const Image& Intro(double t, int seed);

    private:
        FrameSettings frame_;
        FrameVars vars_;
        VideoPlan plan_;
        Image background_;
        Image out_;
        EffectSettings effects_;
        EffectsLayer fx_;
        bool fxOn_ = false;
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
        EffectSettings effects;
    };

    enum class ExportResult
    {
        Done,
        Cancelled,
        Failed
    };

    using ExportStep = std::function<bool(int frame, int total)>;

    struct EncoderSpec
    {
        std::wstring output;
        bool gif = false;
        int quality = 0;
        int width = 0;
        int height = 0;
        double fps = 30.0;
        bool audio = false;
        std::wstring source;
        double start = 0.0;
        double duration = 0.0;
        double fade = 0.0;
        double delay = 0.0;
    };

    using FrameSource = std::function<const Image*(int index)>;
    ExportResult Encode(Host& host, const EncoderSpec& spec, int total, const FrameSource& frames, const ExportStep& step, std::wstring& error);
    ExportResult ExportVideo(Host& host, const ExportJob& job, const ExportStep& step, std::wstring& error);
}
