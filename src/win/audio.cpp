#include "audio.h"

#include <mmsystem.h>

#include <cstring>
#include <vector>

namespace rf::win
{
    namespace
    {
        class WaveOut : public AudioOut
        {
        public:
            static constexpr int Count = 12;
            static constexpr std::size_t Size = 16384;

            explicit WaveOut(HWAVEOUT device) : device_(device)
            {
                for (auto& d : data_)
                {
                    d.resize(Size);
                }
            }

            WaveOut(const WaveOut&) = delete;
            WaveOut& operator=(const WaveOut&) = delete;

            ~WaveOut() override
            {
                waveOutReset(device_);
                for (int i = 0; i < Count; ++i)
                {
                    if (used_[i])
                    {
                        waveOutUnprepareHeader(device_, &headers_[i], sizeof(WAVEHDR));
                    }
                }
                waveOutClose(device_);
            }

            std::size_t Space() override
            {
                std::size_t free = 0;
                for (int i = 0; i < Count; ++i)
                {
                    free += Reclaim(i) ? Size : 0;
                }
                return free;
            }

            bool Write(const void* data, std::size_t size) override
            {
                const auto* p = static_cast<const char*>(data);
                while (size > 0)
                {
                    int slot = -1;
                    for (int i = 0; i < Count && slot < 0; ++i)
                    {
                        if (Reclaim(i))
                        {
                            slot = i;
                        }
                    }
                    if (slot < 0)
                    {
                        return false;
                    }
                    const std::size_t chunk = std::min(size, Size);
                    std::memcpy(data_[slot].data(), p, chunk);
                    WAVEHDR& h = headers_[slot];
                    std::memset(&h, 0, sizeof(h));
                    h.lpData = data_[slot].data();
                    h.dwBufferLength = DWORD(chunk);
                    if (waveOutPrepareHeader(device_, &h, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
                    {
                        return false;
                    }
                    used_[slot] = true;
                    if (waveOutWrite(device_, &h, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
                    {
                        waveOutUnprepareHeader(device_, &h, sizeof(WAVEHDR));
                        used_[slot] = false;
                        return false;
                    }
                    p += chunk;
                    size -= chunk;
                }
                return true;
            }

        private:
            bool Reclaim(int i)
            {
                if (!used_[i])
                {
                    return true;
                }
                if ((headers_[i].dwFlags & WHDR_DONE) == 0)
                {
                    return false;
                }
                waveOutUnprepareHeader(device_, &headers_[i], sizeof(WAVEHDR));
                used_[i] = false;
                return true;
            }

            HWAVEOUT device_ = nullptr;
            WAVEHDR headers_[Count] = {};
            std::vector<char> data_[Count];
            bool used_[Count] = {};
        };
    }

    std::unique_ptr<AudioOut> OpenWaveOut(int rate, int channels)
    {
        WAVEFORMATEX fmt{};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = WORD(channels);
        fmt.nSamplesPerSec = DWORD(rate);
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = WORD(channels * 2);
        fmt.nAvgBytesPerSec = DWORD(rate) * fmt.nBlockAlign;
        HWAVEOUT device = nullptr;
        if (waveOutOpen(&device, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            return nullptr;
        }
        return std::make_unique<WaveOut>(device);
    }
}
