#pragma once

#include "theme.h"

#include <string>
#include <vector>

namespace rf
{
    enum class WindowKind
    {
        Normal = 0,
        Dialog = 1,
        Help = 2,
        Bare = 3
    };

    struct FrameSettings
    {
        Style style = Style::Win98;
        WindowKind kind = WindowKind::Normal;
        bool active = true;
        std::wstring title;
        Image icon;
        FrameColors colors;
        bool menu = false;
        std::vector<std::wstring> menuItems = {L"Файл", L"Правка", L"Вид", L"Справка"};
        bool status = false;
        std::vector<std::wstring> statusFields = {L"Готово", L"{ширина} x {высота}", L"Мой компьютер"};
        bool statusIcon = true;
        bool grip = true;
        bool address = false;
        std::wstring addressText = L"C:\\Мои документы\\Фото";
        bool toolbar = false;
        bool corner = false;
        Image cornerImage;
        bool sunken = false;
        int scale = 0;
        bool player = false;
        double playerPosition = 0.0;
    };

    struct FrameVars
    {
        std::wstring name;
        int width = 0;
        int height = 0;
        std::wstring time;
    };

    constexpr int PlayerPanelHeight = 52;
    constexpr int PlayerMinWidth = 120;
    constexpr int PlayerComfortWidth = 200;

    struct FrameGeometry
    {
        int width = 0;
        int height = 0;
        Rect client;
    };

    std::wstring ExpandVars(const std::wstring& text, const FrameVars& vars);
    FrameGeometry MeasureFrame(const FrameSettings& settings, int contentWidth, int contentHeight);
    Image RenderFrame(const FrameSettings& settings, int contentWidth, int contentHeight, const FrameVars& vars, Rect* client);
    int AutoScale(int contentWidth, int contentHeight);
    Rect CloseButtonRect(const FrameSettings& settings, int frameWidth);
    int EffectiveScale(const FrameSettings& settings, int contentWidth, int contentHeight);
    Image ComposeFramed(const Image& content, const FrameSettings& settings, const FrameVars& vars, int scale, Rect* area = nullptr);
}
