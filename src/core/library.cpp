#include "library.h"

#include "icons.h"
#include "utf.h"

#include <algorithm>

namespace rf
{
    namespace
    {
        constexpr std::uint64_t MaxIconFile = 4ull * 1024ull * 1024ull;
        constexpr std::size_t MaxFiles = 20000;
        constexpr int MaxDepth = 40;

        struct Found
        {
            std::wstring path;
            std::wstring where;
        };

        std::wstring Extension(const std::wstring& name)
        {
            const std::wstring file = FileNameOf(name);
            const std::size_t dot = file.rfind(L'.');
            return dot == std::wstring::npos ? std::wstring() : ToLower(file.substr(dot));
        }

        int Depth(const std::wstring& where)
        {
            return int(std::count(where.begin(), where.end(), L'\\'));
        }

        bool Collect(Host& host, const std::wstring& root, std::vector<Found>& out, const LoadStep& step)
        {
            std::vector<std::pair<std::wstring, std::wstring>> stack{{root, std::wstring()}};
            int visited = 0;
            while (!stack.empty() && out.size() < MaxFiles)
            {
                const auto [dir, where] = stack.back();
                stack.pop_back();
                if (visited++ % 16 == 0 && !step(L"Поиск иконок: " + (where.empty() ? FileNameOf(root) : where), -1.0))
                {
                    return false;
                }
                std::vector<FolderItem> items;
                if (!host.ListFolder(dir, items))
                {
                    continue;
                }
                std::vector<std::pair<std::wstring, std::wstring>> subfolders;
                for (const auto& item : items)
                {
                    if (item.name.empty() || item.name == L"." || item.name == L"..")
                    {
                        continue;
                    }
                    const std::wstring child = where.empty() ? item.name : where + L"\\" + item.name;
                    if (item.folder)
                    {
                        if (Depth(child) < MaxDepth)
                        {
                            subfolders.emplace_back(JoinPath(dir, item.name), child);
                        }
                    }
                    else if (IsIconFile(item.name) && item.size <= MaxIconFile)
                    {
                        out.push_back(Found{JoinPath(dir, item.name), child});
                        if (out.size() >= MaxFiles)
                        {
                            break;
                        }
                    }
                }
                std::sort(subfolders.begin(), subfolders.end(), [](const auto& a, const auto& b) { return NaturalLess(b.second, a.second); });
                for (auto& s : subfolders)
                {
                    stack.push_back(std::move(s));
                }
            }
            return true;
        }

        void AddIcon(LibrarySource& source, const Image& image, const std::wstring& where)
        {
            LibraryIcon icon;
            icon.icon = MakeIcon16(image);
            icon.name = StemOf(FileNameOf(where));
            icon.where = where;
            icon.width = image.width;
            icon.height = image.height;
            source.icons.push_back(std::move(icon));
        }

        bool LoadFiles(Host& host, LibrarySource& source, std::vector<Found> files, const LoadStep& step)
        {
            std::sort(files.begin(), files.end(), [](const Found& a, const Found& b) { return NaturalLess(a.where, b.where); });
            const std::size_t n = files.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                if (!step(files[i].where, double(i) / double(n)))
                {
                    return false;
                }
                Image image;
                std::wstring error;
                if (host.ReadIcon(files[i].path, image, error) && !image.Empty())
                {
                    AddIcon(source, image, files[i].where);
                }
                else
                {
                    ++source.failed;
                }
            }
            return true;
        }

        bool LoadFolder(Host& host, LibrarySource& source, const LoadStep& step)
        {
            std::vector<FolderItem> probe;
            if (!host.ListFolder(source.path, probe))
            {
                source.problem = L"папка не найдена";
                return true;
            }
            std::vector<Found> files;
            if (!Collect(host, source.path, files, step))
            {
                return false;
            }
            return LoadFiles(host, source, std::move(files), step);
        }

        bool LoadWithSevenZip(Host& host, LibrarySource& source, const LoadStep& step)
        {
            if (host.SevenZipPath().empty())
            {
                source.needsSevenZip = true;
                source.problem = L"нужна программа 7-Zip";
                return true;
            }
            const std::wstring temp = host.NewTempFolder();
            if (temp.empty())
            {
                source.problem = L"не удалось создать временную папку";
                return true;
            }
            bool cancelled = false;
            std::wstring error;
            const std::wstring line = L"Распаковка: " + FileNameOf(source.path);
            const bool ok = host.ExtractArchive(source.path, temp, [&]() {
                cancelled = cancelled || !step(line, -1.0);
                return !cancelled;
            }, error);
            bool completed = !cancelled;
            if (ok && !cancelled)
            {
                std::vector<Found> files;
                completed = Collect(host, temp, files, step) && LoadFiles(host, source, std::move(files), step);
            }
            else if (!cancelled)
            {
                source.problem = error.empty() ? L"не удалось распаковать архив" : error;
            }
            host.RemoveFolder(temp);
            return completed;
        }

