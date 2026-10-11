#include "settings.h"

#include "utf.h"

#include <cstdio>
#include <map>
#include <sstream>

namespace rf
{
    namespace
    {
        std::string Hex(Color c)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", Red(c), Green(c), Blue(c));
            return buf;
        }

        bool ParseHex(const std::string& s, Color& out)
        {
            if (s.size() != 7 || s[0] != '#')
            {
                return false;
            }
            unsigned v = 0;
            for (std::size_t i = 1; i < 7; ++i)
            {
                const char c = s[i];
                v <<= 4;
                if (c >= '0' && c <= '9')
                {
                    v |= unsigned(c - '0');
                }
                else if (c >= 'A' && c <= 'F')
                {
                    v |= unsigned(c - 'A' + 10);
                }
                else if (c >= 'a' && c <= 'f')
                {
                    v |= unsigned(c - 'a' + 10);
                }
                else
                {
                    return false;
                }
            }
            out = 0xFF000000u | v;
            return true;
        }

        std::string Escape(const std::wstring& w)
        {
            std::string s = ToUtf8(w);
            std::string out;
            for (char c : s)
            {
                if (c == '\\')
                {
                    out += "\\\\";
                }
                else if (c == '|')
                {
                    out += "\\p";
                }
                else if (c == '\n')
                {
                    out += "\\n";
                }
                else if (c == '\r')
                {
                    continue;
                }
                else
                {
                    out.push_back(c);
                }
            }
            return out;
        }

