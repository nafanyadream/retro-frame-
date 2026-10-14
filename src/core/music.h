#pragma once

#include "lyrics.h"
#include "media.h"

namespace rf
{
    constexpr int MusicClientW = 306;
    constexpr int MusicClientH = 214;
    constexpr double MusicFps = 25.0;
    constexpr double DiscSpeed = 120.0;
    constexpr int LyricsStripH = 22;
    constexpr int RecorderClientW = 260;
    constexpr int RecorderClientH = 102;
    constexpr double RecorderWaveSeconds = 0.1;

    std::wstring RecorderSeconds(double seconds, bool russian);
    std::wstring RecorderTitle(const std::wstring& base, bool russian);
    std::vector<std::wstring> RecorderMenu(bool russian);

    struct MusicPlan
    {
        bool square = true;
        int scale = 1;
        int clientW = MusicClientW;
        int clientH = MusicClientH;
        int windowW = 0;
        int windowH = 0;
        int windowX = 0;
        int windowY = 0;
        int outW = 1080;
        int outH = 1080;
        int reserve = 0;
    };

    struct MusicFrame
    {
        double position = 0.0;
        double now = 0.0;
        double length = 0.0;
        double absolute = 0.0;
        double wave = -1.0;
        bool recording = false;
        double angle = 0.0;
        float level = 0.0f;
        bool equalizer = false;
        float bars[SpectrumBands] = {};
        float peaks[SpectrumBands] = {};
        bool lyricsStrip = false;
        std::wstring lyric;
        double lyricAge = 0.0;
    };

    MusicPlan PlanMusic(const FrameSettings& frame, int canvasW, int canvasH, bool square, bool lyrics = false, bool recorder = false, int reserve = 0);
    void MusicHotspots(const FrameSettings& frame, const MusicPlan& plan, bool recorder, Rect* play, Rect* seek);

    class MusicComposer
    {
    public:
        MusicComposer(const FrameSettings& frame, const FrameVars& vars, const MusicPlan& plan, const Image& background, const std::wstring& artist,
                      const std::wstring& title, const Image& cover);
        void SetEffects(const EffectSettings& effects, double duration = 0.0);
        void SetRecorder(bool russian, double songLength, std::shared_ptr<const std::vector<std::int16_t>> samples);
        void SetOutro(const Image& ending, const Rect& taskTarget);
        const Image& Compose(const MusicFrame& f, int seed = 0, bool animate = false);
        const Image& Intro(double t, int seed);
        const Image& Outro(double u, int seed);
        Image Small(const MusicFrame& f, const Image& background, Rect* content = nullptr, Spot shift = Spot{});

    private:
        Image Window(const MusicFrame& f, Rect& client) const;
        void BuildEffects();

        FrameSettings frame_;
        FrameVars vars_;
        MusicPlan plan_;
        Image background_;
        Image ending_;
        Rect taskTarget_;
        std::wstring artist_;
        std::wstring title_;
        Image cover_;
        Image coverBig_;
        Image coverSmall_;
        Image out_;
        EffectSettings effects_;
        EffectsLayer fx_;
        bool fxOn_ = false;
        bool fxSet_ = false;
        double duration_ = 0.0;
        bool recorder_ = false;
        bool russian_ = false;
        double songLength_ = 0.0;
        std::shared_ptr<const std::vector<std::int16_t>> samples_;
    };

    struct MusicJob
    {
        std::wstring source;
        std::wstring output;
        int quality = 0;
        double start = 0.0;
        double duration = 0.0;
        double fade = 0.0;
        double fps = MusicFps;
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
        bool equalizer = false;
        std::vector<float> spectrum;
        std::vector<LyricLine> lyrics;
        bool recorder = false;
        bool recorderRussian = false;
        double songLength = 0.0;
        std::shared_ptr<const std::vector<std::int16_t>> samples;
        EffectSettings effects;
        Image ending;
        Rect taskTarget;
    };

    MusicFrame MusicFrameAt(double now, double length, double absolute, bool spin, bool meter, const std::vector<float>& levels);
    void AddSpectrum(MusicFrame& frame, const std::vector<float>& spectrum, double absolute);
    void AddLyrics(MusicFrame& frame, const std::vector<LyricLine>& lines, double absolute);
    ExportResult ExportMusic(Host& host, const MusicJob& job, const ExportStep& step, std::wstring& error);
}
