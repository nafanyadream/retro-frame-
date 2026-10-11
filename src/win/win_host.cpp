#include "win_host.h"

#include "files.h"
#include "imaging.h"
#include "process.h"

#include "../core/utf.h"

#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <windowsx.h>

#include <utility>

namespace rf::win
{
    namespace
    {
        const wchar_t* const WindowClass = L"RetroFrameWindow";

        void RegisterWindowClass(HINSTANCE instance, WNDPROC proc)
        {
            static bool done = false;
            if (done)
            {
                return;
            }
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = CS_DBLCLKS;
            wc.lpfnWndProc = proc;
            wc.hInstance = instance;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
            wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
            wc.lpszClassName = WindowClass;
            RegisterClassExW(&wc);
            done = true;
        }

        ui::KeyEvent MapKey(WPARAM vk)
        {
            ui::KeyEvent e;
            e.shift = GetKeyState(VK_SHIFT) < 0;
            e.ctrl = GetKeyState(VK_CONTROL) < 0;
            e.alt = GetKeyState(VK_MENU) < 0;
            switch (vk)
            {
            case VK_LEFT: e.key = ui::KeyCode::Left; break;
            case VK_RIGHT: e.key = ui::KeyCode::Right; break;
            case VK_UP: e.key = ui::KeyCode::Up; break;
            case VK_DOWN: e.key = ui::KeyCode::Down; break;
            case VK_HOME: e.key = ui::KeyCode::Home; break;
            case VK_END: e.key = ui::KeyCode::End; break;
            case VK_BACK: e.key = ui::KeyCode::Backspace; break;
            case VK_DELETE: e.key = ui::KeyCode::Delete; break;
            case VK_RETURN: e.key = ui::KeyCode::Enter; break;
            case VK_ESCAPE: e.key = ui::KeyCode::Escape; break;
            case VK_TAB: e.key = ui::KeyCode::Tab; break;
            case VK_PRIOR: e.key = ui::KeyCode::PageUp; break;
            case VK_NEXT: e.key = ui::KeyCode::PageDown; break;
            default:
                if (vk >= 'A' && vk <= 'Z')
                {
                    e.key = ui::KeyCode::Letter;
                    e.letter = wchar_t(vk);
                }
                else
                {
                    e.key = ui::KeyCode::Other;
                }
                break;
            }
            return e;
        }

        RECT WorkAreaFor(HWND nearWindow)
        {
            RECT area{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
            HMONITOR mon = nearWindow != nullptr ? MonitorFromWindow(nearWindow, MONITOR_DEFAULTTONEAREST) : MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            if (mon != nullptr && GetMonitorInfoW(mon, &mi))
            {
                area = mi.rcWork;
            }
            return area;
        }

        int ScaleForScreen(int w, int h, HWND nearWindow)
        {
            HDC screen = GetDC(nullptr);
            const int dpi = GetDeviceCaps(screen, LOGPIXELSX);
            ReleaseDC(nullptr, screen);
            int k = std::max(1, (dpi + 48) / 96);
            const RECT area = WorkAreaFor(nearWindow);
            while (k > 1 && (w * k > area.right - area.left || h * k > area.bottom - area.top))
            {
                --k;
            }
            return k;
        }

        struct FileType
        {
            std::wstring name;
            std::wstring pattern;
        };

        void SetStartFolder(IFileDialog* dlg, const std::wstring& folder)
        {
            if (folder.empty())
            {
                return;
            }
            IShellItem* item = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(folder.c_str(), nullptr, IID_PPV_ARGS(&item))) && item != nullptr)
            {
                dlg->SetFolder(item);
                item->Release();
            }
        }

