#include "../src/core/settings.h"
#include "../src/core/voice.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    int failures = 0;

    void Check(bool ok, const char* what)
    {
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
        if (!ok)
        {
            ++failures;
        }
    }

    class Memory : public rf::ByteSource
    {
    public:
        explicit Memory(std::vector<unsigned char> d) : d_(std::move(d)) {}
        std::uint64_t Size() const override { return d_.size(); }
        bool Read(std::uint64_t offset, void* dst, std::size_t size) override
        {
            if (offset + size > d_.size())
            {
                return false;
            }
            std::memcpy(dst, d_.data() + offset, size);
            return true;
        }

    private:
        std::vector<unsigned char> d_;
    };

    std::vector<std::int16_t> Voice(int rate, double seconds, double f0)
    {
        std::vector<std::int16_t> out(std::size_t(rate * seconds));
        double phase = 0.0;
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const double t = double(i) / rate;
            phase += 2.0 * 3.14159265358979 * f0 / rate;
            double v = 0.0;
            for (int h = 1; h <= 6; ++h)
            {
                v += std::sin(h * phase) / h;
            }
            const double env = 0.6 + 0.4 * std::sin(2.0 * 3.14159265358979 * 2.0 * t);
            out[i] = std::int16_t(std::lround(v * env * 9000.0));
        }
        return out;
    }

    double Rms(const std::vector<std::int16_t>& x)
    {
        double sum = 0.0;
        for (auto v : x)
        {
            sum += double(v) * v;
        }
        return x.empty() ? 0.0 : std::sqrt(sum / x.size()) / 32768.0;
    }

    double Fundamental(const std::vector<std::int16_t>& x, int rate)
    {
        const std::size_t start = x.size() / 4;
        const std::size_t len = std::size_t(rate * 0.2);
        const int lo = rate / 600;
        const int hi = rate / 50;
        std::vector<double> r(std::size_t(hi + 2), 0.0);
        double best = 0.0;
        for (int lag = lo; lag <= hi + 1; ++lag)
        {
            double s = 0.0;
            double e = 0.0;
            for (std::size_t i = 0; i < len; ++i)
            {
                const double a = x[start + i];
                const double b = x[start + i + std::size_t(lag)];
                s += a * b;
                e += b * b;
            }
            r[std::size_t(lag)] = e > 0.0 ? s / std::sqrt(e) : 0.0;
            best = std::max(best, r[std::size_t(lag)]);
        }
        for (int lag = lo + 1; lag <= hi; ++lag)
        {
            const double v = r[std::size_t(lag)];
            if (v >= 0.85 * best && v >= r[std::size_t(lag - 1)] && v >= r[std::size_t(lag + 1)])
            {
                return double(rate) / lag;
            }
        }
        return 0.0;
    }

    rf::VoiceChain Only(int kind, int a, int b)
    {
        rf::VoiceChain chain = rf::DefaultVoiceChain();
        for (auto& s : chain)
        {
            if (s.kind == kind)
            {
                s.on = true;
                s.a = a;
                s.b = b;
            }
        }
        return chain;
    }

    bool Clean(const std::vector<std::int16_t>& x)
    {
        return !x.empty() && Rms(x) > 0.005;
    }
}

