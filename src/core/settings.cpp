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

    ModeLook DefaultLook(int mode)
    {
        ModeLook look;
        if (mode == VideoMode)
        {
            look.iconIndex = 8;
            look.customTitle = L"Мой ролик";
            look.menu = true;
            look.status = true;
        }
        else if (mode == MusicMode)
        {
            look.iconIndex = NoIconIndex;
            look.titleFromName = false;
            look.customTitle = L"Песня";
            look.kind = WindowKind::Bare;
        }
        else
        {
            look.customTitle = L"Моё фото";
        }
        return look;
    }

    AppSettings DefaultSettings()
    {
        AppSettings s;
        s.frame.style = Style::Win98;
        s.frame.colors = SchemeColors(0, Style::Win98);
        for (int m = 0; m < 3; ++m)
        {
            s.looks[std::size_t(m)] = DefaultLook(m);
        }
        return s;
    }

    std::string SerializeSettings(const AppSettings& s)
    {
        std::ostringstream o;
        const FrameSettings& f = s.frame;
        const FrameColors& c = f.colors;
        o << "version=3\n";
        o << "mode=" << s.mode << "\n";
        o << "style=" << int(f.style) << "\n";
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
        o << "menuItems=" << EscapeList(f.menuItems) << "\n";
        o << "statusFields=" << EscapeList(f.statusFields) << "\n";
        o << "statusIcon=" << (f.statusIcon ? 1 : 0) << "\n";
        o << "grip=" << (f.grip ? 1 : 0) << "\n";
        o << "addressText=" << Escape(f.addressText) << "\n";
        o << "sunken=" << (f.sunken ? 1 : 0) << "\n";
        o << "scale=" << f.scale << "\n";
        o << "scheme=" << s.scheme << "\n";
        const char* prefixes[] = {"", "video.", "music."};
        for (int m = 0; m < 3; ++m)
        {
            const ModeLook& l = s.looks[std::size_t(m)];
            const std::string p = prefixes[m];
            o << p << "icon=" << l.iconIndex << "\n";
            o << p << "titleFromName=" << (l.titleFromName ? 1 : 0) << "\n";
            o << p << "customTitle=" << Escape(l.customTitle) << "\n";
            o << p << "kind=" << int(l.kind) << "\n";
            o << p << "menu=" << (l.menu ? 1 : 0) << "\n";
            o << p << "status=" << (l.status ? 1 : 0) << "\n";
            o << p << "toolbar=" << (l.toolbar ? 1 : 0) << "\n";
            o << p << "address=" << (l.address ? 1 : 0) << "\n";
            o << p << "corner=" << (l.corner ? 1 : 0) << "\n";
        }
        o << "playerMenu=" << EscapeList(s.playerMenu) << "\n";
        o << "playerStatus=" << Escape(s.playerStatus) << "\n";
        o << "playerTime=" << (s.playerTime ? 1 : 0) << "\n";
        o << "video.format=" << s.video.format << "\n";
        o << "video.square=" << (s.video.square ? 1 : 0) << "\n";
        o << "video.canvas=" << s.video.canvas << "\n";
        o << "video.gifFps=" << s.video.gifFps << "\n";
        o << "video.gifWidth=" << s.video.gifWidth << "\n";
        o << "video.fps=" << s.video.fps << "\n";
        o << "music.format=" << s.music.format << "\n";
        o << "music.square=" << (s.music.square ? 1 : 0) << "\n";
        o << "music.canvas=" << s.music.canvas << "\n";
        o << "music.fade=" << s.music.fade << "\n";
        o << "music.spin=" << (s.music.spin ? 1 : 0) << "\n";
        o << "music.meter=" << (s.music.meter ? 1 : 0) << "\n";
        o << "music.fps=" << s.music.fps << "\n";
        o << "music.equalizer=" << (s.music.equalizer ? 1 : 0) << "\n";
        o << "fx.intro=" << (s.effects.intro ? 1 : 0) << "\n";
        o << "fx.cursor=" << s.effects.cursor << "\n";
        o << "fx.cursorPlace=" << s.effects.cursorPlace << "\n";
        o << "fx.popups=" << s.effects.popups << "\n";
        o << "fx.popupIcon=" << s.effects.popupIcon << "\n";
        o << "fx.popupButtons=" << s.effects.popupButtons << "\n";
        o << "fx.popupTitle=" << Escape(s.effects.popupTitle) << "\n";
        o << "fx.popupText=" << Escape(s.effects.popupText) << "\n";
        o << "fx.screen=" << s.effects.screen << "\n";
        o << "background=" << Hex(s.background) << "\n";
        o << "backgroundPicture=" << (s.backgroundPicture ? 1 : 0) << "\n";
        o << "format=" << s.format << "\n";
        o << "openFolder=" << Escape(s.openFolder) << "\n";
        o << "saveFolder=" << Escape(s.saveFolder) << "\n";
        o << "videoFolder=" << Escape(s.videoFolder) << "\n";
        o << "musicFolder=" << Escape(s.musicFolder) << "\n";
        o << "libraryFolder=" << Escape(s.libraryFolder) << "\n";
        o << "library=" << EscapeList(s.library) << "\n";
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
        const auto get = [&](const std::string& key) -> const std::string* {
            const auto it = kv.find(key);
            return it == kv.end() ? nullptr : &it->second;
        };
        const auto getInt = [&](const std::string& key, int& target, int lo, int hi) {
            if (const std::string* v = get(key))
            {
                target = std::clamp(ToInt(*v, target), lo, hi);
            }
        };
        const auto getBool = [&](const std::string& key, bool& target) {
            if (const std::string* v = get(key))
            {
                target = *v == "1";
            }
        };
        const auto getColor = [&](const std::string& key, Color& target) {
            if (const std::string* v = get(key))
            {
                Color c;
                if (ParseHex(*v, c))
                {
                    target = c;
                }
            }
        };
        const auto getText = [&](const std::string& key, std::wstring& target) {
            if (const std::string* v = get(key))
            {
                target = Unescape(*v);
            }
        };
        const auto getList = [&](const std::string& key, std::vector<std::wstring>& target) {
            if (const std::string* v = get(key))
            {
                target.clear();
                for (const auto& item : UnescapeList(*v))
                {
                    if (!item.empty())
                    {
                        target.push_back(item);
                    }
                }
            }
        };
        FrameSettings& f = s.frame;
        getInt("mode", s.mode, 0, 2);
        int style = int(f.style);
        getInt("style", style, 0, 2);
        f.style = static_cast<Style>(style);
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
        if (const std::string* v = get("menuItems"))
        {
            f.menuItems = UnescapeList(*v);
        }
        if (const std::string* v = get("statusFields"))
        {
            f.statusFields = UnescapeList(*v);
            f.statusFields.resize(3);
        }
        getBool("statusIcon", f.statusIcon);
        getBool("grip", f.grip);
        getText("addressText", f.addressText);
        getBool("sunken", f.sunken);
        getInt("scale", f.scale, 0, 4);
        getInt("scheme", s.scheme, -1, SchemeCount() - 1);
        const char* prefixes[] = {"", "video.", "music."};
        for (int m = 0; m < 3; ++m)
        {
            ModeLook& l = s.looks[std::size_t(m)];
            const std::string p = prefixes[m];
            getInt(p + "icon", l.iconIndex, NoIconIndex, CustomIconIndex);
            getBool(p + "titleFromName", l.titleFromName);
            getText(p + "customTitle", l.customTitle);
            int kind = int(l.kind);
            getInt(p + "kind", kind, 0, 3);
            l.kind = static_cast<WindowKind>(kind);
            getBool(p + "menu", l.menu);
            getBool(p + "status", l.status);
            getBool(p + "toolbar", l.toolbar);
            getBool(p + "address", l.address);
            getBool(p + "corner", l.corner);
        }
        int version = 1;
        getInt("version", version, 1, 1000);
        if (version < 3)
        {
            s.looks[MusicMode] = DefaultLook(MusicMode);
        }
        getList("playerMenu", s.playerMenu);
        getText("playerStatus", s.playerStatus);
        getBool("playerTime", s.playerTime);
        getInt("video.format", s.video.format, 0, 3);
        getBool("video.square", s.video.square);
        const auto getCanvas = [&](const std::string& prefix, int& target) {
            if (get(prefix + "canvas") != nullptr)
            {
                getInt(prefix + "canvas", target, 0, CanvasCount - 1);
                return;
            }
            int old = 0;
            getInt(prefix + "squareSize", old, 0, 10000);
            if (old == 720)
            {
                target = 1;
            }
        };
        getCanvas("video.", s.video.canvas);
        getInt("video.gifFps", s.video.gifFps, 5, 50);
        getInt("video.gifWidth", s.video.gifWidth, 0, 4000);
        getInt("video.fps", s.video.fps, 0, 60);
        getInt("music.format", s.music.format, 0, 3);
        getBool("music.square", s.music.square);
        getCanvas("music.", s.music.canvas);
        getInt("music.fade", s.music.fade, 0, 30);
        getBool("music.spin", s.music.spin);
        getBool("music.meter", s.music.meter);
        getBool("music.equalizer", s.music.equalizer);
        getInt("music.fps", s.music.fps, 10, 60);
        const auto listed = [](int fps) {
            for (int choice : FpsChoices)
            {
                if (choice == fps)
                {
                    return true;
                }
            }
            return false;
        };
        if (s.video.fps != 0 && !listed(s.video.fps))
        {
            s.video.fps = 0;
        }
        if (!listed(s.music.fps))
        {
            s.music.fps = MusicSettings().fps;
        }
        getBool("fx.intro", s.effects.intro);
        getInt("fx.cursor", s.effects.cursor, 0, 3);
        getInt("fx.cursorPlace", s.effects.cursorPlace, 0, 3);
        getInt("fx.popups", s.effects.popups, 0, 3);
        getInt("fx.popupIcon", s.effects.popupIcon, 0, 3);
        getInt("fx.popupButtons", s.effects.popupButtons, 0, 2);
        getText("fx.popupTitle", s.effects.popupTitle);
        getText("fx.popupText", s.effects.popupText);
        getInt("fx.screen", s.effects.screen, 0, 2);
        getColor("background", s.background);
        getBool("backgroundPicture", s.backgroundPicture);
        getInt("format", s.format, 0, 2);
        getText("openFolder", s.openFolder);
        getText("saveFolder", s.saveFolder);
        getText("videoFolder", s.videoFolder);
        getText("musicFolder", s.musicFolder);
        getText("libraryFolder", s.libraryFolder);
        getList("library", s.library);
        getInt("windowX", s.windowX, -100000, 100000);
        getInt("windowY", s.windowY, -100000, 100000);
    }

    CanvasSize CanvasAt(int index)
    {
        return CanvasSizes[std::clamp(index, 0, CanvasCount - 1)];
    }

    std::wstring CanvasName(int index)
    {
        const CanvasSize c = CanvasAt(index);
        int a = c.width;
        int b = c.height;
        while (b != 0)
        {
            const int t = a % b;
            a = b;
            b = t;
        }
        return std::to_wstring(c.width / a) + L":" + std::to_wstring(c.height / a) + L", " + std::to_wstring(c.width) + L"x" + std::to_wstring(c.height);
    }
}
