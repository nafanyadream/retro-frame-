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

        class WaveIn : public AudioIn
        {
        public:
            static constexpr int Count = 16;

            WaveIn(HWAVEIN device, std::size_t size) : device_(device)
            {
                for (auto& d : data_)
                {
                    d.resize(size);
                }
            }

            WaveIn(const WaveIn&) = delete;
            WaveIn& operator=(const WaveIn&) = delete;

            ~WaveIn() override
            {
                waveInStop(device_);
                waveInReset(device_);
                for (int i = 0; i < Count; ++i)
                {
                    if (prepared_[i])
                    {
                        waveInUnprepareHeader(device_, &headers_[i], sizeof(WAVEHDR));
                    }
                }
                waveInClose(device_);
            }

            bool Start()
            {
                for (int i = 0; i < Count; ++i)
                {
                    if (!Queue(i))
                    {
                        return false;
                    }
                }
                return waveInStart(device_) == MMSYSERR_NOERROR;
            }

            bool Read(std::vector<std::int16_t>& out) override
            {
                for (int guard = 0; guard < Count; ++guard)
                {
                    WAVEHDR& h = headers_[next_];
                    if ((h.dwFlags & WHDR_DONE) == 0)
                    {
                        break;
                    }
                    const auto* p = reinterpret_cast<const std::int16_t*>(h.lpData);
                    out.insert(out.end(), p, p + h.dwBytesRecorded / 2);
                    waveInUnprepareHeader(device_, &h, sizeof(WAVEHDR));
                    prepared_[next_] = false;
                    if (!Queue(next_))
                    {
                        return false;
                    }
                    next_ = (next_ + 1) % Count;
                }
                return true;
            }

        private:
            bool Queue(int i)
            {
                WAVEHDR& h = headers_[i];
                std::memset(&h, 0, sizeof(h));
                h.lpData = data_[i].data();
                h.dwBufferLength = DWORD(data_[i].size());
                if (waveInPrepareHeader(device_, &h, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
                {
                    return false;
                }
                prepared_[i] = true;
                return waveInAddBuffer(device_, &h, sizeof(WAVEHDR)) == MMSYSERR_NOERROR;
            }

            HWAVEIN device_ = nullptr;
            WAVEHDR headers_[Count] = {};
            std::vector<char> data_[Count];
            bool prepared_[Count] = {};
            int next_ = 0;
        };
    }

    std::vector<std::wstring> ListWaveIn()
    {
        std::vector<std::wstring> names;
        const UINT count = waveInGetNumDevs();
        for (UINT i = 0; i < count; ++i)
        {
            WAVEINCAPSW caps{};
            if (waveInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
            {
                names.push_back(caps.szPname);
            }
        }
        return names;
    }

    std::unique_ptr<AudioIn> OpenWaveIn(const std::wstring& name, int rate)
    {
        UINT id = WAVE_MAPPER;
        const UINT count = waveInGetNumDevs();
        for (UINT i = 0; i < count && !name.empty(); ++i)
        {
            WAVEINCAPSW caps{};
            if (waveInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR && name == caps.szPname)
            {
                id = i;
                break;
            }
        }
        WAVEFORMATEX fmt{};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = 1;
        fmt.nSamplesPerSec = DWORD(rate);
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = 2;
        fmt.nAvgBytesPerSec = DWORD(rate) * 2;
        HWAVEIN device = nullptr;
        if (waveInOpen(&device, id, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            return nullptr;
        }
        auto in = std::make_unique<WaveIn>(device, std::size_t(rate / 10) * 2);
        if (!in->Start())
        {
            return nullptr;
        }
        return in;
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