        std::wstring ItemPath(IShellItem* item)
        {
            PWSTR raw = nullptr;
            std::wstring path;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)) && raw != nullptr)
            {
                path = raw;
                CoTaskMemFree(raw);
            }
            return path;
        }

        std::vector<std::wstring> RunOpen(HWND owner, const std::wstring& folder, const std::wstring& title, const std::vector<FileType>& types, bool multi, bool pickFolder)
        {
            std::vector<std::wstring> result;
            IFileOpenDialog* dlg = nullptr;
            if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))) || dlg == nullptr)
            {
                return result;
            }
            FILEOPENDIALOGOPTIONS options = 0;
            dlg->GetOptions(&options);
            options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
            if (pickFolder)
            {
                options |= FOS_PICKFOLDERS;
            }
            else
            {
                options |= FOS_FILEMUSTEXIST;
            }
            if (multi)
            {
                options |= FOS_ALLOWMULTISELECT;
            }
            dlg->SetOptions(options);
            dlg->SetTitle(title.c_str());
            std::vector<COMDLG_FILTERSPEC> specs;
            for (const auto& t : types)
            {
                specs.push_back(COMDLG_FILTERSPEC{t.name.c_str(), t.pattern.c_str()});
            }
            if (!specs.empty())
            {
                dlg->SetFileTypes(UINT(specs.size()), specs.data());
            }
            SetStartFolder(dlg, folder);
            if (SUCCEEDED(dlg->Show(owner)))
            {
                IShellItemArray* items = nullptr;
                if (SUCCEEDED(dlg->GetResults(&items)) && items != nullptr)
                {
                    DWORD count = 0;
                    items->GetCount(&count);
                    for (DWORD i = 0; i < count; ++i)
                    {
                        IShellItem* item = nullptr;
                        if (SUCCEEDED(items->GetItemAt(i, &item)) && item != nullptr)
                        {
                            const std::wstring p = ItemPath(item);
                            if (!p.empty())
                            {
                                result.push_back(p);
                            }
                            item->Release();
                        }
                    }
                    items->Release();
                }
            }
            dlg->Release();
            return result;
        }

        std::wstring RunSave(HWND owner, const std::wstring& folder, const std::wstring& suggested, const wchar_t* title, const wchar_t* typeName,
                             const wchar_t* pattern, const wchar_t* ext)
        {
            std::wstring result;
            IFileSaveDialog* dlg = nullptr;
            if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))) || dlg == nullptr)
            {
                return result;
            }
            FILEOPENDIALOGOPTIONS options = 0;
            dlg->GetOptions(&options);
            dlg->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST);
            dlg->SetTitle(title);
            COMDLG_FILTERSPEC spec{typeName, pattern};
            dlg->SetFileTypes(1, &spec);
            dlg->SetDefaultExtension(ext);
            dlg->SetFileName(suggested.c_str());
            SetStartFolder(dlg, folder);
            if (SUCCEEDED(dlg->Show(owner)))
            {
                IShellItem* item = nullptr;
                if (SUCCEEDED(dlg->GetResult(&item)) && item != nullptr)
                {
                    result = ItemPath(item);
                    item->Release();
                }
            }
            dlg->Release();
            return result;
        }
    }

    HostWindow::HostWindow(WinHost& host, View& view, HWND owner, bool main)
        : host_(host), view_(view), owner_(owner), main_(main)
    {
    }

    HostWindow::~HostWindow()
    {
        Destroy();
    }

    bool HostWindow::Create(int x, int y)
    {
        RegisterWindowClass(host_.Instance(), &HostWindow::Proc);
        scale_ = ScaleForScreen(view_.root.width, view_.root.height, owner_);
        const DWORD style = WS_POPUP | WS_SYSMENU | WS_CLIPCHILDREN | (main_ ? WS_MINIMIZEBOX : 0);
        const DWORD ex = main_ ? WS_EX_APPWINDOW : 0;
        hwnd_ = CreateWindowExW(ex, WindowClass, view_.title.c_str(), style, x, y, PixelWidth(), PixelHeight(), owner_, nullptr, host_.Instance(), this);
        if (hwnd_ == nullptr)
        {
            return false;
        }
        SetTimer(hwnd_, 1, 40, nullptr);
        host_.Register(this);
        return true;
    }

    void HostWindow::Show(int command)
    {
        ShowWindow(hwnd_, command);
        UpdateWindow(hwnd_);
    }

    void HostWindow::Destroy()
    {
        if (hwnd_ != nullptr && !destroyed_)
        {
            DestroyWindow(hwnd_);
        }
        host_.Unregister(this);
        hwnd_ = nullptr;
    }

    void HostWindow::Sync()
    {
        if (hwnd_ != nullptr && !destroyed_ && view_.root.dirty)
        {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    void HostWindow::PaintNow()
    {
        if (hwnd_ != nullptr && !destroyed_)
        {
            Sync();
            UpdateWindow(hwnd_);
        }
    }

    POINT HostWindow::Logical(LPARAM lParam) const
    {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        POINT p;
        p.x = x >= 0 ? x / scale_ : (x - scale_ + 1) / scale_;
        p.y = y >= 0 ? y / scale_ : (y - scale_ + 1) / scale_;
        return p;
    }

    void HostWindow::Paint(HDC dc)
    {
        view_.root.Paint(back_);
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = back_.width;
        bi.bmiHeader.biHeight = -back_.height;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(dc, 0, 0, back_.width * scale_, back_.height * scale_, 0, 0, back_.width, back_.height, back_.pixels.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
    }

    LRESULT CALLBACK HostWindow::Proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_NCCREATE)
        {
            const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            auto* self = static_cast<HostWindow*>(cs->lpCreateParams);
            self->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        auto* self = reinterpret_cast<HostWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self == nullptr || self->destroyed_)
        {
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        return self->HandleMessage(msg, wParam, lParam);
    }

    LRESULT HostWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        ui::Root& root = view_.root;
        switch (msg)
        {
        case WM_NCHITTEST:
        {
            POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd_, &p);
            if (p.x < 0 || p.y < 0 || p.x >= PixelWidth() || p.y >= PixelHeight())
            {
                return HTNOWHERE;
            }
            return root.InCaption(p.x / scale_, p.y / scale_) ? HTCAPTION : HTCLIENT;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        {
            SetCapture(hwnd_);
            const POINT p = Logical(lParam);
            root.MouseDown(p.x, p.y, (wParam & MK_SHIFT) != 0, (wParam & MK_CONTROL) != 0, msg == WM_LBUTTONDBLCLK ? 2 : 1);
            Sync();
            return 0;
        }
        case WM_LBUTTONUP:
        {
            if (GetCapture() == hwnd_)
            {
                ReleaseCapture();
            }
            const POINT p = Logical(lParam);
            root.MouseUp(p.x, p.y);
            Sync();
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            if (!tracking_)
            {
                TRACKMOUSEEVENT t{};
                t.cbSize = sizeof(t);
                t.dwFlags = TME_LEAVE;
                t.hwndTrack = hwnd_;
                TrackMouseEvent(&t);
                tracking_ = true;
            }
            const POINT p = Logical(lParam);
            root.MouseMove(p.x, p.y);
            Sync();
            return 0;
        }
        case WM_MOUSELEAVE:
            tracking_ = false;
            root.MouseLeave();
            Sync();
            return 0;
        case WM_MOUSEWHEEL:
        {
            POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd_, &p);
            root.Wheel(p.x / scale_, p.y / scale_, GET_WHEEL_DELTA_WPARAM(wParam));
            Sync();
            return 0;
        }
        case WM_KEYDOWN:
            root.Key(MapKey(wParam));
            Sync();
            return 0;
        case WM_CHAR:
            root.Char(wchar_t(wParam));
            Sync();
            return 0;
        case WM_TIMER:
        {
            const unsigned long long ms = GetTickCount64();
            root.Tick(ms);
            view_.OnTick(ms);
            Sync();
            return 0;
        }
        case WM_ACTIVATE:
            root.SetActive(LOWORD(wParam) != WA_INACTIVE);
            Sync();
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT)
            {
                POINT p;
                GetCursorPos(&p);
                ScreenToClient(hwnd_, &p);
                const bool beam = root.WantsIBeam(p.x / scale_, p.y / scale_);
                SetCursor(LoadCursorW(nullptr, beam ? IDC_IBEAM : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_DROPFILES:
        {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            std::vector<std::wstring> files;
            const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; ++i)
            {
                const UINT len = DragQueryFileW(drop, i, nullptr, 0);
                std::wstring path(len, L'\0');
                DragQueryFileW(drop, i, path.data(), len + 1);
                files.push_back(path);
            }
            DragFinish(drop);
            SetForegroundWindow(hwnd_);
            view_.OnDropFiles(files);
            Sync();
            return 0;
        }
        case WM_CLOSE:
            if (view_.OnCloseRequest())
            {
                if (main_)
                {
                    host_.Quit();
                }
                else
                {
                    view_.Close(0);
                }
            }
            return 0;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wParam != 0)
            {
                view_.OnCloseRequest();
            }
            return 0;
        case WM_MOVE:
            if (onMoved)
            {
                RECT r;
                GetWindowRect(hwnd_, &r);
                onMoved(r.left, r.top);
            }
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd_, &ps);
            Paint(dc);
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            KillTimer(hwnd_, 1);
            destroyed_ = true;
            host_.Unregister(this);
            if (main_)
            {
                PostQuitMessage(0);
            }
            return 0;
        default:
            break;
        }
        return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }

    WinHost::WinHost(HINSTANCE instance)
        : instance_(instance)
    {
    }

    HWND WinHost::Owner() const
    {
        for (auto it = windows_.rbegin(); it != windows_.rend(); ++it)
        {
            if ((*it)->Handle() != nullptr)
            {
                return (*it)->Handle();
            }
        }
        return nullptr;
    }

    void WinHost::Register(HostWindow* w)
    {
        windows_.push_back(w);
    }

    void WinHost::Unregister(HostWindow* w)
    {
        for (auto it = windows_.begin(); it != windows_.end(); ++it)
        {
            if (*it == w)
            {
                windows_.erase(it);
                break;
            }
        }
    }

    void WinHost::SyncAll()
    {
        for (HostWindow* w : windows_)
        {
            w->PaintNow();
        }
    }

    void WinHost::PlaceCentered(HostWindow& w, HWND owner)
    {
        RECT area = WorkAreaFor(owner);
        RECT target = area;
        if (owner != nullptr)
        {
            GetWindowRect(owner, &target);
        }
        int x = target.left + ((target.right - target.left) - w.PixelWidth()) / 2;
        int y = target.top + ((target.bottom - target.top) - w.PixelHeight()) / 2;
        x = std::clamp(x, int(area.left), std::max(int(area.left), int(area.right) - w.PixelWidth()));
        y = std::clamp(y, int(area.top), std::max(int(area.top), int(area.bottom) - w.PixelHeight()));
        SetWindowPos(w.Handle(), nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    int WinHost::RunMain(View& view, int show, int x, int y, const std::function<void(int, int)>& onMoved)
    {
        HostWindow w(*this, view, nullptr, true);
        main_ = &w;
        if (!w.Create(CW_USEDEFAULT, CW_USEDEFAULT))
        {
            main_ = nullptr;
            return 1;
        }
        bool placed = false;
        if (x > -100000 && y > -100000)
        {
            POINT corner{x + 40, y + 10};
            if (MonitorFromPoint(corner, MONITOR_DEFAULTTONULL) != nullptr)
            {
                SetWindowPos(w.Handle(), nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                placed = true;
            }
        }
        if (!placed)
        {
            PlaceCentered(w, nullptr);
        }
        w.onMoved = onMoved;
        DragAcceptFiles(w.Handle(), TRUE);
        w.Show(show);
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        main_ = nullptr;
        return 0;
    }

    int WinHost::RunModal(View& view)
    {
        HWND owner = Owner();
        HostWindow w(*this, view, owner, false);
        if (!w.Create(CW_USEDEFAULT, CW_USEDEFAULT))
        {
            return 0;
        }
        PlaceCentered(w, owner);
        if (owner != nullptr)
        {
            EnableWindow(owner, FALSE);
        }
        w.Show(SW_SHOW);
        MSG msg;
        while (!view.closeRequested && !quit_)
        {
            const BOOL r = GetMessageW(&msg, nullptr, 0, 0);
            if (r <= 0)
            {
                quit_ = true;
                PostQuitMessage(int(msg.wParam));
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (owner != nullptr)
        {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
        }
        w.Destroy();
        return view.result;
    }

    void* WinHost::OpenModeless(View& view)
    {
        HWND owner = Owner();
        auto* w = new HostWindow(*this, view, owner, false);
        if (!w->Create(CW_USEDEFAULT, CW_USEDEFAULT))
        {
            delete w;
            return nullptr;
        }
        PlaceCentered(*w, owner);
        if (owner != nullptr)
        {
            EnableWindow(owner, FALSE);
        }
        w->Show(SW_SHOW);
        auto* holder = new std::pair<HostWindow*, HWND>(w, owner);
        return holder;
    }

    void WinHost::CloseModeless(void* handle)
    {
        auto* holder = static_cast<std::pair<HostWindow*, HWND>*>(handle);
        if (holder == nullptr)
        {
            return;
        }
        if (holder->second != nullptr)
        {
            EnableWindow(holder->second, TRUE);
            SetForegroundWindow(holder->second);
        }
        holder->first->Destroy();
        delete holder->first;
        delete holder;
    }

    void WinHost::Pump()
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                quit_ = true;
                PostQuitMessage(int(msg.wParam));
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        SyncAll();
    }

    void WinHost::Minimize()
    {
        if (main_ != nullptr)
        {
            ShowWindow(main_->Handle(), SW_MINIMIZE);
        }
    }

    void WinHost::Quit()
    {
        quit_ = true;
        if (main_ != nullptr && main_->Handle() != nullptr)
        {
            DestroyWindow(main_->Handle());
        }
        else
        {
            PostQuitMessage(0);
        }
    }

    unsigned long long WinHost::Ticks()
    {
        return GetTickCount64();
    }

    std::vector<std::wstring> WinHost::AskOpenImages(const std::wstring& folder)
    {
        return RunOpen(Owner(), folder, L"Открыть фото",
                       {{L"Изображения", L"*.png;*.jpg;*.jpeg;*.jpe;*.bmp;*.gif;*.tif;*.tiff;*.webp;*.heic;*.ico"}, {L"Все файлы", L"*.*"}},
                       true, false);
    }

    std::wstring WinHost::AskOpenIcon(const std::wstring& folder)
    {
        const auto r = RunOpen(Owner(), folder, L"Выбрать картинку",
                               {{L"Иконки, картинки и программы", L"*.ico;*.png;*.bmp;*.gif;*.jpg;*.jpeg;*.exe;*.dll;*.icl;*.cpl"}, {L"Все файлы", L"*.*"}},
                               false, false);
        return r.empty() ? std::wstring() : r.front();
    }

    std::wstring WinHost::AskSavePath(const std::wstring& folder, const std::wstring& suggested, ImageFormat format)
    {
        const bool png = format == ImageFormat::Png;
        return RunSave(Owner(), folder, suggested, L"Сохранить результат", png ? L"Картинка PNG" : L"Картинка JPG", png ? L"*.png" : L"*.jpg;*.jpeg", png ? L"png" : L"jpg");
    }

    std::wstring WinHost::AskSaveVideo(const std::wstring& folder, const std::wstring& suggested, bool gif)
    {
        return RunSave(Owner(), folder, suggested, L"Сохранить видео", gif ? L"Анимация GIF" : L"Видео MP4", gif ? L"*.gif" : L"*.mp4", gif ? L"gif" : L"mp4");
    }

    std::wstring WinHost::AskOpenVideo(const std::wstring& folder)
    {
        const auto r = RunOpen(Owner(), folder, L"Открыть видео",
                               {{L"Видео и GIF", L"*.mp4;*.mov;*.avi;*.mkv;*.webm;*.m4v;*.wmv;*.mpg;*.mpeg;*.3gp;*.flv;*.ts;*.gif"}, {L"Все файлы", L"*.*"}},
                               false, false);
        return r.empty() ? std::wstring() : r.front();
    }

    std::wstring WinHost::AskFolder(const std::wstring& folder, const std::wstring& title)
    {
        const auto r = RunOpen(Owner(), folder, title, {}, false, true);
        return r.empty() ? std::wstring() : r.front();
    }

    std::wstring WinHost::AskOpenArchive(const std::wstring& folder)
    {
        const auto r = RunOpen(Owner(), folder, L"Архив с иконками",
                               {{L"Архивы ZIP, 7z и RAR", L"*.zip;*.7z;*.rar"}, {L"Все файлы", L"*.*"}},
                               false, false);
        return r.empty() ? std::wstring() : r.front();
    }

    bool WinHost::AskColor(Color initial, Color& result)
    {
        static COLORREF custom[16] = {RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
                                      RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
                                      RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
                                      RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255)};
        CHOOSECOLORW cc{};
        cc.lStructSize = sizeof(cc);
        cc.hwndOwner = Owner();
        cc.rgbResult = RGB(Red(initial), Green(initial), Blue(initial));
        cc.lpCustColors = custom;
        cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;
        if (!ChooseColorW(&cc))
        {
            return false;
        }
        result = Rgb(GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));
        return true;
    }

    bool WinHost::ReadImage(const std::wstring& path, Image& out, std::wstring& error)
    {
        return LoadImageFile(path, out, error);
    }

    bool WinHost::ReadIcon(const std::wstring& path, Image& out, std::wstring& error)
    {
        return LoadIconFile(path, out, error);
    }

    bool WinHost::WriteImage(const std::wstring& path, const Image& image, ImageFormat format, int quality, std::wstring& error)
    {
        return SaveImageFile(path, image, format, quality, error);
    }

    bool WinHost::DecodeIcon(const std::vector<unsigned char>& data, Image& out, std::wstring& error)
    {
        return DecodeIconMemory(data, out, error);
    }

    bool WinHost::ReadFileIcons(const std::wstring& path, std::vector<Image>& out, std::wstring& error)
    {
        return LoadFileIcons(path, out, error);
    }

    bool WinHost::ClipboardImage(Image& out)
    {
        return GetClipboardImage(Owner(), out);
    }

    std::vector<std::wstring> WinHost::ClipboardFiles()
    {
        return GetClipboardFiles(Owner());
    }

    bool WinHost::SetClipboardImage(const Image& image)
    {
        return rf::win::SetClipboardImage(Owner(), image);
    }

    std::wstring WinHost::ClipboardText()
    {
        return GetClipboardText(Owner());
    }

    void WinHost::SetClipboardText(const std::wstring& text)
    {
        rf::win::SetClipboardText(Owner(), text);
    }

    bool WinHost::FileExists(const std::wstring& path)
    {
        return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    std::wstring WinHost::SettingsFolder()
    {
        if (!settingsFolder_.empty())
        {
            return settingsFolder_;
        }
        wchar_t buf[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf)))
        {
            return L"";
        }
        std::wstring dir = JoinPath(buf, L"RetroFrame");
        CreateDirectoryW(dir.c_str(), nullptr);
        if (GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            return L"";
        }
        settingsFolder_ = dir;
        return dir;
    }

    bool WinHost::ReadText(const std::wstring& path, std::string& utf8)
    {
        HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        LARGE_INTEGER size{};
        GetFileSizeEx(f, &size);
        if (size.QuadPart > 4 * 1024 * 1024)
        {
            CloseHandle(f);
            return false;
        }
        utf8.assign(std::size_t(size.QuadPart), '\0');
        DWORD read = 0;
        const BOOL ok = utf8.empty() || ReadFile(f, utf8.data(), DWORD(utf8.size()), &read, nullptr);
        CloseHandle(f);
        if (!ok)
        {
            return false;
        }
        utf8.resize(read);
        if (utf8.size() >= 3 && static_cast<unsigned char>(utf8[0]) == 0xEF && static_cast<unsigned char>(utf8[1]) == 0xBB && static_cast<unsigned char>(utf8[2]) == 0xBF)
        {
            utf8.erase(0, 3);
        }
        return true;
    }

    bool WinHost::WriteText(const std::wstring& path, const std::string& utf8)
    {
        const std::wstring tmp = path + L".tmp";
        HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        DWORD written = 0;
        const BOOL ok = WriteFile(f, utf8.data(), DWORD(utf8.size()), &written, nullptr);
        CloseHandle(f);
        if (!ok || written != utf8.size())
        {
            DeleteFileW(tmp.c_str());
            return false;
        }
        return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE;
    }

    bool WinHost::RemoveFile(const std::wstring& path)
    {
        return DeleteFileW(path.c_str()) != FALSE;
    }

    bool WinHost::ListFolder(const std::wstring& path, std::vector<FolderItem>& out)
    {
        return ListFolderItems(path, out);
    }

    std::unique_ptr<ByteSource> WinHost::OpenFile(const std::wstring& path)
    {
        return OpenFileSource(path);
    }

    std::wstring WinHost::SevenZipPath()
    {
        return FindSevenZip();
    }

    bool WinHost::ExtractArchive(const std::wstring& archive, const std::wstring& folder, const std::function<bool()>& keepGoing, std::wstring& error)
    {
        const std::wstring tool = FindSevenZip();
        if (tool.empty())
        {
            error = L"нужна программа 7-Zip";
            return false;
        }
        return RunSevenZip(tool, archive, folder, keepGoing, error);
    }

    std::wstring WinHost::NewTempFolder()
    {
        return CreateTempFolder();
    }

    void WinHost::RemoveFolder(const std::wstring& path)
    {
        DeleteTempFolder(path);
    }

    std::wstring WinHost::FfmpegPath()
    {
        return FindFfmpeg();
    }

    std::unique_ptr<Process> WinHost::StartProcess(const std::wstring& exe, const std::vector<std::wstring>& args, bool input, bool output)
    {
        return StartChild(exe, args, input, output);
    }
}
