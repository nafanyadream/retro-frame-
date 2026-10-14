#pragma once

#include "zip.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rf
{
    enum VoiceKind
    {
        VoicePitch = 0,
        VoiceSpeed,
        VoiceRobot,
        VoiceMetal,
        VoiceDistortion,
        VoiceBits,
        VoiceRadio,
        VoiceGlitch,
        VoiceWobble,
        VoiceChorus,
        VoiceEcho,
        VoiceHall,
        VoiceReverse,
        VoiceVolume,
        VoiceKindCount
    };

    struct VoiceParam
    {
        const wchar_t* name;
        int min;
        int max;
        int def;
        const wchar_t* unit;
    };

    struct VoiceInfo
    {
        const wchar_t* name;
        const wchar_t* about;
        int params;
        VoiceParam p[2];
    };

    const VoiceInfo& VoiceInfoOf(int kind);
    std::wstring VoiceValueText(int kind, int param, int value);

    struct VoiceStep
    {
        int kind = 0;
        bool on = false;
        int a = 0;
        int b = 0;
    };

    using VoiceChain = std::vector<VoiceStep>;

    struct VoicePreset
    {
        std::wstring name;
        VoiceChain chain;
    };

    VoiceStep DefaultVoiceStep(int kind);
    VoiceChain DefaultVoiceChain();
    void NormalizeVoiceChain(VoiceChain& chain);
    int VoiceEffectCount(const VoiceChain& chain);
    std::wstring VoiceSummary(const VoiceChain& chain);
    std::string SerializeVoiceChain(const VoiceChain& chain);
    VoiceChain ParseVoiceChain(const std::string& text);
    std::vector<VoicePreset> BuiltinVoicePresets();

    std::vector<std::int16_t> ProcessVoice(const std::vector<std::int16_t>& samples, int rate, const VoiceChain& chain, long long offset = 0, bool tails = true);
    std::vector<std::int16_t> ResampleVoice(const std::vector<std::int16_t>& samples, int from, int to);

    class VoiceMonitor
    {
    public:
        VoiceMonitor(const VoiceChain& chain, int rate);
        void Push(const std::int16_t* data, std::size_t count);
        std::vector<std::int16_t> Take();
        int Rate() const { return rate_; }

    private:
        VoiceChain chain_;
        int rate_ = 44100;
        std::size_t chunk_ = 0;
        std::size_t context_ = 0;
        std::size_t fade_ = 0;
        std::vector<std::int16_t> history_;
        std::size_t base_ = 0;
        std::size_t done_ = 0;
        std::vector<float> tail_;
    };

    std::vector<unsigned char> EncodeWav(const std::vector<std::int16_t>& samples, int rate, int bits);
    bool DecodeWav(ByteSource& source, std::vector<std::int16_t>& samples, int& rate, int& bits);
}
