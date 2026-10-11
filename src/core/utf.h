#pragma once

#include <string>
#include <vector>

namespace rf
{
    std::string ToUtf8(const std::wstring& s);
    std::wstring FromUtf8(const std::string& s);

    std::wstring FileNameOf(const std::wstring& path);
    std::wstring StemOf(const std::wstring& name);
    std::wstring DirectoryOf(const std::wstring& path);
    std::wstring JoinPath(const std::wstring& dir, const std::wstring& name);
    std::wstring Trim(const std::wstring& s);
    std::vector<std::wstring> SplitList(const std::wstring& s, wchar_t sep);
    std::wstring JoinList(const std::vector<std::wstring>& items, const std::wstring& sep);
    std::wstring ToLower(const std::wstring& s);
}
