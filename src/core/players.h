#pragma once

#include "media.h"

#include <cstdint>
#include <vector>

namespace rf
{
    struct MusicFrame;

    enum class PlayerSkin
    {
        Classic = 0,
        Winamp = 1,
        MediaPlayer = 2
    };

    constexpr int PlayerSkinCount = 3;
    constexpr int WinampWidth = 275;
    constexpr int WinampPart = 116;

    struct SkinOptions
    {
        bool winampEq = true;
        bool winampList = true;
        bool winampScope = false;
        bool wmpTasks = true;
        bool wmpList = true;
        bool wmpCover = false;
    };

    struct SkinSpots
    {
        Rect play;
        Rect seek;
        Rect close;
        Rect minimize;
    };

    void SkinSize(PlayerSkin skin, const SkinOptions& o, int* width, int* height);
    Image DrawSkin(PlayerSkin skin, const SkinOptions& o, const MusicFrame& m, const std::wstring& artist, const std::wstring& title,
                   const std::vector<std::int16_t>* samples, bool hasCover);
    SkinSpots SkinHotspots(PlayerSkin skin, const SkinOptions& o);
    Rect SkinCoverRect(PlayerSkin skin, const SkinOptions& o);
    std::wstring SkinTitle(PlayerSkin skin, const std::wstring& artist, const std::wstring& title);
    Image SkinIcon(PlayerSkin skin);
    int TinyWidth(const std::wstring& text);
    void TinyText(Painter& p, int x, int y, const std::wstring& text, Color c);
}
