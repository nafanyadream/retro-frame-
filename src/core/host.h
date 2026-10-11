#pragma once

#include "ui.h"

#include <string>
#include <vector>

namespace rf
{
    class View
    {
    public:
        View(int width, int height) : root(width, height) {}
        virtual ~View() = default;

        ui::Root root;
        std::wstring title;
        Image icon;
        bool closeRequested = false;
        int result = 0;

        void Close(int value)
        {
            result = value;
            closeRequested = true;
        }

        virtual void OnTick(unsigned long long) {}
        virtual bool OnCloseRequest() { return true; }
        virtual void OnDropFiles(const std::vector<std::wstring>&) {}
    };

    enum class ImageFormat
    {
        Png = 0,
        Jpeg = 1
    };

    enum class MessageKind
    {
        Info,
        Warning
    };

    class Host
    {
    public:
        virtual ~Host() = default;

        virtual std::vector<std::wstring> AskOpenImages(const std::wstring& folder) = 0;
        virtual std::wstring AskOpenIcon(const std::wstring& folder) = 0;
        virtual std::wstring AskSavePath(const std::wstring& folder, const std::wstring& suggested, ImageFormat format) = 0;
        virtual std::wstring AskFolder(const std::wstring& folder) = 0;
        virtual bool AskColor(Color initial, Color& result) = 0;

        virtual bool ReadImage(const std::wstring& path, Image& out, std::wstring& error) = 0;
        virtual bool ReadIcon(const std::wstring& path, Image& out, std::wstring& error) = 0;
        virtual bool WriteImage(const std::wstring& path, const Image& image, ImageFormat format, int quality, std::wstring& error) = 0;

        virtual bool ClipboardImage(Image& out) = 0;
        virtual std::vector<std::wstring> ClipboardFiles() = 0;
        virtual bool SetClipboardImage(const Image& image) = 0;

        virtual bool FileExists(const std::wstring& path) = 0;
        virtual std::wstring SettingsFolder() = 0;
        virtual bool ReadText(const std::wstring& path, std::string& utf8) = 0;
        virtual bool WriteText(const std::wstring& path, const std::string& utf8) = 0;
        virtual bool RemoveFile(const std::wstring& path) = 0;

        virtual int RunModal(View& view) = 0;
        virtual void* OpenModeless(View& view) = 0;
        virtual void CloseModeless(void* handle) = 0;
        virtual void Pump() = 0;
        virtual void Minimize() = 0;
        virtual void Quit() = 0;
        virtual unsigned long long Ticks() = 0;
    };
}
