#include "utf.h"

#include <cwctype>

namespace rf
{
    namespace
    {
        void Append(std::string& out, unsigned cp)
        {
            if (cp < 0x80)
            {
                out.push_back(char(cp));
            }
            else if (cp < 0x800)
            {
                out.push_back(char(0xC0 | (cp >> 6)));
                out.push_back(char(0x80 | (cp & 0x3F)));
            }
            else if (cp < 0x10000)
            {
                out.push_back(char(0xE0 | (cp >> 12)));
                out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(char(0x80 | (cp & 0x3F)));
            }
            else
            {
                out.push_back(char(0xF0 | (cp >> 18)));
                out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(char(0x80 | (cp & 0x3F)));
            }
        }

        void AppendWide(std::wstring& out, unsigned cp)
        {
            if (sizeof(wchar_t) == 2 && cp >= 0x10000)
            {
                cp -= 0x10000;
                out.push_back(wchar_t(0xD800 + (cp >> 10)));
                out.push_back(wchar_t(0xDC00 + (cp & 0x3FF)));
            }
            else
            {
                out.push_back(wchar_t(cp));
            }
        }

        bool IsSep(wchar_t c)
        {
            return c == L'\\' || c == L'/';
        }
    }

    std::string ToUtf8(const std::wstring& s)
    {
        std::string out;
        out.reserve(s.size());
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            unsigned cp = unsigned(s[i]);
            if (sizeof(wchar_t) == 2 && cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size())
            {
                const unsigned lo = unsigned(s[i + 1]);
                if (lo >= 0xDC00 && lo <= 0xDFFF)
                {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    ++i;
                }
            }
            Append(out, cp);
        }
        return out;
    }

    std::wstring FromUtf8(const std::string& s)
    {
        std::wstring out;
        out.reserve(s.size());
        std::size_t i = 0;
        while (i < s.size())
        {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            unsigned cp = 0;
            int extra = 0;
            if (c < 0x80)
            {
                cp = c;
            }
            else if ((c & 0xE0) == 0xC0)
            {
                cp = c & 0x1F;
                extra = 1;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                cp = c & 0x0F;
                extra = 2;
            }
            else if ((c & 0xF8) == 0xF0)
            {
                cp = c & 0x07;
                extra = 3;
            }
            else
            {
                ++i;
                continue;
            }
            bool ok = true;
            for (int k = 1; k <= extra; ++k)
            {
                if (i + std::size_t(k) >= s.size())
                {
                    ok = false;
                    break;
                }
                const unsigned char cc = static_cast<unsigned char>(s[i + std::size_t(k)]);
                if ((cc & 0xC0) != 0x80)
                {
                    ok = false;
                    break;
                }
                cp = (cp << 6) | (cc & 0x3F);
            }
            i += std::size_t(extra) + 1;
            if (ok)
            {
                AppendWide(out, cp);
            }
        }
        return out;
    }

    std::wstring FileNameOf(const std::wstring& path)
    {
        std::size_t p = path.size();
        while (p > 0 && !IsSep(path[p - 1]))
        {
            --p;
        }
        return path.substr(p);
    }

    std::wstring StemOf(const std::wstring& name)
    {
        const std::size_t dot = name.rfind(L'.');
        if (dot == std::wstring::npos || dot == 0)
        {
            return name;
        }
        return name.substr(0, dot);
    }

    std::wstring DirectoryOf(const std::wstring& path)
    {
        std::size_t p = path.size();
        while (p > 0 && !IsSep(path[p - 1]))
        {
            --p;
        }
        if (p == 0)
        {
            return std::wstring();
        }
        return path.substr(0, p - 1);
    }

    std::wstring JoinPath(const std::wstring& dir, const std::wstring& name)
    {
        if (dir.empty())
        {
            return name;
        }
        if (IsSep(dir.back()))
        {
            return dir + name;
        }
        return dir + L"\\" + name;
    }

    std::wstring Trim(const std::wstring& s)
    {
        std::size_t a = 0;
        std::size_t b = s.size();
        while (a < b && std::iswspace(s[a]))
        {
            ++a;
        }
        while (b > a && std::iswspace(s[b - 1]))
        {
            --b;
        }
        return s.substr(a, b - a);
    }

    std::vector<std::wstring> SplitList(const std::wstring& s, wchar_t sep)
    {
        std::vector<std::wstring> out;
        std::wstring cur;
        for (wchar_t c : s)
        {
            if (c == sep)
            {
                out.push_back(Trim(cur));
                cur.clear();
            }
            else
            {
                cur.push_back(c);
            }
        }
        out.push_back(Trim(cur));
        return out;
    }

    std::wstring JoinList(const std::vector<std::wstring>& items, const std::wstring& sep)
    {
        std::wstring out;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (i > 0)
            {
                out += sep;
            }
            out += items[i];
        }
        return out;
    }

    std::wstring ToLower(const std::wstring& s)
    {
        std::wstring out = s;
        for (auto& c : out)
        {
            if (c >= L'A' && c <= L'Z')
            {
                c = wchar_t(c - L'A' + L'a');
            }
            else if (c >= 0x410 && c <= 0x42F)
            {
                c = wchar_t(c + 0x20);
            }
            else if (c == 0x401)
            {
                c = 0x451;
            }
        }
        return out;
    }
}
