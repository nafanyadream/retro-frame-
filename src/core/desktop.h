#pragma once

#include "frame.h"

#include <string>

namespace rf
{
    struct DesktopOptions
    {
        bool taskbar = true;
        bool icons = true;
        std::wstring clock;
    };

    int TaskbarHeight(Style style);
    std::wstring ClockText(const std::wstring& custom);
    Rect TaskButtonRect(const FrameSettings& frame, int width, int height);
    Image DesktopLayer(const FrameSettings& frame, const DesktopOptions& d, int width, int height, const std::wstring& task, const Image& taskIcon, bool taskActive = true);
    Image DesktopBackground(const Image& wallpaper, const FrameSettings& frame, const DesktopOptions& d, int outW, int outH, int scale, const std::wstring& task,
                            const Image& taskIcon, bool taskActive = true);
}