        std::wstring Unescape(const std::string& s)
        {
            std::string out;
            for (std::size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] == '\\' && i + 1 < s.size())
                {
                    const char n = s[++i];
                    out.push_back(n == 'p' ? '|' : n == 'n' ? '\n' : n);
                }
                else
                {
                    out.push_back(s[i]);
                }
            }
            return FromUtf8(out);
        }

        std::string EscapeList(const std::vector<std::wstring>& items)
        {
            std::string out;
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                if (i > 0)
                {
                    out.push_back('|');
                }
                out += Escape(items[i]);
            }
            return out;
        }

        std::vector<std::wstring> UnescapeList(const std::string& s)
        {
            std::vector<std::wstring> out;
            std::string cur;
            for (std::size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] == '\\' && i + 1 < s.size())
                {
                    cur.push_back(s[i]);
                    cur.push_back(s[++i]);
                }
                else if (s[i] == '|')
                {
                    out.push_back(Unescape(cur));
                    cur.clear();
                }
                else
                {
                    cur.push_back(s[i]);
                }
            }
            out.push_back(Unescape(cur));
            return out;
        }

        int ToInt(const std::string& s, int fallback)
        {
            try
            {
                std::size_t used = 0;
                const int v = std::stoi(s, &used);
                return used > 0 ? v : fallback;
            }
            catch (...)
            {
                return fallback;
            }
        }
    }

    AppSettings DefaultSettings()
    {
        AppSettings s;
        s.frame.style = Style::Win98;
        s.frame.colors = SchemeColors(0, Style::Win98);
        return s;
    }

    std::string SerializeSettings(const AppSettings& s)
    {
        std::ostringstream o;
        const FrameSettings& f = s.frame;
        const FrameColors& c = f.colors;
        o << "version=1\n";
        o << "style=" << int(f.style) << "\n";
        o << "kind=" << int(f.kind) << "\n";
        o << "active=" << (f.active ? 1 : 0) << "\n";
        o << "face=" << Hex(c.face) << "\n";
        o << "active1=" << Hex(c.active1) << "\n";
        o << "active2=" << Hex(c.active2) << "\n";
        o << "inactive1=" << Hex(c.inactive1) << "\n";
        o << "inactive2=" << Hex(c.inactive2) << "\n";
        o << "titleText=" << Hex(c.titleText) << "\n";
        o << "inactiveText=" << Hex(c.inactiveText) << "\n";
        o << "text=" << Hex(c.text) << "\n";
        o << "window=" << Hex(c.window) << "\n";
        o << "select=" << Hex(c.select) << "\n";
        o << "menu=" << (f.menu ? 1 : 0) << "\n";
        o << "menuItems=" << EscapeList(f.menuItems) << "\n";
        o << "status=" << (f.status ? 1 : 0) << "\n";
        o << "statusFields=" << EscapeList(f.statusFields) << "\n";
        o << "statusIcon=" << (f.statusIcon ? 1 : 0) << "\n";
        o << "grip=" << (f.grip ? 1 : 0) << "\n";
        o << "address=" << (f.address ? 1 : 0) << "\n";
        o << "addressText=" << Escape(f.addressText) << "\n";
        o << "toolbar=" << (f.toolbar ? 1 : 0) << "\n";
        o << "corner=" << (f.corner ? 1 : 0) << "\n";
        o << "sunken=" << (f.sunken ? 1 : 0) << "\n";
        o << "scale=" << f.scale << "\n";
        o << "scheme=" << s.scheme << "\n";
        o << "icon=" << s.iconIndex << "\n";
        o << "titleFromName=" << (s.titleFromName ? 1 : 0) << "\n";
        o << "customTitle=" << Escape(s.customTitle) << "\n";
        o << "format=" << s.format << "\n";
        o << "openFolder=" << Escape(s.openFolder) << "\n";
        o << "saveFolder=" << Escape(s.saveFolder) << "\n";
        o << "windowX=" << s.windowX << "\n";
        o << "windowY=" << s.windowY << "\n";
        return o.str();
    }

    void ParseSettings(const std::string& text, AppSettings& s)
    {
        std::map<std::string, std::string> kv;
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            const std::size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }
            kv[line.substr(0, eq)] = line.substr(eq + 1);
        }
        const auto get = [&](const char* key) -> const std::string* {
            const auto it = kv.find(key);
            return it == kv.end() ? nullptr : &it->second;
        };
        const auto getInt = [&](const char* key, int& target, int lo, int hi) {
            if (const std::string* v = get(key))
            {
                target = std::clamp(ToInt(*v, target), lo, hi);
            }
        };
        const auto getBool = [&](const char* key, bool& target) {
            if (const std::string* v = get(key))
            {
                target = *v == "1";
            }
        };
        const auto getColor = [&](const char* key, Color& target) {
            if (const std::string* v = get(key))
            {
                Color c;
                if (ParseHex(*v, c))
                {
                    target = c;
                }
            }
        };
        FrameSettings& f = s.frame;
        int style = int(f.style);
        getInt("style", style, 0, 2);
        f.style = static_cast<Style>(style);
        int kind = int(f.kind);
        getInt("kind", kind, 0, 3);
        f.kind = static_cast<WindowKind>(kind);
        getBool("active", f.active);
        getColor("face", f.colors.face);
        getColor("active1", f.colors.active1);
        getColor("active2", f.colors.active2);
        getColor("inactive1", f.colors.inactive1);
        getColor("inactive2", f.colors.inactive2);
        getColor("titleText", f.colors.titleText);
        getColor("inactiveText", f.colors.inactiveText);
        getColor("text", f.colors.text);
        getColor("window", f.colors.window);
        getColor("select", f.colors.select);
        getBool("menu", f.menu);
        if (const std::string* v = get("menuItems"))
        {
            f.menuItems = UnescapeList(*v);
        }
        getBool("status", f.status);
        if (const std::string* v = get("statusFields"))
        {
            f.statusFields = UnescapeList(*v);
            f.statusFields.resize(3);
        }
        getBool("statusIcon", f.statusIcon);
        getBool("grip", f.grip);
        getBool("address", f.address);
        if (const std::string* v = get("addressText"))
        {
            f.addressText = Unescape(*v);
        }
        getBool("toolbar", f.toolbar);
        getBool("corner", f.corner);
        getBool("sunken", f.sunken);
        getInt("scale", f.scale, 0, 4);
        getInt("scheme", s.scheme, -1, SchemeCount() - 1);
        getInt("icon", s.iconIndex, NoIconIndex, CustomIconIndex);
        getBool("titleFromName", s.titleFromName);
        if (const std::string* v = get("customTitle"))
        {
            s.customTitle = Unescape(*v);
        }
        getInt("format", s.format, 0, 2);
        if (const std::string* v = get("openFolder"))
        {
            s.openFolder = Unescape(*v);
        }
        if (const std::string* v = get("saveFolder"))
        {
            s.saveFolder = Unescape(*v);
        }
        getInt("windowX", s.windowX, -100000, 100000);
        getInt("windowY", s.windowY, -100000, 100000);
    }
}
