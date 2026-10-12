#pragma once

#include "ui.h"
#include "zip.h"

#include <cstdint>
#include <functional>
#include <memory>
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

    struct FolderItem
    {
        std::wstring name;
        bool folder = false;
        std::uint64_t size = 0;
    };

    class Process
    {
    public:
        virtual ~Process() = default;
        virtual bool Write(const void* data, std::size_t size) = 0;
        virtual void CloseInput() = 0;
        virtual std::size_t Available() = 0;
        virtual bool Read(void* data, std::size_t size) = 0;
        virtual bool Running() = 0;
        virtual bool Finish(int& exitCode, const std::function<bool()>& keepGoing) = 0;
        virtual void Kill() = 0;
        virtual std::string Log() = 0;
    };

    class Host
    {
    public:
        virtual ~Host() = default;

        virtual std::vector<std::wstring> AskOpenImages(const std::wstring& folder) = 0;
        virtual std::wstring AskOpenIcon(const std::wstring& folder) = 0;
        virtual std::wstring AskSavePath(const std::wstring& folder, const std::wstring& suggested, ImageFormat format) = 0;
        virtual std::wstring AskFolder(const std::wstring& folder, const std::wstring& title) = 0;
        virtual std::wstring AskOpenArchive(const std::wstring& folder) = 0;
        virtual std::wstring AskOpenVideo(const std::wstring& folder) = 0;
        virtual std::wstring AskOpenAudio(const std::wstring& folder) = 0;
        virtual std::wstring AskSaveVideo(const std::wstring& folder, const std::wstring& suggested, bool gif) = 0;
        virtual bool AskColor(Color initial, Color& result) = 0;

        virtual bool ReadImage(const std::wstring& path, Image& out, std::wstring& error) = 0;
        virtual bool ReadIcon(const std::wstring& path, Image& out, std::wstring& error) = 0;
        virtual bool WriteImage(const std::wstring& path, const Image& image, ImageFormat format, int quality, std::wstring& error) = 0;
        virtual bool DecodeIcon(const std::vector<unsigned char>& data, Image& out, std::wstring& error) = 0;
        virtual bool ReadFileIcons(const std::wstring& path, std::vector<Image>& out, std::wstring& error) = 0;

        virtual bool ClipboardImage(Image& out) = 0;
        virtual std::vector<std::wstring> ClipboardFiles() = 0;
        virtual bool SetClipboardImage(const Image& image) = 0;

        virtual bool FileExists(const std::wstring& path) = 0;
        virtual std::wstring SettingsFolder() = 0;
        virtual bool ReadText(const std::wstring& path, std::string& utf8) = 0;
        virtual bool WriteText(const std::wstring& path, const std::string& utf8) = 0;
        virtual bool RemoveFile(const std::wstring& path) = 0;
        virtual bool ListFolder(const std::wstring& path, std::vector<FolderItem>& out) = 0;
        virtual std::unique_ptr<ByteSource> OpenFile(const std::wstring& path) = 0;
        virtual std::wstring SevenZipPath() = 0;
        virtual bool ExtractArchive(const std::wstring& archive, const std::wstring& folder, const std::function<bool()>& keepGoing, std::wstring& error) = 0;
        virtual std::wstring NewTempFolder() = 0;
        virtual void RemoveFolder(const std::wstring& path) = 0;
        virtual std::wstring FfmpegPath() = 0;
        virtual std::unique_ptr<Process> StartProcess(const std::wstring& exe, const std::vector<std::wstring>& args, bool input, bool output) = 0;

        virtual int RunModal(View& view) = 0;
        virtual void* OpenModeless(View& view) = 0;
        virtual void CloseModeless(void* handle) = 0;
        virtual void Pump() = 0;
        virtual void Minimize() = 0;
        virtual void Quit() = 0;
        virtual unsigned long long Ticks() = 0;
    };
}