        bool LoadZip(Host& host, LibrarySource& source, const LoadStep& step)
        {
            auto file = host.OpenFile(source.path);
            if (!file)
            {
                source.problem = L"архив не найден";
                return true;
            }
            ZipReader zip(*file);
            const ZipError opened = zip.Open();
            if (opened == ZipError::NotZip)
            {
                return LoadWithSevenZip(host, source, step);
            }
            if (opened != ZipError::None)
            {
                source.problem = L"архив повреждён";
                return true;
            }
            std::vector<const ZipEntry*> entries;
            int foreign = 0;
            for (const auto& e : zip.Entries())
            {
                if (e.folder || !IsIconFile(e.name) || e.size > MaxIconFile)
                {
                    continue;
                }
                if (e.encrypted)
                {
                    ++source.failed;
                }
                else if (!ZipReader::Supported(e))
                {
                    ++foreign;
                }
                else
                {
                    entries.push_back(&e);
                }
            }
            if (foreign > 0 && entries.empty())
            {
                return LoadWithSevenZip(host, source, step);
            }
            source.failed += foreign;
            std::sort(entries.begin(), entries.end(), [](const ZipEntry* a, const ZipEntry* b) { return NaturalLess(a->name, b->name); });
            const std::size_t n = entries.size();
            std::vector<unsigned char> data;
            for (std::size_t i = 0; i < n; ++i)
            {
                const ZipEntry& e = *entries[i];
                if (!step(e.name, double(i) / double(n)))
                {
                    return false;
                }
                Image image;
                std::wstring error;
                if (zip.Extract(e, data) == ZipError::None && host.DecodeIcon(data, image, error) && !image.Empty())
                {
                    AddIcon(source, image, e.name);
                }
                else
                {
                    ++source.failed;
                }
            }
            return true;
        }
    }

    bool IsIconFile(const std::wstring& name)
    {
        const std::wstring ext = Extension(name);
        return ext == L".ico" || ext == L".png";
    }

    bool IsSevenZipArchive(const std::wstring& path)
    {
        const std::wstring ext = Extension(path);
        return ext == L".7z" || ext == L".rar";
    }

    bool IsProgramFile(const std::wstring& path)
    {
        const std::wstring ext = Extension(path);
        return ext == L".exe" || ext == L".dll" || ext == L".icl" || ext == L".cpl" || ext == L".ocx" || ext == L".scr" || ext == L".mun";
    }

    bool NaturalLess(const std::wstring& a, const std::wstring& b)
    {
        const std::wstring x = ToLower(a);
        const std::wstring y = ToLower(b);
        std::size_t i = 0;
        std::size_t j = 0;
        const auto digit = [](wchar_t c) { return c >= L'0' && c <= L'9'; };
        while (i < x.size() && j < y.size())
        {
            if (digit(x[i]) && digit(y[j]))
            {
                std::size_t ie = i;
                std::size_t je = j;
                while (ie < x.size() && digit(x[ie]))
                {
                    ++ie;
                }
                while (je < y.size() && digit(y[je]))
                {
                    ++je;
                }
                std::size_t is = i;
                std::size_t js = j;
                while (is + 1 < ie && x[is] == L'0')
                {
                    ++is;
                }
                while (js + 1 < je && y[js] == L'0')
                {
                    ++js;
                }
                if (ie - is != je - js)
                {
                    return ie - is < je - js;
                }
                const int c = x.compare(is, ie - is, y, js, je - js);
                if (c != 0)
                {
                    return c < 0;
                }
                i = ie;
                j = je;
                continue;
            }
            const wchar_t cx = x[i] == L'\\' ? L'\x01' : x[i];
            const wchar_t cy = y[j] == L'\\' ? L'\x01' : y[j];
            if (cx != cy)
            {
                return cx < cy;
            }
            ++i;
            ++j;
        }
        if (x.size() - i != y.size() - j)
        {
            return x.size() - i < y.size() - j;
        }
        return a < b;
    }

    std::wstring SourceName(const LibrarySource& source)
    {
        std::wstring name = FileNameOf(source.path);
        if (name.empty())
        {
            name = source.path;
        }
        return name;
    }

    std::vector<LibraryIcon> BuiltinLibrary()
    {
        std::vector<LibraryIcon> out;
        for (int i = 0; i < FrameIconCount(); ++i)
        {
            LibraryIcon icon;
            icon.icon = BuiltinIcon(FrameIconAt(i));
            icon.name = FrameIconName(i);
            icon.where = L"встроенная иконка";
            icon.width = 16;
            icon.height = 16;
            out.push_back(std::move(icon));
        }
        return out;
    }

    bool LoadSource(Host& host, LibrarySource& source, const LoadStep& step)
    {
        source.icons.clear();
        source.failed = 0;
        source.problem.clear();
        source.needsSevenZip = false;
        source.partial = false;
        bool completed = true;
        if (source.kind == SourceKind::Folder)
        {
            completed = LoadFolder(host, source, step);
        }
        else if (IsSevenZipArchive(source.path))
        {
            completed = LoadWithSevenZip(host, source, step);
        }
        else
        {
            completed = LoadZip(host, source, step);
        }
        source.partial = !completed;
        source.loaded = true;
        return completed;
    }

    std::wstring EncodeSource(const LibrarySource& source)
    {
        return (source.kind == SourceKind::Folder ? L"folder?" : L"archive?") + source.path;
    }

    bool DecodeSource(const std::wstring& text, LibrarySource& source)
    {
        const std::size_t q = text.find(L'?');
        if (q == std::wstring::npos || q + 1 >= text.size())
        {
            return false;
        }
        const std::wstring kind = text.substr(0, q);
        if (kind == L"folder")
        {
            source.kind = SourceKind::Folder;
        }
        else if (kind == L"archive")
        {
            source.kind = SourceKind::Archive;
        }
        else
        {
            return false;
        }
        source.path = text.substr(q + 1);
        source.loaded = false;
        source.icons.clear();
        return true;
    }
}
