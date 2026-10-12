#pragma once

#include "../core/host.h"

#include <windows.h>

namespace rf::win
{
    std::unique_ptr<AudioOut> OpenWaveOut(int rate, int channels);
}
