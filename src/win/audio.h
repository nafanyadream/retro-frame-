#pragma once

#include "../core/host.h"

#include <windows.h>

namespace rf::win
{
    std::unique_ptr<AudioOut> OpenWaveOut(int rate, int channels);
    std::vector<std::wstring> ListWaveIn();
    std::unique_ptr<AudioIn> OpenWaveIn(const std::wstring& name, int rate);
}
