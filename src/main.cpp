#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <string>

namespace
{
    constexpr COLORREF Face = RGB(0xC0, 0xC0, 0xC0);
    constexpr COLORREF Light = RGB(0xDF, 0xDF, 0xDF);
    constexpr COLORREF Hilite = RGB(0xFF, 0xFF, 0xFF);
    constexpr COLORREF Shadow = RGB(0x80, 0x80, 0x80);
    constexpr COLORREF Dark = RGB(0x00, 0x00, 0x00);
    constexpr COLORREF TitleFrom = RGB(0x00, 0x00, 0x80);
    constexpr COLORREF TitleTo = RGB(0x10, 0x84, 0xD0);

    constexpr int LogicalWidth = 340;
    constexpr int LogicalHeight = 132;
    constexpr int Border = 3;
    constexpr int TitleHeight = 18;

    const char* const CloseGlyph[] = {"xx....xx", ".xx..xx.", "..xxxx..", "...xx...", "..xxxx..", ".xx..xx.", "xx....xx"};
    const char* const MinimizeGlyph[] = {"xxxxxx", "xxxxxx"};

    enum class Part
    {
        None,
        Minimize,
        Close,
        Ok
    };

    struct AppState
    {
        int scale = 1;
        HFONT font = nullptr;
        HFONT bold = nullptr;
        RECT minimizeRect{};
        RECT closeRect{};
        RECT okRect{};
        Part pressed = Part::None;
        std::wstring faceName;
    };

    AppState app;

    RECT MakeRect(int x, int y, int w, int h)
    {
        return RECT{x, y, x + w, y + h};
    }

    void Fill(HDC dc, int x, int y, int w, int h, COLORREF color)
    {
        if (w <= 0 || h <= 0)
        {
            return;
        }
        SetDCBrushColor(dc, color);
        RECT r = MakeRect(x, y, w, h);
        FillRect(dc, &r, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }

    void Edge(HDC dc, int x, int y, int w, int h, COLORREF topLeft, COLORREF bottomRight)
    {
        Fill(dc, x, y, w - 1, 1, topLeft);
        Fill(dc, x, y, 1, h - 1, topLeft);
        Fill(dc, x + w - 1, y, 1, h, bottomRight);
        Fill(dc, x, y + h - 1, w, 1, bottomRight);
    }

    void Button(HDC dc, const RECT& r, bool down)
    {
        const int w = r.right - r.left;
        const int h = r.bottom - r.top;
        Fill(dc, r.left, r.top, w, h, Face);
        if (down)
        {
            Edge(dc, r.left, r.top, w, h, Dark, Hilite);
            Edge(dc, r.left + 1, r.top + 1, w - 2, h - 2, Shadow, Light);
        }
        else
        {
            Edge(dc, r.left, r.top, w, h, Hilite, Dark);
            Edge(dc, r.left + 1, r.top + 1, w - 2, h - 2, Light, Shadow);
        }
    }

    void Glyph(HDC dc, int x, int y, const char* const* rows, int count, COLORREF color)
    {
        for (int j = 0; j < count; ++j)
        {
            for (int i = 0; rows[j][i] != '\0'; ++i)
            {
                if (rows[j][i] == 'x')
                {
                    Fill(dc, x + i, y + j, 1, 1, color);
                }
            }
        }
    }

    COLORREF Mix(COLORREF a, COLORREF b, int i, int n)
    {
        const int d = n > 1 ? n - 1 : 1;
        const auto channel = [&](int ca, int cb) { return ca + (cb - ca) * i / d; };
        return RGB(channel(GetRValue(a), GetRValue(b)), channel(GetGValue(a), GetGValue(b)), channel(GetBValue(a), GetBValue(b)));
    }

    void Text(HDC dc, HFONT font, const std::wstring& s, RECT r, COLORREF color, UINT flags)
    {
        HGDIOBJ old = SelectObject(dc, font);
        SetTextColor(dc, color);
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, s.c_str(), static_cast<int>(s.size()), &r, flags | DT_NOPREFIX);
        SelectObject(dc, old);
    }

    HFONT MakeFont(int weight)
    {
        return CreateFontW(-11, 0, 0, 0, weight, FALSE, FALSE, FALSE, RUSSIAN_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"MS Sans Serif");
    }

    void Layout()
    {
        const int titleRight = LogicalWidth - Border;
        app.closeRect = MakeRect(titleRight - 2 - 16, Border + 2, 16, 14);
        app.minimizeRect = MakeRect(app.closeRect.left - 2 - 16, Border + 2, 16, 14);
        app.okRect = MakeRect(LogicalWidth - Border - 10 - 75, LogicalHeight - Border - 10 - 23, 75, 23);
    }

