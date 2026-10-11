#include "imaging.h"

#include "../core/paint.h"
#include "../core/utf.h"

#include <propvarutil.h>
#include <shellapi.h>
#include <wincodec.h>

#include <cstring>

namespace rf::win
{
    namespace
    {
        template <class T>
        class Com
        {
        public:
            Com() = default;
            ~Com() { Reset(); }
            Com(const Com&) = delete;
            Com& operator=(const Com&) = delete;
            T** Put()
            {
                Reset();
                return &p_;
            }
            T* operator->() const { return p_; }
            T* Get() const { return p_; }
            explicit operator bool() const { return p_ != nullptr; }
            void Reset()
            {
                if (p_ != nullptr)
                {
                    p_->Release();
                    p_ = nullptr;
                }
            }

        private:
            T* p_ = nullptr;
        };

        IWICImagingFactory* Factory()
        {
            static IWICImagingFactory* factory = nullptr;
            if (factory == nullptr)
            {
                CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
            }
            return factory;
        }

        std::wstring HrText(HRESULT hr)
        {
            if (hr == WINCODEC_ERR_COMPONENTNOTFOUND)
            {
                return L"формат не поддерживается";
            }
            if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
            {
                return L"файл не найден";
            }
            if (hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) || hr == E_ACCESSDENIED)
            {
                return L"нет доступа к файлу";
            }
            if (hr == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION))
            {
                return L"файл занят другой программой";
            }
            wchar_t buf[32];
            wsprintfW(buf, L"ошибка 0x%08X", unsigned(hr));
            return buf;
        }

        bool ReadSource(IWICBitmapSource* source, Image& out, std::wstring& error)
        {
            Com<IWICBitmapSource> converted;
            HRESULT hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, source, converted.Put());
            if (FAILED(hr))
            {
                error = HrText(hr);
                return false;
            }
            UINT w = 0;
            UINT h = 0;
            converted->GetSize(&w, &h);
            if (w == 0 || h == 0 || w > 30000 || h > 30000)
            {
                error = L"неподходящий размер картинки";
                return false;
            }
            out = Image(int(w), int(h));
            hr = converted->CopyPixels(nullptr, w * 4, w * h * 4, reinterpret_cast<BYTE*>(out.pixels.data()));
            if (FAILED(hr))
            {
                error = HrText(hr);
                return false;
            }
            return true;
        }

        int Orientation(IWICBitmapFrameDecode* frame)
        {
            Com<IWICMetadataQueryReader> reader;
            if (FAILED(frame->GetMetadataQueryReader(reader.Put())))
            {
                return 1;
            }
            const wchar_t* queries[] = {L"System.Photo.Orientation", L"/app1/ifd/{ushort=274}"};
            for (const wchar_t* q : queries)
            {
                PROPVARIANT pv;
                PropVariantInit(&pv);
                if (SUCCEEDED(reader->GetMetadataByName(q, &pv)))
                {
                    int value = 1;
                    if (pv.vt == VT_UI2)
                    {
                        value = pv.uiVal;
                    }
                    else if (pv.vt == VT_UI4)
                    {
                        value = int(pv.ulVal);
                    }
                    else if (pv.vt == VT_I4)
                    {
                        value = pv.lVal;
                    }
                    PropVariantClear(&pv);
                    if (value >= 1 && value <= 8)
                    {
                        return value;
                    }
                }
            }
            return 1;
        }

        Image ApplyOrientation(const Image& img, int o)
        {
            switch (o)
            {
            case 2:
                return FlipHorizontal(img);
            case 3:
                return Rotate180(img);
            case 4:
                return FlipVertical(img);
            case 5:
                return Rotate270(FlipHorizontal(img));
            case 6:
                return Rotate90(img);
            case 7:
                return Rotate90(FlipHorizontal(img));
            case 8:
                return Rotate270(img);
            default:
                return img;
            }
        }

        bool DecodeFrame(IWICBitmapDecoder* decoder, UINT index, Image& out, std::wstring& error, bool orient)
        {
            Com<IWICBitmapFrameDecode> frame;
            HRESULT hr = decoder->GetFrame(index, frame.Put());
            if (FAILED(hr))
            {
                error = HrText(hr);
                return false;
            }
            if (!ReadSource(frame.Get(), out, error))
            {
                return false;
            }
            if (orient)
            {
                const int o = Orientation(frame.Get());
                if (o != 1)
                {
                    out = ApplyOrientation(out, o);
                }
            }
            return true;
        }

        bool DecodeMemory(const void* data, std::size_t size, Image& out)
        {
            IWICImagingFactory* f = Factory();
            if (f == nullptr || size == 0)
            {
                return false;
            }
            Com<IWICStream> stream;
            if (FAILED(f->CreateStream(stream.Put())))
            {
                return false;
            }
            if (FAILED(stream->InitializeFromMemory(static_cast<BYTE*>(const_cast<void*>(data)), DWORD(size))))
            {
                return false;
            }
            Com<IWICBitmapDecoder> decoder;
            if (FAILED(f->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.Put())))
            {
                return false;
            }
            std::wstring err;
            return DecodeFrame(decoder.Get(), 0, out, err, false);
        }

        bool IconToImage(HICON icon, Image& out)
        {
            ICONINFO info{};
            if (!GetIconInfo(icon, &info))
            {
                return false;
            }
            BITMAP bm{};
            GetObjectW(info.hbmColor != nullptr ? info.hbmColor : info.hbmMask, sizeof(bm), &bm);
            const int w = bm.bmWidth;
            const int h = info.hbmColor != nullptr ? bm.bmHeight : bm.bmHeight / 2;
            bool ok = w > 0 && h > 0;
            if (ok)
            {
                out = Image(w, h);
                HDC dc = CreateCompatibleDC(nullptr);
                BITMAPINFO bi{};
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = w;
                bi.bmiHeader.biHeight = -h;
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;
                if (info.hbmColor != nullptr)
                {
                    GetDIBits(dc, info.hbmColor, 0, UINT(h), out.pixels.data(), &bi, DIB_RGB_COLORS);
                }
                bool anyAlpha = false;
                for (Color c : out.pixels)
                {
                    if (Alpha(c) != 0)
                    {
                        anyAlpha = true;
                        break;
                    }
                }
                if (!anyAlpha)
                {
                    std::vector<std::uint32_t> mask(std::size_t(w) * std::size_t(h));
                    GetDIBits(dc, info.hbmMask, 0, UINT(h), mask.data(), &bi, DIB_RGB_COLORS);
                    for (std::size_t i = 0; i < out.pixels.size(); ++i)
                    {
                        const bool transparent = (mask[i] & 0x00FFFFFFu) != 0;
                        out.pixels[i] = transparent ? 0 : Opaque(out.pixels[i]);
                    }
                }
                DeleteDC(dc);
            }
            if (info.hbmColor != nullptr)
            {
                DeleteObject(info.hbmColor);
            }
            if (info.hbmMask != nullptr)
            {
                DeleteObject(info.hbmMask);
            }
            return ok;
        }

        std::wstring LowerExt(const std::wstring& path)
        {
            const std::size_t dot = path.rfind(L'.');
            if (dot == std::wstring::npos)
            {
                return L"";
            }
            return ToLower(path.substr(dot));
        }

        bool EncodePng(const Image& img, std::vector<unsigned char>& bytes)
        {
            IWICImagingFactory* f = Factory();
            if (f == nullptr)
            {
                return false;
            }
            IStream* raw = nullptr;
            if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &raw)))
            {
                return false;
            }
            Com<IStream> stream;
            *stream.Put() = raw;
            Com<IWICBitmapEncoder> encoder;
            if (FAILED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.Put())) ||
                FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)))
            {
                return false;
            }
            Com<IWICBitmapFrameEncode> frame;
            Com<IPropertyBag2> props;
            if (FAILED(encoder->CreateNewFrame(frame.Put(), props.Put())) || FAILED(frame->Initialize(props.Get())))
            {
                return false;
            }
            frame->SetSize(UINT(img.width), UINT(img.height));
            WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
            frame->SetPixelFormat(&fmt);
            if (FAILED(frame->WritePixels(UINT(img.height), UINT(img.width * 4), UINT(img.width * img.height * 4),
                                          reinterpret_cast<BYTE*>(const_cast<Color*>(img.pixels.data())))) ||
                FAILED(frame->Commit()) || FAILED(encoder->Commit()))
            {
                return false;
            }
            HGLOBAL mem = nullptr;
            if (FAILED(GetHGlobalFromStream(stream.Get(), &mem)))
            {
                return false;
            }
            STATSTG st{};
            stream->Stat(&st, STATFLAG_NONAME);
            const std::size_t size = std::size_t(st.cbSize.QuadPart);
            const void* p = GlobalLock(mem);
            if (p == nullptr)
            {
                return false;
            }
            bytes.assign(static_cast<const unsigned char*>(p), static_cast<const unsigned char*>(p) + size);
            GlobalUnlock(mem);
            return true;
        }

        bool ParseDib(const void* data, std::size_t size, Image& out)
        {
            if (size < sizeof(BITMAPINFOHEADER))
            {
                return false;
            }
            const auto* h = static_cast<const BITMAPINFOHEADER*>(data);
            const int w = h->biWidth;
            const int hh = h->biHeight < 0 ? -h->biHeight : h->biHeight;
            const bool topDown = h->biHeight < 0;
            const int bpp = h->biBitCount;
            if (w <= 0 || hh <= 0 || (bpp != 24 && bpp != 32))
            {
                return false;
            }
            if (h->biCompression != BI_RGB && h->biCompression != BI_BITFIELDS)
            {
                return false;
            }
            std::size_t offset = h->biSize;
            if (h->biCompression == BI_BITFIELDS && h->biSize == sizeof(BITMAPINFOHEADER))
            {
                offset += 12;
            }
            offset += std::size_t(h->biClrUsed) * 4;
            const std::size_t stride = ((std::size_t(w) * std::size_t(bpp) + 31) / 32) * 4;
            if (offset + stride * std::size_t(hh) > size)
            {
                return false;
            }
            const auto* pixels = static_cast<const unsigned char*>(data) + offset;
            out = Image(w, hh);
            bool anyAlpha = false;
            for (int y = 0; y < hh; ++y)
            {
                const unsigned char* row = pixels + stride * std::size_t(topDown ? y : hh - 1 - y);
                Color* dst = out.Row(y);
                for (int x = 0; x < w; ++x)
                {
                    const unsigned char* p = row + std::size_t(x) * std::size_t(bpp / 8);
                    const int a = bpp == 32 ? p[3] : 255;
                    if (bpp == 32 && a != 0)
                    {
                        anyAlpha = true;
                    }
                    dst[x] = Argb(a, p[2], p[1], p[0]);
                }
            }
            if (bpp == 32 && !anyAlpha)
            {
                for (auto& c : out.pixels)
                {
                    c = Opaque(c);
                }
            }
            return true;
        }

        class ClipboardLock
        {
        public:
            explicit ClipboardLock(HWND owner)
            {
                for (int i = 0; i < 10 && !open_; ++i)
                {
                    open_ = OpenClipboard(owner) != FALSE;
                    if (!open_)
                    {
                        Sleep(20);
                    }
                }
            }
            ~ClipboardLock()
            {
                if (open_)
                {
                    CloseClipboard();
                }
            }
            ClipboardLock(const ClipboardLock&) = delete;
            ClipboardLock& operator=(const ClipboardLock&) = delete;
            bool Ok() const { return open_; }

        private:
            bool open_ = false;
        };
    }

    bool LoadImageFile(const std::wstring& path, Image& out, std::wstring& error)
    {
        IWICImagingFactory* f = Factory();
        if (f == nullptr)
        {
            error = L"компонент WIC недоступен";
            return false;
        }
        Com<IWICBitmapDecoder> decoder;
        HRESULT hr = f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.Put());
        if (FAILED(hr))
        {
            error = HrText(hr);
            return false;
        }
        return DecodeFrame(decoder.Get(), 0, out, error, true);
    }

    bool LoadIconFile(const std::wstring& path, Image& out, std::wstring& error)
    {
        const std::wstring ext = LowerExt(path);
        if (ext == L".exe" || ext == L".dll" || ext == L".cpl" || ext == L".icl")
        {
            HICON smallIcon = nullptr;
            if (ExtractIconExW(path.c_str(), 0, nullptr, &smallIcon, 1) == 0 || smallIcon == nullptr)
            {
                error = L"в файле нет иконок";
                return false;
            }
            const bool ok = IconToImage(smallIcon, out);
            DestroyIcon(smallIcon);
            if (!ok)
            {
                error = L"не удалось прочитать иконку";
            }
            return ok;
        }
        IWICImagingFactory* f = Factory();
        if (f == nullptr)
        {
            error = L"компонент WIC недоступен";
            return false;
        }
        Com<IWICBitmapDecoder> decoder;
        HRESULT hr = f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.Put());
        if (FAILED(hr))
        {
            error = HrText(hr);
            return false;
        }
        UINT count = 0;
        decoder->GetFrameCount(&count);
        if (ext == L".ico" && count > 1)
        {
            int best = -1;
            int bestScore = -1;
            for (UINT i = 0; i < count; ++i)
            {
                Com<IWICBitmapFrameDecode> frame;
                if (FAILED(decoder->GetFrame(i, frame.Put())))
                {
                    continue;
                }
                UINT w = 0;
                UINT h = 0;
                frame->GetSize(&w, &h);
                WICPixelFormatGUID fmt{};
                frame->GetPixelFormat(&fmt);
                int score = 0;
                if (w == 16 && h == 16)
                {
                    score = 1000;
                }
                else if (w >= 16)
                {
                    score = 500 - int(w);
                }
                else
                {
                    score = int(w);
                }
                if (IsEqualGUID(fmt, GUID_WICPixelFormat32bppBGRA))
                {
                    score += 10;
                }
                if (score > bestScore)
                {
                    bestScore = score;
                    best = int(i);
                }
            }
            if (best >= 0)
            {
                return DecodeFrame(decoder.Get(), UINT(best), out, error, false);
            }
        }
        return DecodeFrame(decoder.Get(), 0, out, error, true);
    }

    bool SaveImageFile(const std::wstring& path, const Image& img, ImageFormat format, int quality, std::wstring& error)
    {
        IWICImagingFactory* f = Factory();
        if (f == nullptr)
        {
            error = L"компонент WIC недоступен";
            return false;
        }
        Com<IWICStream> stream;
        HRESULT hr = f->CreateStream(stream.Put());
        if (SUCCEEDED(hr))
        {
            hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
        }
        if (FAILED(hr))
        {
            error = HrText(hr);
            return false;
        }
        Com<IWICBitmapEncoder> encoder;
        hr = f->CreateEncoder(format == ImageFormat::Png ? GUID_ContainerFormatPng : GUID_ContainerFormatJpeg, nullptr, encoder.Put());
        if (SUCCEEDED(hr))
        {
            hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
        }
        Com<IWICBitmapFrameEncode> frame;
        Com<IPropertyBag2> props;
        if (SUCCEEDED(hr))
        {
            hr = encoder->CreateNewFrame(frame.Put(), props.Put());
        }
        if (SUCCEEDED(hr) && format == ImageFormat::Jpeg && props)
        {
            PROPBAG2 option{};
            wchar_t name[] = L"ImageQuality";
            option.pstrName = name;
            VARIANT value;
            VariantInit(&value);
            value.vt = VT_R4;
            value.fltVal = float(std::clamp(quality, 1, 100)) / 100.0f;
            props->Write(1, &option, &value);
        }
        if (SUCCEEDED(hr))
        {
            hr = frame->Initialize(props.Get());
        }
        if (SUCCEEDED(hr))
        {
            hr = frame->SetSize(UINT(img.width), UINT(img.height));
        }
        bool hasAlpha = false;
        if (format == ImageFormat::Png)
        {
            for (Color c : img.pixels)
            {
                if (Alpha(c) != 255)
                {
                    hasAlpha = true;
                    break;
                }
            }
        }
        WICPixelFormatGUID fmt = hasAlpha ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat24bppBGR;
        if (SUCCEEDED(hr))
        {
            hr = frame->SetPixelFormat(&fmt);
        }
        if (SUCCEEDED(hr))
        {
            if (IsEqualGUID(fmt, GUID_WICPixelFormat32bppBGRA))
            {
                hr = frame->WritePixels(UINT(img.height), UINT(img.width * 4), UINT(img.width * img.height * 4),
                                        reinterpret_cast<BYTE*>(const_cast<Color*>(img.pixels.data())));
            }
            else
            {
                const UINT stride = (UINT(img.width) * 3 + 3) & ~3u;
                std::vector<BYTE> buf(std::size_t(stride) * std::size_t(img.height));
                for (int y = 0; y < img.height; ++y)
                {
                    BYTE* d = buf.data() + std::size_t(stride) * std::size_t(y);
                    const Color* s = img.Row(y);
                    for (int x = 0; x < img.width; ++x)
                    {
                        d[x * 3 + 0] = BYTE(Blue(s[x]));
                        d[x * 3 + 1] = BYTE(Green(s[x]));
                        d[x * 3 + 2] = BYTE(Red(s[x]));
                    }
                }
                hr = frame->WritePixels(UINT(img.height), stride, UINT(buf.size()), buf.data());
            }
        }
        if (SUCCEEDED(hr))
        {
            hr = frame->Commit();
        }
        if (SUCCEEDED(hr))
        {
            hr = encoder->Commit();
        }
        if (FAILED(hr))
        {
            error = HrText(hr);
            return false;
        }
        return true;
    }

    bool GetClipboardImage(HWND owner, Image& out)
    {
        ClipboardLock lock(owner);
        if (!lock.Ok())
        {
            return false;
        }
        const UINT png = RegisterClipboardFormatW(L"PNG");
        if (IsClipboardFormatAvailable(png))
        {
            HANDLE h = GetClipboardData(png);
            if (h != nullptr)
            {
                const void* p = GlobalLock(h);
                const std::size_t size = GlobalSize(h);
                const bool ok = p != nullptr && DecodeMemory(p, size, out);
                if (p != nullptr)
                {
                    GlobalUnlock(h);
                }
                if (ok)
                {
                    return true;
                }
            }
        }
        if (IsClipboardFormatAvailable(CF_DIB))
        {
            HANDLE h = GetClipboardData(CF_DIB);
            if (h != nullptr)
            {
                const void* p = GlobalLock(h);
                const bool ok = p != nullptr && ParseDib(p, GlobalSize(h), out);
                if (p != nullptr)
                {
                    GlobalUnlock(h);
                }
                if (ok)
                {
                    return true;
                }
            }
        }
        if (IsClipboardFormatAvailable(CF_BITMAP))
        {
            HBITMAP bmp = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
            BITMAP bm{};
            if (bmp != nullptr && GetObjectW(bmp, sizeof(bm), &bm) != 0 && bm.bmWidth > 0 && bm.bmHeight > 0)
            {
                out = Image(bm.bmWidth, bm.bmHeight);
                BITMAPINFO bi{};
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = bm.bmWidth;
                bi.bmiHeader.biHeight = -bm.bmHeight;
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;
                HDC dc = GetDC(nullptr);
                const int lines = GetDIBits(dc, bmp, 0, UINT(bm.bmHeight), out.pixels.data(), &bi, DIB_RGB_COLORS);
                ReleaseDC(nullptr, dc);
                for (auto& c : out.pixels)
                {
                    c = Opaque(c);
                }
                return lines > 0;
            }
        }
        return false;
    }

    std::vector<std::wstring> GetClipboardFiles(HWND owner)
    {
        std::vector<std::wstring> files;
        ClipboardLock lock(owner);
        if (!lock.Ok() || !IsClipboardFormatAvailable(CF_HDROP))
        {
            return files;
        }
        HDROP drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
        if (drop == nullptr)
        {
            return files;
        }
        const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < n; ++i)
        {
            const UINT len = DragQueryFileW(drop, i, nullptr, 0);
            std::wstring path(len, L'\0');
            DragQueryFileW(drop, i, path.data(), len + 1);
            files.push_back(path);
        }
        return files;
    }

    bool SetClipboardImage(HWND owner, const Image& img)
    {
        std::vector<unsigned char> png;
        EncodePng(img, png);
        const std::size_t pixelsSize = std::size_t(img.width) * std::size_t(img.height) * 4u;
        HGLOBAL dib = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + pixelsSize);
        if (dib == nullptr)
        {
            return false;
        }
        auto* header = static_cast<BITMAPINFOHEADER*>(GlobalLock(dib));
        if (header == nullptr)
        {
            GlobalFree(dib);
            return false;
        }
        std::memset(header, 0, sizeof(BITMAPINFOHEADER));
        header->biSize = sizeof(BITMAPINFOHEADER);
        header->biWidth = img.width;
        header->biHeight = img.height;
        header->biPlanes = 1;
        header->biBitCount = 32;
        header->biCompression = BI_RGB;
        header->biSizeImage = DWORD(pixelsSize);
        auto* dst = reinterpret_cast<unsigned char*>(header + 1);
        for (int y = 0; y < img.height; ++y)
        {
            std::memcpy(dst + std::size_t(img.height - 1 - y) * std::size_t(img.width) * 4u, img.Row(y), std::size_t(img.width) * 4u);
        }
        GlobalUnlock(dib);
        ClipboardLock lock(owner);
        if (!lock.Ok())
        {
            GlobalFree(dib);
            return false;
        }
        EmptyClipboard();
        bool ok = SetClipboardData(CF_DIB, dib) != nullptr;
        if (!ok)
        {
            GlobalFree(dib);
        }
        if (!png.empty())
        {
            HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, png.size());
            if (mem != nullptr)
            {
                void* p = GlobalLock(mem);
                if (p != nullptr)
                {
                    std::memcpy(p, png.data(), png.size());
                    GlobalUnlock(mem);
                    if (SetClipboardData(RegisterClipboardFormatW(L"PNG"), mem) == nullptr)
                    {
                        GlobalFree(mem);
                    }
                    else
                    {
                        ok = true;
                    }
                }
                else
                {
                    GlobalFree(mem);
                }
            }
        }
        return ok;
    }

    std::wstring GetClipboardText(HWND owner)
    {
        ClipboardLock lock(owner);
        if (!lock.Ok() || !IsClipboardFormatAvailable(CF_UNICODETEXT))
        {
            return L"";
        }
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        if (h == nullptr)
        {
            return L"";
        }
        const auto* p = static_cast<const wchar_t*>(GlobalLock(h));
        std::wstring text = p != nullptr ? std::wstring(p) : std::wstring();
        GlobalUnlock(h);
        return text;
    }

    void SetClipboardText(HWND owner, const std::wstring& text)
    {
        const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (mem == nullptr)
        {
            return;
        }
        void* p = GlobalLock(mem);
        if (p == nullptr)
        {
            GlobalFree(mem);
            return;
        }
        std::memcpy(p, text.c_str(), bytes);
        GlobalUnlock(mem);
        ClipboardLock lock(owner);
        if (!lock.Ok())
        {
            GlobalFree(mem);
            return;
        }
        EmptyClipboard();
        if (SetClipboardData(CF_UNICODETEXT, mem) == nullptr)
        {
            GlobalFree(mem);
        }
    }
}
