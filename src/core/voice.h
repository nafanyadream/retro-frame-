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

    std::vector<std::int16_t> ProcessVoice(const std::vector<std::int16_t>& samples, int rate, const VoiceChain& chain);
    std::vector<std::int16_t> ResampleVoice(const std::vector<std::int16_t>& samples, int from, int to);

    std::vector<unsigned char> EncodeWav(const std::vector<std::int16_t>& samples, int rate, int bits);
    bool DecodeWav(ByteSource& source, std::vector<std::int16_t>& samples, int& rate, int& bits);
}
