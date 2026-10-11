#pragma once

#include "frame.h"
#include "host.h"

#include <string>

namespace rf
{
    struct AppSettings
    {
        FrameSettings frame;
        int scheme = 0;
        int iconIndex = 0;
        bool titleFromName = true;
        std::wstring customTitle = L"Моё фото";
        int format = 0;
        std::wstring openFolder;
        std::wstring saveFolder;
        int windowX = -100000;
        int windowY = -100000;
        Image customIcon;
        Image cornerImage;
    };

    constexpr int CustomIconIndex = 100;
    constexpr int NoIconIndex = -1;

    std::string SerializeSettings(const AppSettings& s);
    void ParseSettings(const std::string& text, AppSettings& s);
    AppSettings DefaultSettings();
}
