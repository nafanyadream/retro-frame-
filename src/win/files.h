#pragma once

#include "../core/host.h"

#include <windows.h>

namespace rf::win
{
    bool ListFolderItems(const std::wstring& path, std::vector<FolderItem>& out);
    std::unique_ptr<ByteSource> OpenFileSource(const std::wstring& path);
    std::wstring FindSevenZip();
    bool RunSevenZip(const std::wstring& tool, const std::wstring& archive, const std::wstring& folder, const std::function<bool()>& keepGoing, std::wstring& error);
    std::wstring CreateTempFolder();
    void DeleteTempFolder(const std::wstring& path);
}
