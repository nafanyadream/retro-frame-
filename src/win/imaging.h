#pragma once

#include "../core/host.h"

#include <windows.h>

#include <string>
#include <vector>

namespace rf::win
{
    bool LoadImageFile(const std::wstring& path, Image& out, std::wstring& error);
    bool LoadIconFile(const std::wstring& path, Image& out, std::wstring& error);
    bool SaveImageFile(const std::wstring& path, const Image& image, ImageFormat format, int quality, std::wstring& error);

    bool GetClipboardImage(HWND owner, Image& out);
    std::vector<std::wstring> GetClipboardFiles(HWND owner);
    bool SetClipboardImage(HWND owner, const Image& image);
    std::wstring GetClipboardText(HWND owner);
    void SetClipboardText(HWND owner, const std::wstring& text);
}
