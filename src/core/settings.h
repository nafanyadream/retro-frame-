#pragma once

#include "effects.h"
#include "frame.h"
#include "host.h"

#include <array>
#include <string>

namespace rf
{
    enum Mode
    {
        PhotoMode = 0,
        VideoMode = 1,
        MusicMode = 2
    };

    struct ModeLook
    {
        int iconIndex = 0;
        bool titleFromName = true;
        std::wstring customTitle;
        WindowKind kind = WindowKind::Normal;
        bool menu = false;
        bool status = false;
        bool toolbar = false;
        bool address = false;
        bool corner = false;
    };

    struct VideoSettings
    {
        int format = 0;
        bool square = false;
        int canvas = 0;
        int gifFps = 15;
        int gifWidth = 0;
        int fps = 0;
    };

    struct MusicSettings
    {
        int format = 0;
        bool square = false;
        int canvas = 0;
        int fade = 2;
        bool spin = true;
        bool meter = true;
        bool equalizer = false;
        int fps = 25;
    };

    struct AppSettings
    {
        FrameSettings frame;
        std::array<ModeLook, 3> looks;
        int mode = PhotoMode;
        int scheme = 0;
        int format = 0;
        std::vector<std::wstring> playerMenu = {L"Файл", L"Вид", L"Воспроизведение", L"Избранное", L"Переход", L"?"};
        std::wstring playerStatus = L"Воспроизведение";
        bool playerTime = true;
        VideoSettings video;
        MusicSettings music;
        EffectSettings effects;
        Color background = Rgb(0x3A, 0x6E, 0xA5);
        bool backgroundPicture = false;
        std::wstring openFolder;
        std::wstring saveFolder;
        std::wstring videoFolder;
        std::wstring musicFolder;
        std::wstring libraryFolder;
        std::vector<std::wstring> library;
        int windowX = -100000;
        int windowY = -100000;
        Image customIcon;
        Image cornerImage;
        Image backgroundImage;
    };

    constexpr int CustomIconIndex = 100;
    constexpr int NoIconIndex = -1;
    constexpr int SquareSizes[] = {1080, 720};

    struct CanvasSize
    {
        int width;
        int height;
    };

    constexpr CanvasSize CanvasSizes[] = {{1080, 1080}, {720, 720}, {1080, 1350}, {1080, 1920}, {720, 1280}, {1920, 1080}};
    constexpr int CanvasCount = int(sizeof(CanvasSizes) / sizeof(CanvasSizes[0]));
    constexpr int GifFpsChoices[] = {10, 12, 15, 20, 25, 30};
    constexpr int GifWidthChoices[] = {0, 800, 640, 480, 320};
    constexpr int FadeChoices[] = {0, 1, 2, 3, 5};
    constexpr int FpsChoices[] = {60, 50, 30, 25, 24, 15};

    std::string SerializeSettings(const AppSettings& s);
    void ParseSettings(const std::string& text, AppSettings& s);
    AppSettings DefaultSettings();
    ModeLook DefaultLook(int mode);
    CanvasSize CanvasAt(int index);
    std::wstring CanvasName(int index);
}
