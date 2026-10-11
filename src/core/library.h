#pragma once

#include "host.h"

#include <functional>

namespace rf
{
    enum class SourceKind
    {
        Folder = 0,
        Archive = 1
    };

    struct LibraryIcon
    {
        Image icon;
        std::wstring name;
        std::wstring where;
        int width = 0;
        int height = 0;
    };

    struct LibrarySource
    {
        SourceKind kind = SourceKind::Folder;
        std::wstring path;
        bool loaded = false;
        bool partial = false;
        bool needsSevenZip = false;
        int failed = 0;
        std::wstring problem;
        std::vector<LibraryIcon> icons;
    };

    using LoadStep = std::function<bool(const std::wstring& line, double fraction)>;

    bool IsIconFile(const std::wstring& name);
    bool IsSevenZipArchive(const std::wstring& path);
    bool IsProgramFile(const std::wstring& path);
    bool NaturalLess(const std::wstring& a, const std::wstring& b);
    std::wstring SourceName(const LibrarySource& source);
    std::vector<LibraryIcon> BuiltinLibrary();
    bool LoadSource(Host& host, LibrarySource& source, const LoadStep& step);
    std::wstring EncodeSource(const LibrarySource& source);
    bool DecodeSource(const std::wstring& text, LibrarySource& source);
}
