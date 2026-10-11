#include "gdi_text.h"
#include "win_host.h"

#include "../core/mainview.h"

#include <objbase.h>
#include <shellapi.h>

#include <vector>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    SetProcessDPIAware();
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int code = 0;
    {
        rf::win::GdiTextEngine text;
        rf::SetTextEngine(&text);
        rf::win::WinHost host(instance);
        rf::ui::SetPlatform(&host);
        rf::MainView view(host);

        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::vector<std::wstring> files;
        for (int i = 1; argv != nullptr && i < argc; ++i)
        {
            files.push_back(argv[i]);
        }
        if (argv != nullptr)
        {
            LocalFree(argv);
        }
        if (!files.empty())
        {
            view.OnDropFiles(files);
        }

        rf::AppSettings& s = view.MutableSettings();
        code = host.RunMain(view, show, s.windowX, s.windowY, [&s](int x, int y) {
            s.windowX = x;
            s.windowY = y;
        });
        view.SaveSettings();
        rf::ui::SetPlatform(nullptr);
        rf::SetTextEngine(nullptr);
    }
    if (SUCCEEDED(com))
    {
        CoUninitialize();
    }
    return code;
}