int main()
{
    const int rate = 22050;
    const auto voice = Voice(rate, 2.0, 160.0);
    Check(std::fabs(Fundamental(voice, rate) - 160.0) < 6.0, "test voice is 160 Hz");

    Check(rf::ProcessVoice(voice, rate, rf::DefaultVoiceChain()) == voice, "no effects: sound untouched");

    const auto up = rf::ProcessVoice(voice, rate, Only(rf::VoicePitch, 12, 0));
    const auto down = rf::ProcessVoice(voice, rate, Only(rf::VoicePitch, -5, 0));
    std::printf("     pitch up %.1f Hz, down %.1f Hz\n", Fundamental(up, rate), Fundamental(down, rate));
    Check(up.size() == voice.size() && std::fabs(Fundamental(up, rate) - 320.0) < 16.0, "pitch +12: octave higher, same length");
    Check(down.size() == voice.size() && std::fabs(Fundamental(down, rate) - 160.0 * std::pow(2.0, -5.0 / 12.0)) < 8.0, "pitch -5: lower, same length");

    const auto fast = rf::ProcessVoice(voice, rate, Only(rf::VoiceSpeed, 200, 0));
    Check(std::abs(int(fast.size()) - int(voice.size() / 2)) <= 2 && std::fabs(Fundamental(fast, rate) - 320.0) < 16.0, "speed 200%: half length, higher");

    const auto echo = rf::ProcessVoice(voice, rate, Only(rf::VoiceEcho, 250, 50));
    Check(echo.size() > voice.size() + std::size_t(rate / 4) && echo.size() < voice.size() + std::size_t(rate * 6), "echo adds a tail");
    const auto hall = rf::ProcessVoice(voice, rate, Only(rf::VoiceHall, 60, 40));
    Check(hall.size() > voice.size() && Rms(hall) > Rms(voice) * 0.5 && Rms(hall) < Rms(voice) * 2.5, "hall adds a tail, sane loudness");

    auto reversed = rf::ProcessVoice(voice, rate, Only(rf::VoiceReverse, 0, 0));
    std::reverse(reversed.begin(), reversed.end());
    Check(reversed == voice, "reverse plays from the end");

    const auto bits = rf::ProcessVoice(voice, rate, Only(rf::VoiceBits, 4, 4000));
    int distinct = 0;
    {
        std::vector<bool> seen(65536, false);
        for (auto v : bits)
        {
            if (!seen[std::size_t(v + 32768)])
            {
                seen[std::size_t(v + 32768)] = true;
                ++distinct;
            }
        }
    }
    Check(distinct <= 17 && bits.size() == voice.size(), "8 bit: few levels");

    const auto loud = rf::ProcessVoice(voice, rate, Only(rf::VoiceVolume, 400, 0));
    Check(std::fabs(Rms(loud) / Rms(voice) - 4.0) < 0.6, "volume 400%");

    bool allSane = true;
    for (int k = 0; k < rf::VoiceKindCount; ++k)
    {
        const rf::VoiceStep d = rf::DefaultVoiceStep(k);
        const auto out = rf::ProcessVoice(voice, rate, Only(k, d.a, d.b));
        const bool changed = out != voice;
        const bool ok = Clean(out) && changed && Rms(out) < 1.0;
        if (!ok)
        {
            std::printf("     effect %d (%ls): rms %.3f size %zu changed %d\n", k, rf::VoiceInfoOf(k).name, Rms(out), out.size(), int(changed));
        }
        allSane = allSane && ok;
    }
    Check(allSane, "every effect changes the sound and keeps it audible");

    bool presetsOk = true;
    for (const auto& p : rf::BuiltinVoicePresets())
    {
        presetsOk = presetsOk && int(p.chain.size()) == rf::VoiceKindCount && !p.name.empty();
        const auto out = rf::ProcessVoice(voice, rate, p.chain);
        presetsOk = presetsOk && (rf::VoiceEffectCount(p.chain) == 0 ? out == voice : Clean(out));
    }
    Check(presetsOk && rf::BuiltinVoicePresets().size() >= 10, "built-in presets work");

    const auto kidd = rf::BuiltinVoicePresets()[1];
    Check(kidd.name == L"Как c00lkidd" && kidd.chain[0].kind == rf::VoicePitch && kidd.chain[0].on && rf::VoiceSummary(kidd.chain) == L"Тон голоса, Металл, Искажение",
          "c00lkidd preset: pitch first, summary");

    rf::VoiceChain custom = rf::BuiltinVoicePresets()[2].chain;
    std::swap(custom[0], custom[3]);
    custom[5].a = 999;
    const rf::VoiceChain back = rf::ParseVoiceChain(rf::SerializeVoiceChain(custom));
    rf::VoiceChain expect = custom;
    rf::NormalizeVoiceChain(expect);
    bool same = back.size() == expect.size();
    for (std::size_t i = 0; same && i < back.size(); ++i)
    {
        same = back[i].kind == expect[i].kind && back[i].on == expect[i].on && back[i].a == expect[i].a && back[i].b == expect[i].b;
    }
    Check(same && back[5].a == rf::VoiceInfoOf(back[5].kind).p[0].max, "chain text round trip keeps order, clamps values");
    Check(rf::ParseVoiceChain("junk,1:1:x:2,99:1:1:1").size() == std::size_t(rf::VoiceKindCount), "broken chain text falls back to all effects");

    rf::AppSettings s = rf::DefaultSettings();
    s.voice = custom;
    s.micName = L"Микрофон (USB)";
    s.micQuality = 1;
    s.voicePresets.push_back({L"Мой голос", rf::BuiltinVoicePresets()[6].chain});
    s.recorder.format = 5;
    rf::AppSettings r = rf::DefaultSettings();
    rf::ParseSettings(rf::SerializeSettings(s), r);
    Check(rf::SerializeVoiceChain(r.voice) == rf::SerializeVoiceChain(expect) && r.micName == s.micName && r.micQuality == 1 && r.voicePresets.size() == 1 &&
              r.voicePresets[0].name == L"Мой голос" && rf::SerializeVoiceChain(r.voicePresets[0].chain) == rf::SerializeVoiceChain(s.voicePresets[0].chain) &&
              r.recorder.format == 5,
          "voice settings saved");

    for (int bitsIn : {8, 16})
    {
        Memory m(rf::EncodeWav(voice, rate, bitsIn));
        std::vector<std::int16_t> decoded;
        int rr = 0;
        int bb = 0;
        const bool ok = rf::DecodeWav(m, decoded, rr, bb);
        double err = 0.0;
        for (std::size_t i = 0; ok && i < decoded.size() && i < voice.size(); ++i)
        {
            err = std::max(err, std::fabs(double(decoded[i]) - voice[i]));
        }
        Check(ok && rr == rate && bb == bitsIn && decoded.size() == voice.size() && err <= (bitsIn == 8 ? 129.0 : 0.0),
              bitsIn == 8 ? "WAV 8 bit round trip" : "WAV 16 bit round trip");
    }
    Memory junk(std::vector<unsigned char>(100, 7));
    std::vector<std::int16_t> none;
    int jr = 0;
    int jb = 0;
    Check(!rf::DecodeWav(junk, none, jr, jb), "junk is not a WAV");

    const auto minute = Voice(44100, 20.0, 140.0);
    const auto t0 = std::chrono::steady_clock::now();
    const auto heavy = rf::ProcessVoice(minute, 44100, rf::BuiltinVoicePresets()[2].chain);
    const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("     JX1DX1 on 20 s at 44.1 kHz: %.2f s\n", took);
    Check(heavy.size() == minute.size() && took < 30.0, "a long recording is processed in reasonable time");

    const auto small = rf::ResampleVoice(voice, rate, 16000);
    Check(std::abs(int(small.size()) - int(voice.size() * 16000 / rate)) <= 1 && std::fabs(Fundamental(small, 16000) - 160.0) < 6.0, "resample keeps pitch");

    std::printf(failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 2;
}
