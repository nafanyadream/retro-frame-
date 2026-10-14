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
    };

    constexpr double IntroSeconds = 0.6;
    constexpr int CascadeCount = 5;
    constexpr double PopupStart = 0.5;
    constexpr double PopupStep = 0.35;

    struct Stage
    {
        Rect window;
        int scale = 1;
        Rect content;
    };

    bool HasOverlays(const EffectSettings& e);
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
        void Apply(Image& out, double t, int seed, bool animate) const;
        int VisiblePopups(double t, bool animate) const;
        std::vector<Rect> PopupRects(int width, int height) const;

    private:
        EffectSettings e_;
        FrameSettings frame_;
        Stage stage_;
        Image popupActive_;
        Image popupInactive_;
        int popupCount_ = 0;
        Rect popupButton_;
        Rect close_;
    };
}
