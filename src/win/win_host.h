#pragma once

#include "../core/host.h"
#include "../core/ui.h"

#include <windows.h>

#include <functional>
#include <vector>

namespace rf::win
{
    class WinHost;

    class HostWindow
    {
    public:
        HostWindow(WinHost& host, View& view, HWND owner, bool main);
        ~HostWindow();
        HostWindow(const HostWindow&) = delete;
        HostWindow& operator=(const HostWindow&) = delete;

        bool Create(int x, int y);
        void Show(int command);
        void Destroy();
        void Sync();
        void PaintNow();

        HWND Handle() const { return hwnd_; }
        View& GetView() { return view_; }
        int Scale() const { return scale_; }
        int PixelWidth() const { return view_.root.width * scale_; }
        int PixelHeight() const { return view_.root.height * scale_; }

        std::function<void(int, int)> onMoved;

    private:
        static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
        void Paint(HDC dc);
        POINT Logical(LPARAM lParam) const;

        WinHost& host_;
        View& view_;
        HWND owner_;
        HWND hwnd_ = nullptr;
        bool main_;
        int scale_ = 1;
        bool tracking_ = false;
        bool destroyed_ = false;
        Image back_;
    };

    class WinHost : public Host, public ui::Platform
    {
    public:
        explicit WinHost(HINSTANCE instance);

        int RunMain(View& view, int show, int x, int y, const std::function<void(int, int)>& onMoved, const std::function<void()>& onShown);

        std::vector<std::wstring> AskOpenImages(const std::wstring& folder) override;
        std::wstring AskOpenIcon(const std::wstring& folder) override;
        std::wstring AskSavePath(const std::wstring& folder, const std::wstring& suggested, ImageFormat format) override;
        std::wstring AskFolder(const std::wstring& folder, const std::wstring& title) override;
        std::wstring AskOpenArchive(const std::wstring& folder) override;
        std::wstring AskOpenVideo(const std::wstring& folder) override;
        std::wstring AskOpenAudio(const std::wstring& folder) override;
        std::wstring AskSaveVideo(const std::wstring& folder, const std::wstring& suggested, bool gif) override;
        bool AskColor(Color initial, Color& result) override;

        bool ReadImage(const std::wstring& path, Image& out, std::wstring& error) override;
        bool ReadIcon(const std::wstring& path, Image& out, std::wstring& error) override;
        bool WriteImage(const std::wstring& path, const Image& image, ImageFormat format, int quality, std::wstring& error) override;
        bool DecodeIcon(const std::vector<unsigned char>& data, Image& out, std::wstring& error) override;
        bool ReadFileIcons(const std::wstring& path, std::vector<Image>& out, std::wstring& error) override;

        bool ClipboardImage(Image& out) override;
        std::vector<std::wstring> ClipboardFiles() override;
        bool SetClipboardImage(const Image& image) override;

        bool FileExists(const std::wstring& path) override;
        std::wstring SettingsFolder() override;
        bool ReadText(const std::wstring& path, std::string& utf8) override;
        bool WriteText(const std::wstring& path, const std::string& utf8) override;
        bool RemoveFile(const std::wstring& path) override;
        bool ListFolder(const std::wstring& path, std::vector<FolderItem>& out) override;
        std::unique_ptr<ByteSource> OpenFile(const std::wstring& path) override;
        std::wstring SevenZipPath() override;
        bool ExtractArchive(const std::wstring& archive, const std::wstring& folder, const std::function<bool()>& keepGoing, std::wstring& error) override;
        std::wstring NewTempFolder() override;
        void RemoveFolder(const std::wstring& path) override;
        std::wstring FfmpegPath() override;
        std::unique_ptr<Process> StartProcess(const std::wstring& exe, const std::vector<std::wstring>& args, bool input, bool output) override;

        int RunModal(View& view) override;
        void* OpenModeless(View& view) override;
        void CloseModeless(void* handle) override;
        void Pump() override;
        void Minimize() override;
        void Quit() override;
        unsigned long long Ticks() override;

        std::wstring ClipboardText() override;
        void SetClipboardText(const std::wstring& text) override;

        HINSTANCE Instance() const { return instance_; }
        HWND Owner() const;
        void Register(HostWindow* w);
        void Unregister(HostWindow* w);
        void SyncAll();
        bool QuitRequested() const { return quit_; }

    private:
        void PlaceCentered(HostWindow& w, HWND owner);

        HINSTANCE instance_;
        std::vector<HostWindow*> windows_;
        HostWindow* main_ = nullptr;
        bool quit_ = false;
        std::wstring settingsFolder_;
    };
}
