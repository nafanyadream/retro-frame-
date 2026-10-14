#pragma once

#include "effects.h"
#include "media.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace rf
{
    constexpr int NotepadSpeedCount = 4;
    constexpr int NotepadSizeCount = 3;

    struct NotepadOptions
    {
        std::wstring text;
        bool russian = true;
        int font = 0;
        int speed = 1;
        bool typos = false;
        bool caret = true;
        int size = 1;
        double hold = 2.0;
    };

    struct Keystroke
    {
        double t = 0.0;
        wchar_t ch = 0;
        bool erase = false;
    };

    std::vector<Keystroke> NotepadKeys(const NotepadOptions& o);
    double NotepadDuration(const NotepadOptions& o, const std::vector<Keystroke>& keys);
    std::wstring NotepadTextAt(const std::vector<Keystroke>& keys, double t);
    bool NotepadCaretAt(const NotepadOptions& o, const std::vector<Keystroke>& keys, double t);
    void NotepadClient(const NotepadOptions& o, int* width, int* height);
    std::wstring NotepadTitle(const std::wstring& file, bool russian);
    std::vector<std::wstring> NotepadMenu(bool russian, Style style);
    Font NotepadFont(const NotepadOptions& o, Style style);
    void DrawNotepad(Painter& p, const NotepadOptions& o, int width, int height, const std::wstring& shown, bool caretOn, const FrameSettings& frame);

    enum class SetupKind
    {
        Copy = 0,
        Delete = 1,
        Install = 2
    };

    struct SetupOptions
    {
        int kind = 0;
        std::wstring from = L"C:\\Мои документы";
        std::wstring to = L"A:\\";
        std::wstring program = L"Ретро-рамка";
        std::wstring files;
        double seconds = 10.0;
        int pace = 1;
        int ending = 0;
    };

    std::vector<std::wstring> SetupFiles(const SetupOptions& o);
    double SetupProgress(const SetupOptions& o, double t);
    double SetupDuration(const SetupOptions& o);
    void SetupClient(const SetupOptions& o, int* width, int* height);
    std::wstring SetupTitle(const SetupOptions& o);
    EffectSettings SetupEndingPopup(const SetupOptions& o);
    void DrawSetup(Painter& p, const SetupOptions& o, int width, int height, double t, const FrameSettings& frame);

    std::vector<std::int16_t> KeyboardSound(const std::vector<Keystroke>& keys, double duration, int rate);
    void AddChime(std::vector<std::int16_t>& samples, double at, bool error, int rate);

    struct ScenePlan
    {
        bool square = false;
        int scale = 1;
        int clientW = 0;
        int clientH = 0;
        int windowW = 0;
        int windowH = 0;
        int windowX = 0;
        int windowY = 0;
        int outW = 0;
        int outH = 0;
        int reserve = 0;
    };

    ScenePlan PlanScene(const FrameSettings& frame, int clientW, int clientH, int canvasW, int canvasH, bool square, int reserve, int gifWidth);

    using SceneDraw = std::function<void(Painter& p, int width, int height, double t)>;

    class SceneComposer
    {
    public:
        SceneComposer(const FrameSettings& frame, const FrameVars& vars, const ScenePlan& plan, const Image& background, SceneDraw draw);
        void SetEffects(const EffectSettings& effects, double duration);
        void SetOutro(const Image& ending, const Rect& taskTarget);
        void SetPopup(const EffectSettings& popup, double from);
        const Image& Compose(double t, int seed, bool animate);
        const Image& Intro(double t, int seed);
        const Image& Outro(double u, int seed);
        Image Small(double t, const Image& background, Rect* window, Rect* content, Spot shift = Spot{});

    private:
        Image Window(double t, Rect& client) const;
        void DrawPopup(Image& out, double t, int k, const Rect& window) const;

        FrameSettings frame_;
        FrameVars vars_;
        ScenePlan plan_;
        Image background_;
        SceneDraw draw_;
        Image ending_;
        Rect taskTarget_;
        Image out_;
        EffectSettings effects_;
        EffectsLayer fx_;
        bool fxOn_ = false;
        Image popup_;
        double popupFrom_ = -1.0;
    };

    struct SceneJob
    {
        std::wstring output;
        bool gif = false;
        int quality = 0;
        double fps = 25.0;
        double duration = 1.0;
        FrameSettings frame;
        FrameVars vars;
        ScenePlan plan;
        Image background;
        Image ending;
        Rect taskTarget;
        EffectSettings effects;
        EffectSettings popup;
        double popupFrom = -1.0;
        std::wstring audio;
        SceneDraw draw;
    };

    ExportResult ExportScene(Host& host, const SceneJob& job, const ExportStep& step, std::wstring& error);
}
