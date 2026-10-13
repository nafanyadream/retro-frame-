#include "voice.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace rf
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;

        const VoiceInfo Infos[VoiceKindCount] = {
            {L"Тон голоса", L"Выше — детский или мультяшный голос, ниже — грубый и низкий. Длина записи не меняется.", 1,
             {{L"Тон", -12, 12, 5, L""}, {L"", 0, 0, 0, L""}}},
            {L"Скорость", L"Как в меню «Эффекты» у «Звукозаписи»: быстрее — голос выше, медленнее — ниже.", 1,
             {{L"Скорость", 50, 300, 200, L"%"}, {L"", 0, 0, 0, L""}}},
            {L"Робот", L"Металлический голос робота: голос дрожит на частоте гула.", 2, {{L"Гул", 20, 300, 60, L" Гц"}, {L"Сила", 0, 100, 100, L"%"}}},
            {L"Металл", L"Звенящий голос, будто из трубы или жестяной банки.", 2, {{L"Звон", 0, 100, 60, L"%"}, {L"Сила", 0, 100, 60, L"%"}}},
            {L"Искажение", L"Хрип и перегруз, как у сломанного динамика.", 1, {{L"Сила", 0, 100, 50, L"%"}, {L"", 0, 0, 0, L""}}},
            {L"8 бит", L"Грубый «цифровой» звук старого компьютера.", 2, {{L"Разрядность", 2, 12, 6, L" бит"}, {L"Частота", 2000, 22050, 8000, L" Гц"}}},
            {L"Рация", L"Узкий звук с шипением, как по рации или телефону.", 1, {{L"Шипение", 0, 100, 30, L"%"}, {L"", 0, 0, 0, L""}}},
            {L"Глитч", L"Голос заикается, обрывается и прыгает.", 2, {{L"Сбои", 0, 100, 40, L"%"}, {L"Кусочек", 20, 200, 60, L" мс"}}},
            {L"Дрожь", L"Голос дрожит и плавает, как на зажёванной кассете.", 2, {{L"Скорость", 1, 15, 6, L" Гц"}, {L"Сила", 0, 100, 50, L"%"}}},
            {L"Хор", L"Будто говорят несколько голосов сразу.", 2, {{L"Голоса", 2, 6, 3, L""}, {L"Разброс", 0, 100, 50, L"%"}}},
            {L"Эхо", L"Голос повторяется, как пункт «Добавить эхо» в «Звукозаписи».", 2, {{L"Задержка", 50, 1000, 250, L" мс"}, {L"Повторы", 0, 90, 50, L"%"}}},
            {L"Зал", L"Голос в большом гулком зале или пещере.", 2, {{L"Размер", 0, 100, 60, L"%"}, {L"Сила", 0, 100, 40, L"%"}}},
            {L"Задом наперёд", L"Запись играет с конца, как пункт «Обратить» в «Звукозаписи».", 0, {{L"", 0, 0, 0, L""}, {L"", 0, 0, 0, L""}}},
            {L"Громкость", L"Тише или громче. Слишком громко — хрипит, как старые записи.", 1, {{L"Громкость", 10, 400, 150, L"%"}, {L"", 0, 0, 0, L""}}},
        };

        class Rng
        {
        public:
            explicit Rng(std::uint32_t seed) : s_(seed == 0 ? 1u : seed) {}

            std::uint32_t Next()
            {
                s_ ^= s_ << 13;
                s_ ^= s_ >> 17;
                s_ ^= s_ << 5;
                return s_;
            }

            double Unit() { return double(Next() >> 8) / double(1u << 24); }
            double Signed() { return Unit() * 2.0 - 1.0; }

        private:
            std::uint32_t s_;
        };

        class Biquad
        {
        public:
            static Biquad Low(double freq, double rate)
            {
                const double w = 2.0 * Pi * std::clamp(freq, 10.0, rate * 0.45) / rate;
                const double c = std::cos(w);
                const double alpha = std::sin(w) / (2.0 * 0.7071);
                return Biquad((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            }

            static Biquad High(double freq, double rate)
            {
                const double w = 2.0 * Pi * std::clamp(freq, 10.0, rate * 0.45) / rate;
                const double c = std::cos(w);
                const double alpha = std::sin(w) / (2.0 * 0.7071);
                return Biquad((1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            }

            double Run(double x)
            {
                const double y = b0_ * x + z1_;
                z1_ = b1_ * x - a1_ * y + z2_;
                z2_ = b2_ * x - a2_ * y;
                return y;
            }

        private:
            Biquad(double b0, double b1, double b2, double a0, double a1, double a2)
                : b0_(b0 / a0), b1_(b1 / a0), b2_(b2 / a0), a1_(a1 / a0), a2_(a2 / a0)
            {
            }

            double b0_;
            double b1_;
            double b2_;
            double a1_;
            double a2_;
            double z1_ = 0.0;
            double z2_ = 0.0;
        };

        using Wave = std::vector<float>;

        float At(const Wave& x, double pos)
        {
            if (pos < 0.0 || x.empty())
            {
                return 0.0f;
            }
            const std::size_t i = std::size_t(pos);
            if (i + 1 >= x.size())
            {
                return i < x.size() ? x[i] : 0.0f;
            }
            const float f = float(pos - double(i));
            return x[i] + (x[i + 1] - x[i]) * f;
        }

        void LowPass(Wave& x, double freq, int rate)
        {
            for (int pass = 0; pass < 2; ++pass)
            {
                Biquad f = Biquad::Low(freq, rate);
                for (float& v : x)
                {
                    v = float(f.Run(v));
                }
            }
        }

        Wave Respeed(Wave x, double k, int rate)
        {
            if (k > 1.0)
            {
                LowPass(x, 0.45 * rate / k, rate);
            }
            const std::size_t n = std::size_t(double(x.size()) / k);
            Wave out(n);
            for (std::size_t i = 0; i < n; ++i)
            {
                out[i] = At(x, double(i) * k);
            }
            return out;
        }

        Wave Stretch(const Wave& x, double alpha, int rate)
        {
            if (x.size() < 4)
            {
                return x;
            }
            const int frame = std::max(64, int(rate * 0.03)) & ~1;
            const int hop = frame / 2;
            const int tolerance = std::max(4, int(rate * 0.008));
            std::vector<double> window(std::size_t(frame), 0.0);
            for (int i = 0; i < frame; ++i)
            {
                window[std::size_t(i)] = 0.5 - 0.5 * std::cos(2.0 * Pi * i / frame);
            }
            const std::size_t total = std::size_t(std::llround(double(x.size()) * alpha));
            std::vector<double> out(total + std::size_t(frame) * 2, 0.0);
            std::vector<double> norm(out.size(), 0.0);
            const long long size = (long long)x.size();
            long long previous = 0;
            bool first = true;
            for (std::size_t o = 0; o < total + std::size_t(hop); o += std::size_t(hop))
            {
                const long long nominal = (long long)std::llround(double(o) / alpha);
                if (nominal >= size)
                {
                    break;
                }
                long long best = nominal;
                if (!first)
                {
                    const long long natural = previous + hop;
                    double bestScore = -1e300;
                    for (long long d = -tolerance; d <= tolerance; d += 2)
                    {
                        const long long cand = nominal + d;
                        if (cand < 0 || cand >= size)
                        {
                            continue;
                        }
                        double score = 0.0;
                        for (int i = 0; i < frame; i += 4)
                        {
                            const long long a = cand + i;
                            const long long b = natural + i;
                            if (a >= size || b >= size)
                            {
                                break;
                            }
                            score += double(x[std::size_t(a)]) * double(x[std::size_t(b)]);
                        }
                        if (score > bestScore)
                        {
                            bestScore = score;
                            best = cand;
                        }
                    }
                }
                for (int i = 0; i < frame; ++i)
                {
                    const long long src = best + i;
                    const double v = src < size ? double(x[std::size_t(src)]) : 0.0;
                    out[o + std::size_t(i)] += v * window[std::size_t(i)];
                    norm[o + std::size_t(i)] += window[std::size_t(i)];
                }
                previous = best;
                first = false;
            }
            Wave result(total);
            for (std::size_t i = 0; i < total; ++i)
            {
                result[i] = norm[i] > 1e-3 ? float(out[i] / norm[i]) : float(out[i]);
            }
            return result;
        }

        Wave Pitch(const Wave& x, int semitones, int rate)
        {
            if (semitones == 0)
            {
                return x;
            }
            const double k = std::pow(2.0, semitones / 12.0);
            Wave out = Respeed(Stretch(x, k, rate), k, rate);
            out.resize(x.size(), 0.0f);
            return out;
        }

        void Robot(Wave& x, int hz, int strength, int rate)
        {
            const double m = strength / 100.0;
            const double gain = 1.0 + 0.4 * m;
            for (std::size_t n = 0; n < x.size(); ++n)
            {
                const double ring = std::sin(2.0 * Pi * hz * double(n) / rate);
                x[n] = float(x[n] * ((1.0 - m) + m * ring) * gain);
            }
        }

        void Metal(Wave& x, int ring, int strength, int rate)
        {
            const double g = 0.3 + 0.65 * ring / 100.0;
            const double mix = strength / 100.0;
            const double base = 0.0025 * rate;
            const double swing = 0.0006 * rate;
            const double keep = std::sqrt(1.0 - g * g);
            Wave y(x.size(), 0.0f);
            for (std::size_t n = 0; n < x.size(); ++n)
            {
                const double d = base + swing * std::sin(2.0 * Pi * 0.3 * double(n) / rate);
                const double back = double(n) - d;
                y[n] = float(x[n] + g * At(y, back));
            }
            for (std::size_t n = 0; n < x.size(); ++n)
            {
                x[n] = float(x[n] * (1.0 - mix) + y[n] * keep * mix);
            }
        }

        void Distort(Wave& x, int strength)
        {
            const double drive = 1.0 + 0.4 * strength;
            const double norm = std::tanh(drive);
            for (float& v : x)
            {
                v = float(std::tanh(drive * v) / norm);
            }
        }

        void Crush(Wave& x, int bits, int freq, int rate)
        {
            const double step = std::max(1.0, double(rate) / std::max(1, freq));
            const double levels = std::pow(2.0, bits - 1);
            double acc = step;
            float held = 0.0f;
            for (float& v : x)
            {
                acc += 1.0;
                if (acc >= step)
                {
                    acc -= step;
                    held = v;
                }
                v = float(std::floor(held * levels + 0.5) / levels);
            }
        }

        void Radio(Wave& x, int hiss, int rate)
        {
            Biquad h1 = Biquad::High(400.0, rate);
            Biquad h2 = Biquad::High(400.0, rate);
            Biquad l1 = Biquad::Low(2800.0, rate);
            Biquad l2 = Biquad::Low(2800.0, rate);
            const double amount = hiss / 100.0;
            const double norm = std::tanh(3.0);
            Rng rng(0x5ADD10u);
            for (float& v : x)
            {
                double s = l2.Run(l1.Run(h2.Run(h1.Run(v))));
                s = std::tanh(3.0 * s) / norm * 0.9;
                s += rng.Signed() * 0.06 * amount;
                if (rng.Unit() < 0.0006 * amount)
                {
                    s += rng.Signed() * 0.4 * amount;
                }
                v = float(s);
            }
        }

        void Glitch(Wave& x, int amount, int pieceMs, int rate)
        {
            const double chance = amount / 100.0 * 0.55;
            const std::size_t piece = std::size_t(std::max(1, pieceMs * rate / 1000));
            const Wave src = x;
            Rng rng(0x6C1A55E5u);
            for (std::size_t pos = 0; pos < x.size(); pos += piece)
            {
                const std::size_t len = std::min(piece, x.size() - pos);
                if (rng.Unit() >= chance)
                {
                    continue;
                }
                const std::uint32_t type = rng.Next() % 4;
                const std::size_t slice = std::max<std::size_t>(1, len / (2 + rng.Next() % 3));
                for (std::size_t i = 0; i < len; ++i)
                {
                    float v = 0.0f;
                    if (type == 0)
                    {
                        v = src[pos + i % slice];
                    }
                    else if (type == 2)
                    {
                        v = src[pos + len - 1 - i];
                    }
                    else if (type == 3)
                    {
                        v = src[pos + (2 * i) % len];
                    }
                    x[pos + i] = v;
                }
            }
        }

        void Wobble(Wave& x, int hz, int strength, int rate)
        {
            const double depth = strength / 100.0 * 0.006 * rate;
            const double base = 0.001 * rate + depth;
            const Wave src = x;
            for (std::size_t n = 0; n < x.size(); ++n)
            {
                const double d = base + depth * std::sin(2.0 * Pi * hz * double(n) / rate);
                x[n] = At(src, double(n) - d);
            }
        }

        void Chorus(Wave& x, int voices, int spread, int rate)
        {
            const double s = spread / 100.0;
            const Wave src = x;
            const double norm = 1.0 + 0.55 * (voices - 1);
            for (std::size_t n = 0; n < x.size(); ++n)
            {
                double sum = src[n];
                for (int j = 1; j < voices; ++j)
                {
                    const double base = (0.010 + 0.007 * j) * rate;
                    const double depth = (0.002 + 0.004 * s) * rate;
                    const double lfo = 0.5 + 0.5 * std::sin(2.0 * Pi * (0.25 + 0.21 * j) * double(n) / rate + 1.7 * j);
                    sum += At(src, double(n) - base - depth * lfo);
                }
                x[n] = float(sum / norm);
            }
        }

        void Echo(Wave& x, int delayMs, int repeats, int rate)
        {
            const std::size_t d = std::size_t(std::max(1, delayMs * rate / 1000));
            const double g = std::max(0.05, repeats / 100.0);
            const double count = std::ceil(std::log(0.01) / std::log(g));
            const std::size_t tail = std::min(std::size_t(double(d) * count), std::size_t(6 * rate));
            const std::size_t n = x.size();
            x.resize(n + tail, 0.0f);
            for (std::size_t i = d; i < x.size(); ++i)
            {
                x[i] = float(x[i] + g * x[i - d]);
            }
        }

        void Hall(Wave& x, int size, int strength, int rate)
        {
            const double room = size / 100.0;
            const double wet = strength / 100.0;
            const double feedback = 0.70 + 0.28 * room;
            const double damp = 0.25;
            const double scale = double(rate) / 44100.0;
            const int combLens[] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
            const int passLens[] = {556, 441, 341, 225};
            std::vector<Wave> combs;
            std::vector<double> stores(8, 0.0);
            for (int len : combLens)
            {
                combs.emplace_back(std::size_t(std::max(1, int(len * scale))), 0.0f);
            }
            std::vector<Wave> passes;
            for (int len : passLens)
            {
                passes.emplace_back(std::size_t(std::max(1, int(len * scale))), 0.0f);
            }
            std::vector<std::size_t> combPos(combs.size(), 0);
            std::vector<std::size_t> passPos(passes.size(), 0);
            const std::size_t n = x.size();
            const std::size_t tail = std::size_t((0.4 + 2.6 * room) * rate);
            x.resize(n + tail, 0.0f);
            for (std::size_t i = 0; i < x.size(); ++i)
            {
                const double in = (i < n ? x[i] : 0.0) * 0.015;
                double sum = 0.0;
                for (std::size_t c = 0; c < combs.size(); ++c)
                {
                    Wave& buf = combs[c];
                    const double out = buf[combPos[c]];
                    stores[c] = out * (1.0 - damp) + stores[c] * damp;
                    buf[combPos[c]] = float(in + stores[c] * feedback);
                    combPos[c] = (combPos[c] + 1) % buf.size();
                    sum += out;
                }
                for (std::size_t a = 0; a < passes.size(); ++a)
                {
                    Wave& buf = passes[a];
                    const double held = buf[passPos[a]];
                    const double out = held - sum;
                    buf[passPos[a]] = float(sum + held * 0.5);
                    passPos[a] = (passPos[a] + 1) % buf.size();
                    sum = out;
                }
                x[i] = float(x[i] * (1.0 - 0.5 * wet) + sum * 3.0 * wet);
            }
        }

        void Apply(Wave& x, const VoiceStep& s, int rate)
        {
            switch (s.kind)
            {
            case VoicePitch:
                x = Pitch(x, s.a, rate);
                break;
            case VoiceSpeed:
                x = Respeed(std::move(x), s.a / 100.0, rate);
                break;
            case VoiceRobot:
                Robot(x, s.a, s.b, rate);
                break;
            case VoiceMetal:
                Metal(x, s.a, s.b, rate);
                break;
            case VoiceDistortion:
                Distort(x, s.a);
                break;
            case VoiceBits:
                Crush(x, s.a, s.b, rate);
                break;
            case VoiceRadio:
                Radio(x, s.a, rate);
                break;
            case VoiceGlitch:
                Glitch(x, s.a, s.b, rate);
                break;
            case VoiceWobble:
                Wobble(x, s.a, s.b, rate);
                break;
            case VoiceChorus:
                Chorus(x, s.a, s.b, rate);
                break;
            case VoiceEcho:
                Echo(x, s.a, s.b, rate);
                break;
            case VoiceHall:
                Hall(x, s.a, s.b, rate);
                break;
            case VoiceReverse:
                std::reverse(x.begin(), x.end());
                break;
            case VoiceVolume:
                for (float& v : x)
                {
                    v = float(v * s.a / 100.0);
                }
                break;
            default:
                break;
            }
        }

        VoicePreset Preset(const std::wstring& name, std::initializer_list<VoiceStep> steps)
        {
            VoicePreset p;
            p.name = name;
            for (VoiceStep s : steps)
            {
                s.on = true;
                p.chain.push_back(s);
            }
            NormalizeVoiceChain(p.chain);
            return p;
        }

        void Put16(std::vector<unsigned char>& out, unsigned v)
        {
            out.push_back((unsigned char)(v & 0xFF));
            out.push_back((unsigned char)((v >> 8) & 0xFF));
        }

        void Put32(std::vector<unsigned char>& out, unsigned v)
        {
            Put16(out, v & 0xFFFF);
            Put16(out, (v >> 16) & 0xFFFF);
        }

        unsigned Get16(const unsigned char* p)
        {
            return unsigned(p[0]) | (unsigned(p[1]) << 8);
        }

        unsigned Get32(const unsigned char* p)
        {
            return Get16(p) | (Get16(p + 2) << 16);
        }
    }

    const VoiceInfo& VoiceInfoOf(int kind)
    {
        return Infos[std::clamp(kind, 0, VoiceKindCount - 1)];
    }

    std::wstring VoiceValueText(int kind, int param, int value)
    {
        const VoiceInfo& info = VoiceInfoOf(kind);
        if (kind == VoicePitch)
        {
            return value > 0 ? L"+" + std::to_wstring(value) : value < 0 ? L"-" + std::to_wstring(-value) : std::wstring(L"0");
        }
        return std::to_wstring(value) + info.p[std::clamp(param, 0, 1)].unit;
    }

    VoiceStep DefaultVoiceStep(int kind)
    {
        const VoiceInfo& info = VoiceInfoOf(kind);
        VoiceStep s;
        s.kind = kind;
        s.a = info.params > 0 ? info.p[0].def : 0;
        s.b = info.params > 1 ? info.p[1].def : 0;
        return s;
    }

    VoiceChain DefaultVoiceChain()
    {
        VoiceChain chain;
        for (int k = 0; k < VoiceKindCount; ++k)
        {
            chain.push_back(DefaultVoiceStep(k));
        }
        return chain;
    }

    void NormalizeVoiceChain(VoiceChain& chain)
    {
        VoiceChain out;
        bool seen[VoiceKindCount] = {};
        for (VoiceStep s : chain)
        {
            if (s.kind < 0 || s.kind >= VoiceKindCount || seen[s.kind])
            {
                continue;
            }
            seen[s.kind] = true;
            const VoiceInfo& info = VoiceInfoOf(s.kind);
            s.a = info.params > 0 ? std::clamp(s.a, info.p[0].min, info.p[0].max) : 0;
            s.b = info.params > 1 ? std::clamp(s.b, info.p[1].min, info.p[1].max) : 0;
            out.push_back(s);
        }
        for (int k = 0; k < VoiceKindCount; ++k)
        {
            if (!seen[k])
            {
                out.push_back(DefaultVoiceStep(k));
            }
        }
        chain = std::move(out);
    }

    int VoiceEffectCount(const VoiceChain& chain)
    {
        return int(std::count_if(chain.begin(), chain.end(), [](const VoiceStep& s) { return s.on; }));
    }

    std::wstring VoiceSummary(const VoiceChain& chain)
    {
        std::wstring text;
        for (const VoiceStep& s : chain)
        {
            if (s.on)
            {
                text += (text.empty() ? L"" : L", ") + std::wstring(VoiceInfoOf(s.kind).name);
            }
        }
        return text.empty() ? std::wstring(L"выключены") : text;
    }

    std::string SerializeVoiceChain(const VoiceChain& chain)
    {
        std::ostringstream o;
        for (std::size_t i = 0; i < chain.size(); ++i)
        {
            const VoiceStep& s = chain[i];
            o << (i == 0 ? "" : ",") << s.kind << ":" << (s.on ? 1 : 0) << ":" << s.a << ":" << s.b;
        }
        return o.str();
    }

    VoiceChain ParseVoiceChain(const std::string& text)
    {
        VoiceChain chain;
        std::istringstream in(text);
        std::string item;
        while (std::getline(in, item, ','))
        {
            int v[4] = {0, 0, 0, 0};
            int count = 0;
            std::istringstream parts(item);
            std::string part;
            while (count < 4 && std::getline(parts, part, ':'))
            {
                try
                {
                    v[count++] = std::stoi(part);
                }
                catch (...)
                {
                    count = -1;
                    break;
                }
            }
            if (count == 4)
            {
                VoiceStep s;
                s.kind = v[0];
                s.on = v[1] != 0;
                s.a = v[2];
                s.b = v[3];
                chain.push_back(s);
            }
        }
        NormalizeVoiceChain(chain);
        return chain;
    }

    std::vector<VoicePreset> BuiltinVoicePresets()
    {
        return {
            Preset(L"Без эффектов", {}),
            Preset(L"Как c00lkidd", {{VoicePitch, true, 6, 0}, {VoiceMetal, true, 45, 50}, {VoiceDistortion, true, 20, 0}}),
            Preset(L"Как JX1DX1", {{VoicePitch, true, -2, 0}, {VoiceRobot, true, 70, 55}, {VoiceDistortion, true, 65, 0}, {VoiceBits, true, 6, 11025},
                                   {VoiceGlitch, true, 30, 60}}),
            Preset(L"Windows 98", {{VoiceVolume, true, 160, 0}, {VoiceBits, true, 8, 22050}}),
            Preset(L"Робот", {{VoiceRobot, true, 50, 100}, {VoiceMetal, true, 60, 40}}),
            Preset(L"Бурундук", {{VoicePitch, true, 10, 0}}),
            Preset(L"Демон", {{VoicePitch, true, -8, 0}, {VoiceDistortion, true, 35, 0}, {VoiceHall, true, 50, 35}}),
            Preset(L"Рация", {{VoiceRadio, true, 40, 0}}),
            Preset(L"Пещера", {{VoiceEcho, true, 350, 45}, {VoiceHall, true, 85, 50}}),
            Preset(L"Призрак", {{VoiceWobble, true, 4, 40}, {VoiceChorus, true, 4, 70}, {VoiceHall, true, 80, 60}}),
            Preset(L"Наоборот", {{VoiceReverse, true, 0, 0}, {VoiceHall, true, 60, 40}}),
            Preset(L"Ускорить, как в Windows", {{VoiceSpeed, true, 200, 0}}),
        };
    }

    std::vector<std::int16_t> ProcessVoice(const std::vector<std::int16_t>& samples, int rate, const VoiceChain& chain)
    {
        if (VoiceEffectCount(chain) == 0 || samples.empty() || rate <= 0)
        {
            return samples;
        }
        Wave x(samples.size());
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            x[i] = samples[i] / 32768.0f;
        }
        for (const VoiceStep& s : chain)
        {
            if (s.on)
            {
                Apply(x, s, rate);
            }
        }
        std::vector<std::int16_t> out(x.size());
        for (std::size_t i = 0; i < x.size(); ++i)
        {
            out[i] = std::int16_t(std::lround(std::clamp(double(x[i]), -1.0, 32767.0 / 32768.0) * 32768.0));
        }
        return out;
    }

    std::vector<std::int16_t> ResampleVoice(const std::vector<std::int16_t>& samples, int from, int to)
    {
        if (from == to || from <= 0 || to <= 0 || samples.empty())
        {
            return samples;
        }
        const double k = double(from) / to;
        const std::size_t n = std::size_t(double(samples.size()) / k);
        std::vector<std::int16_t> out(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const double pos = double(i) * k;
            const std::size_t j = std::size_t(pos);
            const double f = pos - double(j);
            const double a = samples[j];
            const double b = j + 1 < samples.size() ? samples[j + 1] : a;
            out[i] = std::int16_t(std::lround(a + (b - a) * f));
        }
        return out;
    }

    std::vector<unsigned char> EncodeWav(const std::vector<std::int16_t>& samples, int rate, int bits)
    {
        const bool eight = bits == 8;
        const unsigned bytes = unsigned(samples.size()) * (eight ? 1u : 2u);
        std::vector<unsigned char> out;
        out.reserve(44 + bytes);
        for (char c : std::string("RIFF"))
        {
            out.push_back((unsigned char)c);
        }
        Put32(out, 36 + bytes);
        for (char c : std::string("WAVEfmt "))
        {
            out.push_back((unsigned char)c);
        }
        Put32(out, 16);
        Put16(out, 1);
        Put16(out, 1);
        Put32(out, unsigned(rate));
        Put32(out, unsigned(rate) * (eight ? 1u : 2u));
        Put16(out, eight ? 1 : 2);
        Put16(out, eight ? 8 : 16);
        for (char c : std::string("data"))
        {
            out.push_back((unsigned char)c);
        }
        Put32(out, bytes);
        for (std::int16_t v : samples)
        {
            if (eight)
            {
                out.push_back((unsigned char)std::clamp(int(std::lround(v / 256.0)) + 128, 0, 255));
            }
            else
            {
                Put16(out, unsigned(std::uint16_t(v)));
            }
        }
        return out;
    }

    bool DecodeWav(ByteSource& source, std::vector<std::int16_t>& samples, int& rate, int& bits)
    {
        const std::uint64_t size = source.Size();
        if (size < 44 || size > 512ull * 1024 * 1024)
        {
            return false;
        }
        std::vector<unsigned char> data(static_cast<std::size_t>(size));
        if (!source.Read(0, data.data(), data.size()))
        {
            return false;
        }
        if (std::memcmp(data.data(), "RIFF", 4) != 0 || std::memcmp(data.data() + 8, "WAVE", 4) != 0)
        {
            return false;
        }
        unsigned format = 0;
        unsigned channels = 0;
        unsigned sampleRate = 0;
        unsigned sampleBits = 0;
        std::size_t pos = 12;
        while (pos + 8 <= data.size())
        {
            const unsigned char* chunk = data.data() + pos;
            const std::size_t len = Get32(chunk + 4);
            const std::size_t body = pos + 8;
            if (body + len > data.size() && std::memcmp(chunk, "data", 4) != 0)
            {
                return false;
            }
            if (std::memcmp(chunk, "fmt ", 4) == 0 && len >= 16)
            {
                format = Get16(data.data() + body);
                channels = Get16(data.data() + body + 2);
                sampleRate = Get32(data.data() + body + 4);
                sampleBits = Get16(data.data() + body + 14);
                if (format == 0xFFFE && len >= 26)
                {
                    format = Get16(data.data() + body + 24);
                }
            }
            else if (std::memcmp(chunk, "data", 4) == 0)
            {
                if (format != 1 || channels < 1 || channels > 2 || sampleRate < 1000 || sampleRate > 384000 || (sampleBits != 8 && sampleBits != 16))
                {
                    return false;
                }
                const std::size_t avail = std::min(len, data.size() - body);
                const std::size_t width = sampleBits / 8 * channels;
                const std::size_t frames = avail / width;
                samples.assign(frames, 0);
                for (std::size_t i = 0; i < frames; ++i)
                {
                    int sum = 0;
                    for (unsigned c = 0; c < channels; ++c)
                    {
                        const unsigned char* p = data.data() + body + i * width + c * (sampleBits / 8);
                        sum += sampleBits == 8 ? (int(p[0]) - 128) * 256 : int(std::int16_t(std::uint16_t(Get16(p))));
                    }
                    samples[i] = std::int16_t(sum / int(channels));
                }
                rate = int(sampleRate);
                bits = int(sampleBits);
                return true;
            }
            pos = body + len + (len & 1);
        }
        return false;
    }
}
