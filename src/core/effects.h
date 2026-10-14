#pragma once

#include "frame.h"

#include <string>
#include <vector>

namespace rf
{
    struct EffectSettings
    {
        bool intro = false;
        int cursor = 0;
        int cursorPlace = 0;
        int popups = 0;
        int popupIcon = 0;
        int popupButtons = 0;
        std::wstring popupTitle = L"Ошибка";
        std::wstring popupText = L"Программа выполнила недопустимую операцию\nи будет закрыта.";
        int screen = 0;
        int palette = 0;
        int motion = 0;
        int outro = 0;
        bool desktop = false;
        bool desktopIcons = false;
        std::wstring clock;
    };

    constexpr double IntroSeconds = 0.6;
    constexpr double OutroSeconds = 1.0;
    constexpr double OutroAnim = 0.5;
    constexpr int LiveCursor = 4;
    constexpr int CascadeCount = 5;
    constexpr double PopupStart = 0.5;
    constexpr double PopupStep = 0.35;

    struct Stage
    {
        Rect window;
        int scale = 1;
        Rect content;
        Rect play;
        Rect seek;
        double duration = 0.0;
    };

    struct Spot
    {
        int x = 0;
        int y = 0;
    };

    bool HasOverlays(const EffectSettings& e);
    Rect ScaleRect(const Rect& r, int scale, int ox, int oy);
    Spot WindowMotion(const EffectSettings& e, double t, int seed, int scale, const Rect& window, const Rect& area);
    Image RenderPopup(const EffectSettings& e, const FrameSettings& frame, bool active, Rect* firstButton);
    void ApplyScreen(Image& img, int kind, int seed, int scale);
    void ApplyPalette(Image& img, int kind, int scale);

    class EffectsLayer
    {
    public:
        EffectsLayer() = default;
        EffectsLayer(const EffectSettings& e, const FrameSettings& frame, const Stage& stage);

        bool Intro() const { return e_.intro; }
        void DrawIntro(Image& out, const Image& background, double t) const;
        void DrawOutro(Image& out, const Image& background, double u, const Rect& taskTarget) const;
        void SetButtons(const Rect& close, const Rect& minimize)
        {
            close_ = close;
            minimize_ = minimize;
        }
        void Shift(int dx, int dy)
        {
            dx_ = dx;
            dy_ = dy;
        }
        void Apply(Image& out, double t, int seed, bool animate, double progress = -1.0) const;
        int VisiblePopups(double t, bool animate) const;
        std::vector<Rect> PopupRects(int width, int height) const;
        Spot CursorAt(double t, double progress, int width, int height, int* closed) const;

    private:
        EffectSettings e_;
        FrameSettings frame_;
        Stage stage_;
        Image popupActive_;
        Image popupInactive_;
        int popupCount_ = 0;
        Rect popupButton_;
        Rect close_;
        Rect minimize_;
        int dx_ = 0;
        int dy_ = 0;

        int LiveShown(double t) const;
        double LiveClosingStart() const;
        void DrawCursor(Image& out, int px, int py) const;
    };
}
