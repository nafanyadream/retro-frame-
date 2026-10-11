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
            const bool mono = info.hbmColor == nullptr;
            BITMAP bm{};
            GetObjectW(mono ? info.hbmMask : info.hbmColor, sizeof(bm), &bm);
            const int w = bm.bmWidth;
            const int h = mono ? bm.bmHeight / 2 : bm.bmHeight;
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
                if (mono)
                {
                    const std::size_t n = out.pixels.size();
                    std::vector<std::uint32_t> bits(n * 2u);
                    BITMAPINFO both = bi;
                    both.bmiHeader.biHeight = -h * 2;
                    GetDIBits(dc, info.hbmMask, 0, UINT(h * 2), bits.data(), &both, DIB_RGB_COLORS);
                    for (std::size_t i = 0; i < n; ++i)
                    {
                        const bool clear = (bits[i] & 0x00FFFFFFu) != 0;
                        const bool white = (bits[i + n] & 0x00FFFFFFu) != 0;
                        out.pixels[i] = clear && !white ? 0 : (white && !clear ? Rgb(255, 255, 255) : Rgb(0, 0, 0));
                    }
                }
                else
                {
                    GetDIBits(dc, info.hbmColor, 0, UINT(h), out.pixels.data(), &bi, DIB_RGB_COLORS);
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

        int FrameScore(UINT w, UINT h, int bits)
        {
            int base = 0;
            if (w == 16 && h == 16)
            {
                base = 3000;
            }
            else if (w >= 16 && h >= 16)
            {
                base = 2000 - int(std::min<UINT>(w, 1999));
            }
            else
            {
                base = int(std::max(w, h));
            }
            return base * 64 + std::clamp(bits, 0, 63);
        }

        int FormatBits(const WICPixelFormatGUID& fmt)
        {
            if (IsEqualGUID(fmt, GUID_WICPixelFormat32bppBGRA) || IsEqualGUID(fmt, GUID_WICPixelFormat32bppPBGRA) ||
                IsEqualGUID(fmt, GUID_WICPixelFormat32bppRGBA))
            {
                return 32;
            }
            if (IsEqualGUID(fmt, GUID_WICPixelFormat24bppBGR) || IsEqualGUID(fmt, GUID_WICPixelFormat32bppBGR))
            {
                return 24;
            }
            if (IsEqualGUID(fmt, GUID_WICPixelFormat8bppIndexed))
            {
                return 8;
            }
            if (IsEqualGUID(fmt, GUID_WICPixelFormat4bppIndexed))
            {
                return 4;
            }
            return 1;
        }

        bool DecodeBestFrame(IWICBitmapDecoder* decoder, Image& out, std::wstring& error)
        {
            GUID container{};
            decoder->GetContainerFormat(&container);
            const bool ico = IsEqualGUID(container, GUID_ContainerFormatIco) != FALSE;
            UINT count = 0;
            decoder->GetFrameCount(&count);
            if (!ico || count <= 1)
            {
                return DecodeFrame(decoder, 0, out, error, !ico);
            }
            int best = 0;
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
                const int score = FrameScore(w, h, FormatBits(fmt));
                if (score > bestScore)
                {
                    bestScore = score;
                    best = int(i);
                }
            }
            return DecodeFrame(decoder, UINT(best), out, error, false);
        }

#pragma pack(push, 2)
        struct GroupIconEntry
        {
            BYTE width;
            BYTE height;
            BYTE colors;
            BYTE reserved;
            WORD planes;
            WORD bitCount;
            DWORD bytes;
            WORD id;
        };

        struct GroupIconHeader
        {
            WORD reserved;
            WORD type;
            WORD count;
        };
