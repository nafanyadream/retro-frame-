#pragma once

#include "../core/host.h"

#include <windows.h>

namespace rf::win
{
    std::unique_ptr<Process> StartChild(const std::wstring& exe, const std::vector<std::wstring>& args, bool input, bool output);
    std::wstring FindFfmpeg();
    std::wstring QuoteArg(const std::wstring& s);
}
