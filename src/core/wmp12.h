#pragma once

#include "paint.h"

#include <string>

namespace rf
{
    struct MusicFrame;

    enum class Wmp12Frame
    {
        Seven = 0,
        Modern = 1
    };

    constexpr int Wmp12MinWidth = 240;
    constexpr int Wmp12MusicW = 480;
    constexpr int Wmp12MusicH = 300;

    struct Wmp12Look
    {
        int frame = 0;
        std::wstring caption = L"Windows Media Player";
        std::wstring title;
        std::wstring elapsed;
        std::wstring total;
        double position = 0.0;
        bool playing = false;
        bool active = true;
    };

    struct Wmp12Spots
    {
        Rect play;
        Rect seek;
        Rect close;
        Rect minimize;
    };

    void Wmp12Chrome(int frame, int* chromeW, int* chromeH, int* contentX, int* contentY);
    Image DrawWmp12(const Wmp12Look& look, int contentW, int contentH, int scale, Rect* content, Wmp12Spots* spots);
    Image DrawWmp12Base(const Wmp12Look& look, int contentW, int contentH, int scale, Rect* content, Wmp12Spots* spots);
    void DrawWmp12Live(Image& img, const Wmp12Look& look, int contentW, int contentH, int scale);
    void DrawWmp12Art(Image& img, const Rect& area, int scale, const MusicFrame& m, const Image& cover, const std::wstring& title, const std::wstring& artist,
                      bool titleOn, bool bars, bool base = true, bool live = true);
    void DrawWmp12Icon(Image& img, double x, double y, double size);
}