#pragma pack(pop)

        struct ResourceName
        {
            ULONG_PTR id = 0;
            std::wstring text;
            LPCWSTR Get() const { return id != 0 ? MAKEINTRESOURCEW(id) : text.c_str(); }
        };

        BOOL CALLBACK CollectGroup(HMODULE, LPCWSTR, LPWSTR name, LONG_PTR param)
        {
            auto* names = reinterpret_cast<std::vector<ResourceName>*>(param);
            ResourceName n;
            if (IS_INTRESOURCE(name))
            {
                n.id = reinterpret_cast<ULONG_PTR>(name);
            }
            else
            {
                n.text = name;
            }
            names->push_back(std::move(n));
            return names->size() < 4000 ? TRUE : FALSE;
        }

        bool ResourceBytes(HMODULE module, LPCWSTR name, LPCWSTR type, const BYTE*& data, DWORD& size)
        {
            HRSRC res = FindResourceW(module, name, type);
            if (res == nullptr)
            {
                return false;
            }
            HGLOBAL mem = LoadResource(module, res);
            size = SizeofResource(module, res);
            data = mem != nullptr ? static_cast<const BYTE*>(LockResource(mem)) : nullptr;
            return data != nullptr && size > 0;
        }

        bool DecodeIconEntry(const BYTE* data, DWORD size, Image& out)
        {
            if (size >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')
            {
                return DecodeMemory(data, size, out);
            }
            if (size < sizeof(BITMAPINFOHEADER))
            {
                return false;
            }
            const auto* bih = reinterpret_cast<const BITMAPINFOHEADER*>(data);
            const int w = bih->biWidth;
            const int h = (bih->biHeight < 0 ? -bih->biHeight : bih->biHeight) / 2;
            if (w <= 0 || h <= 0 || w > 1024 || h > 1024)
            {
                return false;
            }
            HICON icon = CreateIconFromResourceEx(const_cast<PBYTE>(data), size, TRUE, 0x00030000, w, h, LR_DEFAULTCOLOR);
            if (icon == nullptr)
            {
                return false;
            }
            const bool ok = IconToImage(icon, out);
            DestroyIcon(icon);
            return ok;
        }

        bool ReadIconResources(const std::wstring& path, std::vector<Image>& out)
        {
            HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
            if (module == nullptr)
            {
                return false;
            }
            std::vector<ResourceName> groups;
            EnumResourceNamesW(module, MAKEINTRESOURCEW(14), CollectGroup, reinterpret_cast<LONG_PTR>(&groups));
            for (const auto& g : groups)
            {
                const BYTE* data = nullptr;
                DWORD size = 0;
                if (!ResourceBytes(module, g.Get(), MAKEINTRESOURCEW(14), data, size) || size < sizeof(GroupIconHeader))
                {
                    continue;
                }
                const auto* header = reinterpret_cast<const GroupIconHeader*>(data);
                const std::size_t count = std::min<std::size_t>(header->count, (size - sizeof(GroupIconHeader)) / sizeof(GroupIconEntry));
                const auto* entries = reinterpret_cast<const GroupIconEntry*>(data + sizeof(GroupIconHeader));
                std::vector<std::size_t> order(count);
                for (std::size_t i = 0; i < count; ++i)
                {
                    order[i] = i;
                }
                const auto score = [&](std::size_t i) {
                    const UINT w = entries[i].width == 0 ? 256u : entries[i].width;
                    const UINT h = entries[i].height == 0 ? 256u : entries[i].height;
                    return FrameScore(w, h, entries[i].bitCount);
                };
                std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return score(a) > score(b); });
                for (const std::size_t i : order)
                {
                    const BYTE* icon = nullptr;
                    DWORD iconSize = 0;
                    Image img;
                    if (ResourceBytes(module, MAKEINTRESOURCEW(entries[i].id), MAKEINTRESOURCEW(3), icon, iconSize) && DecodeIconEntry(icon, iconSize, img))
                    {
                        out.push_back(std::move(img));
                        break;
                    }
                }
            }
            FreeLibrary(module);
            return true;
        }

        std::wstring MunPath(const std::wstring& path)
        {
            wchar_t windows[MAX_PATH] = {};
            if (GetWindowsDirectoryW(windows, MAX_PATH) == 0)
            {
                return L"";
            }
            const std::wstring mun = std::wstring(windows) + L"\\SystemResources\\" + FileNameOf(path) + L".mun";
            return GetFileAttributesW(mun.c_str()) != INVALID_FILE_ATTRIBUTES ? mun : std::wstring();
        }

        void ExtractEachIcon(const std::wstring& path, std::vector<Image>& out)
        {
            const UINT total = ExtractIconExW(path.c_str(), -1, nullptr, nullptr, 0);
            for (UINT i = 0; i < total && i < 4000; ++i)
            {
                HICON icon = nullptr;
                UINT id = 0;
                if (PrivateExtractIconsW(path.c_str(), int(i), 16, 16, &icon, &id, 1, LR_DEFAULTCOLOR) == 1 && icon != nullptr)
                {
                    Image img;
                    if (IconToImage(icon, img))
                    {
                        out.push_back(std::move(img));
                    }
                    DestroyIcon(icon);
                }
            }
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
        if (ext == L".exe" || ext == L".dll" || ext == L".icl" || ext == L".cpl" || ext == L".ocx" || ext == L".scr" || ext == L".mun")
        {
            std::vector<Image> icons;
            if (!LoadFileIcons(path, icons, error) || icons.empty())
            {
                if (error.empty())
                {
                    error = L"в файле нет иконок";
                }
                return false;
            }
            out = std::move(icons.front());
            return true;
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
        return DecodeBestFrame(decoder.Get(), out, error);
    }

    bool DecodeIconMemory(const std::vector<unsigned char>& data, Image& out, std::wstring& error)
    {
        IWICImagingFactory* f = Factory();
        if (f == nullptr)
        {
            error = L"компонент WIC недоступен";
            return false;
        }
        if (data.empty())
        {
            error = L"пустой файл";
            return false;
        }
        Com<IWICStream> stream;
        HRESULT hr = f->CreateStream(stream.Put());
        if (SUCCEEDED(hr))
        {
            hr = stream->InitializeFromMemory(const_cast<BYTE*>(data.data()), DWORD(data.size()));
        }
        Com<IWICBitmapDecoder> decoder;
        if (SUCCEEDED(hr))
        {
            hr = f->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.Put());
        }
        if (FAILED(hr))
        {
            error = HrText(hr);
            return false;
        }
        return DecodeBestFrame(decoder.Get(), out, error);
    }

    bool LoadFileIcons(const std::wstring& path, std::vector<Image>& out, std::wstring& error)
    {
        out.clear();
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            error = L"файл не найден";
            return false;
        }
        if (ReadIconResources(path, out) && !out.empty())
        {
            return true;
        }
        const std::wstring mun = MunPath(path);
        if (!mun.empty() && ReadIconResources(mun, out) && !out.empty())
        {
            return true;
        }
        ExtractEachIcon(path, out);
        if (out.empty())
        {
            error = L"в файле нет иконок";
            return false;
        }
        return true;
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
