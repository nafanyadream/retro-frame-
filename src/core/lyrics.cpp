#include "lyrics.h"

#include "utf.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace rf
{
    namespace
    {
        const wchar_t Cp1251High[128] = {
            0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
            0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x003F, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
            0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
            0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
            0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
            0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
            0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
            0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F};

        bool ValidUtf8(const std::string& s)
        {
            std::size_t i = 0;
            while (i < s.size())
            {
                const unsigned char c = static_cast<unsigned char>(s[i]);
                int extra = 0;
                if (c < 0x80)
                {
                    ++i;
                    continue;
                }
                if ((c & 0xE0) == 0xC0 && c >= 0xC2)
                {
                    extra = 1;
                }
                else if ((c & 0xF0) == 0xE0)
                {
                    extra = 2;
                }
                else if ((c & 0xF8) == 0xF0 && c <= 0xF4)
                {
                    extra = 3;
                }
                else
                {
                    return false;
                }
                if (i + std::size_t(extra) >= s.size())
                {
                    return false;
                }
                for (int k = 1; k <= extra; ++k)
                {
                    if ((static_cast<unsigned char>(s[i + std::size_t(k)]) & 0xC0) != 0x80)
                    {
                        return false;
                    }
                }
                i += std::size_t(extra) + 1;
            }
            return true;
        }

        int Digits(const std::wstring& s, std::size_t& i, int maxDigits, int& value)
        {
            int n = 0;
            value = 0;
            while (i < s.size() && n < maxDigits && s[i] >= L'0' && s[i] <= L'9')
            {
                value = value * 10 + int(s[i] - L'0');
                ++i;
                ++n;
            }
            return n;
        }

        bool ParseStamp(const std::wstring& tag, double& seconds)
        {
            std::size_t i = 0;
            int minutes = 0;
            int secs = 0;
            if (Digits(tag, i, 3, minutes) == 0 || i >= tag.size() || tag[i] != L':')
            {
                return false;
            }
            ++i;
            if (Digits(tag, i, 2, secs) == 0)
            {
                return false;
            }
            double fraction = 0.0;
            if (i < tag.size() && (tag[i] == L'.' || tag[i] == L':'))
            {
                ++i;
                double scale = 0.1;
                int n = 0;
                while (i < tag.size() && tag[i] >= L'0' && tag[i] <= L'9' && n < 3)
                {
                    fraction += int(tag[i] - L'0') * scale;
                    scale *= 0.1;
                    ++i;
                    ++n;
                }
            }
            if (i != tag.size())
            {
                return false;
            }
            seconds = minutes * 60.0 + secs + fraction;
            return true;
        }

        std::wstring StripWordStamps(const std::wstring& text)
        {
            std::wstring out;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] == L'<')
                {
                    const std::size_t close = text.find(L'>', i);
                    double ignored = 0.0;
                    if (close != std::wstring::npos && ParseStamp(text.substr(i + 1, close - i - 1), ignored))
                    {
                        i = close;
                        continue;
                    }
                }
                out.push_back(text[i]);
            }
            return Trim(out);
        }
    }

    std::wstring DecodeText(const std::string& bytes)
    {
        if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE)
        {
            std::wstring out;
            for (std::size_t i = 2; i + 1 < bytes.size(); i += 2)
            {
                const wchar_t c = wchar_t(static_cast<unsigned char>(bytes[i]) | (static_cast<unsigned char>(bytes[i + 1]) << 8));
                if (c < 0xD800 || c > 0xDFFF)
                {
                    out.push_back(c);
                }
            }
            return out;
        }
        std::string s = bytes;
        if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF)
        {
            s.erase(0, 3);
        }
        if (ValidUtf8(s))
        {
            return FromUtf8(s);
        }
        std::wstring out;
        out.reserve(s.size());
        for (char ch : s)
        {
            const unsigned char c = static_cast<unsigned char>(ch);
            out.push_back(c < 0x80 ? wchar_t(c) : Cp1251High[c - 0x80]);
        }
        return out;
    }

    std::vector<LyricLine> ParseLyrics(const std::string& bytes, double duration)
    {
        const std::wstring text = DecodeText(bytes);
        std::vector<LyricLine> timed;
        std::vector<std::wstring> plain;
        double offset = 0.0;
        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t end = text.find(L'\n', start);
            if (end == std::wstring::npos)
            {
                end = text.size();
            }
            std::wstring line = text.substr(start, end - start);
            start = end + 1;
            if (!line.empty() && line.back() == L'\r')
            {
                line.pop_back();
            }
            std::vector<double> stamps;
            bool tagged = false;
            std::size_t i = 0;
            while (i < line.size() && line[i] == L' ')
            {
                ++i;
            }
            while (i < line.size() && line[i] == L'[')
            {
                const std::size_t close = line.find(L']', i);
                if (close == std::wstring::npos)
                {
                    break;
                }
                const std::wstring tag = line.substr(i + 1, close - i - 1);
                double t = 0.0;
                if (ParseStamp(tag, t))
                {
                    stamps.push_back(t);
                }
                else if (ToLower(tag).rfind(L"offset:", 0) == 0)
                {
                    offset = std::wcstod(Trim(tag.substr(7)).c_str(), nullptr) / 1000.0;
                }
                tagged = true;
                i = close + 1;
            }
            const std::wstring rest = StripWordStamps(line.substr(i));
            if (!stamps.empty())
            {
                for (double t : stamps)
                {
                    timed.push_back(LyricLine{t, rest});
                }
            }
            else if (!tagged && !rest.empty())
            {
                plain.push_back(rest);
            }
            if (end == text.size())
            {
                break;
            }
        }
        if (!timed.empty())
        {
            for (auto& l : timed)
            {
                l.time = std::max(0.0, l.time - offset);
            }
            std::stable_sort(timed.begin(), timed.end(), [](const LyricLine& a, const LyricLine& b) { return a.time < b.time; });
            return timed;
        }
        std::vector<LyricLine> spread;
        const double step = plain.empty() ? 0.0 : std::max(0.5, duration) / double(plain.size());
        for (std::size_t k = 0; k < plain.size(); ++k)
        {
            spread.push_back(LyricLine{step * double(k), plain[k]});
        }
        return spread;
    }

    int LyricIndex(const std::vector<LyricLine>& lines, double seconds)
    {
        const auto it = std::upper_bound(lines.begin(), lines.end(), seconds + 1e-9, [](double t, const LyricLine& l) { return t < l.time; });
        return int(it - lines.begin()) - 1;
    }

    bool IsLyricsFile(const std::wstring& path)
    {
        const std::wstring name = ToLower(FileNameOf(path));
        const std::size_t dot = name.rfind(L'.');
        const std::wstring ext = dot == std::wstring::npos ? std::wstring() : name.substr(dot);
        return ext == L".lrc" || ext == L".txt";
    }
}