    void Render(HDC dc)
    {
        const int w = LogicalWidth;
        const int h = LogicalHeight;
        Fill(dc, 0, 0, w, h, Face);
        Edge(dc, 0, 0, w, h, Light, Dark);
        Edge(dc, 1, 1, w - 2, h - 2, Hilite, Shadow);

        const int titleWidth = w - Border * 2;
        for (int i = 0; i < titleWidth; ++i)
        {
            Fill(dc, Border + i, Border, 1, TitleHeight, Mix(TitleFrom, TitleTo, i, titleWidth));
        }
        Text(dc, app.bold, L"Ретро-рамка — проверка сборки",
             RECT{Border + 3, Border, app.minimizeRect.left - 4, Border + TitleHeight}, Hilite,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        const bool minDown = app.pressed == Part::Minimize;
        Button(dc, app.minimizeRect, minDown);
        Glyph(dc, app.minimizeRect.left + 4 + (minDown ? 1 : 0), app.minimizeRect.top + 9 + (minDown ? 1 : 0), MinimizeGlyph, 2, Dark);

        const bool closeDown = app.pressed == Part::Close;
        Button(dc, app.closeRect, closeDown);
        Glyph(dc, app.closeRect.left + 4 + (closeDown ? 1 : 0), app.closeRect.top + 3 + (closeDown ? 1 : 0), CloseGlyph, 7, Dark);

        const int left = Border + 12;
        const int top = Border + TitleHeight + 12;
        Text(dc, app.bold, L"Сборка работает!", RECT{left, top, w - left, top + 14}, Dark, DT_LEFT | DT_SINGLELINE);
        Text(dc, app.font, L"Программа написана на C++ и Win32, без внешних библиотек.",
             RECT{left, top + 22, w - left, top + 36}, Dark, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        const std::wstring info = L"Шрифт: " + app.faceName + L". Масштаб экрана: " + std::to_wstring(app.scale) + L"x.";
        Text(dc, app.font, info, RECT{left, top + 38, w - left, top + 52}, Dark, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        const RECT& ok = app.okRect;
        Fill(dc, ok.left, ok.top, ok.right - ok.left, 1, Dark);
        Fill(dc, ok.left, ok.top, 1, ok.bottom - ok.top, Dark);
        Fill(dc, ok.right - 1, ok.top, 1, ok.bottom - ok.top, Dark);
        Fill(dc, ok.left, ok.bottom - 1, ok.right - ok.left, 1, Dark);
        const RECT inner{ok.left + 1, ok.top + 1, ok.right - 1, ok.bottom - 1};
        const bool okDown = app.pressed == Part::Ok;
        Button(dc, inner, okDown);
        RECT label = inner;
        if (okDown)
        {
            OffsetRect(&label, 1, 1);
        }
        Text(dc, app.font, L"OK", label, Dark, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    Part HitPart(int x, int y)
    {
        const POINT p{x / app.scale, y / app.scale};
        if (PtInRect(&app.closeRect, p))
        {
            return Part::Close;
        }
        if (PtInRect(&app.minimizeRect, p))
        {
            return Part::Minimize;
        }
        if (PtInRect(&app.okRect, p))
        {
            return Part::Ok;
        }
        return Part::None;
    }

    void Paint(HWND hwnd)
    {
        PAINTSTRUCT ps;
        HDC target = BeginPaint(hwnd, &ps);
        HDC memory = CreateCompatibleDC(target);
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = LogicalWidth;
        bi.bmiHeader.biHeight = -LogicalHeight;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(memory, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HGDIOBJ old = SelectObject(memory, bitmap);
        Render(memory);
        SetStretchBltMode(target, COLORONCOLOR);
        StretchBlt(target, 0, 0, LogicalWidth * app.scale, LogicalHeight * app.scale, memory, 0, 0, LogicalWidth, LogicalHeight, SRCCOPY);
        SelectObject(memory, old);
        DeleteObject(bitmap);
        DeleteDC(memory);
        EndPaint(hwnd, &ps);
    }

    void Activate(HWND hwnd, Part part)
    {
        if (part == Part::Close || part == Part::Ok)
        {
            DestroyWindow(hwnd);
        }
        else if (part == Part::Minimize)
        {
            ShowWindow(hwnd, SW_MINIMIZE);
        }
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_NCHITTEST:
        {
            POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &p);
            const int ly = p.y / app.scale;
            if (HitPart(p.x, p.y) == Part::None && ly >= Border && ly < Border + TitleHeight)
            {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_LBUTTONDOWN:
            app.pressed = HitPart(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (app.pressed != Part::None)
            {
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
        {
            const Part was = app.pressed;
            app.pressed = Part::None;
            ReleaseCapture();
            InvalidateRect(hwnd, nullptr, FALSE);
            if (was != Part::None && HitPart(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)) == was)
            {
                Activate(hwnd, was);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_RETURN || wParam == VK_ESCAPE)
            {
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            Paint(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    SetProcessDPIAware();
    HDC screen = GetDC(nullptr);
    const int dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);
    app.scale = std::max(1, (dpi + 48) / 96);

    app.font = MakeFont(FW_NORMAL);
    app.bold = MakeFont(FW_BOLD);
    HDC probe = CreateCompatibleDC(nullptr);
    HGDIOBJ oldFont = SelectObject(probe, app.font);
    wchar_t face[LF_FACESIZE]{};
    GetTextFaceW(probe, LF_FACESIZE, face);
    SelectObject(probe, oldFont);
    DeleteDC(probe);
    app.faceName = face;
    Layout();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"RetroFrameTest";
    RegisterClassExW(&wc);

    const int width = LogicalWidth * app.scale;
    const int height = LogicalHeight * app.scale;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"Ретро-рамка", WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX,
                                x, y, width, height, nullptr, nullptr, instance, nullptr);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(app.font);
    DeleteObject(app.bold);
    return static_cast<int>(msg.wParam);
}
