#include "settings.h"

#include "utf.h"

#include <cstdio>
#include <cstdlib>
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
        else if (mode == RecorderMode)
        {
            look.iconIndex = 10;
            look.titleFromName = true;
            look.customTitle = L"Звук";
            look.menu = true;
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
        for (int m = 0; m < ModeCount; ++m)
        {
            s.looks[std::size_t(m)] = DefaultLook(m);
        }
        s.presets = StarterPresets();
        return s;
    }

    std::vector<LookPreset> StarterPresets()
    {
        int burgundy = 0;
        int sea = 0;
        for (int i = 0; i < SchemeCount(); ++i)
        {
            if (SchemeName(i) == L"Бордовая")
            {
                burgundy = i;
            }
            if (SchemeName(i) == L"Морская волна")
            {
                sea = i;
            }
        }
        std::vector<LookPreset> out;
        LookPreset a;
        a.name = L"Windows 95, классика";
        a.style = Style::Win95;
        a.scheme = 0;
        a.colors = SchemeColors(0, a.style);
        a.look.iconIndex = 0;
        a.look.menu = true;
        a.look.status = true;
        out.push_back(a);
        LookPreset b;
        b.name = L"Windows 98, бордовая";
        b.style = Style::Win98;
        b.scheme = burgundy;
        b.colors = SchemeColors(burgundy, b.style);
        b.look.iconIndex = 2;
        b.look.menu = true;
        b.look.toolbar = true;
        b.look.address = true;
        out.push_back(b);
        LookPreset c;
        c.name = L"Windows 2000, диалог";
        c.style = Style::Win2000;
        c.scheme = sea;
        c.colors = SchemeColors(sea, c.style);
        c.look.iconIndex = NoIconIndex;
        c.look.kind = WindowKind::Dialog;
        c.look.titleFromName = false;
        c.look.customTitle = L"Свойства";
        out.push_back(c);
        return out;
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
        const char* prefixes[] = {"", "video.", "music.", "recorder."};
        for (int m = 0; m < ModeCount; ++m)
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
        o << "music.lyrics=" << (s.music.lyrics ? 1 : 0) << "\n";
        o << "recorder.format=" << s.recorder.format << "\n";
        o << "recorder.square=" << (s.recorder.square ? 1 : 0) << "\n";
        o << "recorder.canvas=" << s.recorder.canvas << "\n";
        o << "recorder.fade=" << s.recorder.fade << "\n";
        o << "recorder.fps=" << s.recorder.fps << "\n";
        o << "recorder.russian=" << (s.recorder.russian ? 1 : 0) << "\n";
        o << "voice=" << SerializeVoiceChain(s.voice) << "\n";
        o << "micName=" << Escape(s.micName) << "\n";
        o << "micQuality=" << s.micQuality << "\n";
        o << "voicePresets=" << s.voicePresets.size() << "\n";
        for (std::size_t i = 0; i < s.voicePresets.size(); ++i)
        {
            const std::string p = "voicePreset." + std::to_string(i) + ".";
            o << p << "name=" << Escape(s.voicePresets[i].name) << "\n";
            o << p << "chain=" << SerializeVoiceChain(s.voicePresets[i].chain) << "\n";
        }
        o << "fx.intro=" << (s.effects.intro ? 1 : 0) << "\n";
        o << "fx.cursor=" << s.effects.cursor << "\n";
        o << "fx.cursorPlace=" << s.effects.cursorPlace << "\n";
        o << "fx.popups=" << s.effects.popups << "\n";
        o << "fx.popupIcon=" << s.effects.popupIcon << "\n";
        o << "fx.popupButtons=" << s.effects.popupButtons << "\n";
        o << "fx.popupTitle=" << Escape(s.effects.popupTitle) << "\n";
        o << "fx.popupText=" << Escape(s.effects.popupText) << "\n";
        o << "fx.screen=" << s.effects.screen << "\n";
        o << "fx.palette=" << s.effects.palette << "\n";
        o << "background=" << Hex(s.background) << "\n";
        o << "backgroundPicture=" << (s.backgroundPicture ? 1 : 0) << "\n";
        o << "previewSound=" << (s.previewSound ? 1 : 0) << "\n";
        o << "format=" << s.format << "\n";
        o << "openFolder=" << Escape(s.openFolder) << "\n";
        o << "saveFolder=" << Escape(s.saveFolder) << "\n";
        o << "videoFolder=" << Escape(s.videoFolder) << "\n";
        o << "musicFolder=" << Escape(s.musicFolder) << "\n";
        o << "libraryFolder=" << Escape(s.libraryFolder) << "\n";
        o << "library=" << EscapeList(s.library) << "\n";
        o << "windowX=" << s.windowX << "\n";
        o << "windowY=" << s.windowY << "\n";
        o << "presets=" << s.presets.size() << "\n";
        for (std::size_t i = 0; i < s.presets.size(); ++i)
        {
            const LookPreset& pr = s.presets[i];
            const std::string p = "preset." + std::to_string(i) + ".";
            o << p << "name=" << Escape(pr.name) << "\n";
            o << p << "style=" << int(pr.style) << "\n";
            o << p << "scheme=" << pr.scheme << "\n";
            o << p << "face=" << Hex(pr.colors.face) << "\n";
            o << p << "active1=" << Hex(pr.colors.active1) << "\n";
            o << p << "active2=" << Hex(pr.colors.active2) << "\n";
            o << p << "inactive1=" << Hex(pr.colors.inactive1) << "\n";
            o << p << "inactive2=" << Hex(pr.colors.inactive2) << "\n";
            o << p << "titleText=" << Hex(pr.colors.titleText) << "\n";
            o << p << "inactiveText=" << Hex(pr.colors.inactiveText) << "\n";
            o << p << "text=" << Hex(pr.colors.text) << "\n";
            o << p << "window=" << Hex(pr.colors.window) << "\n";
            o << p << "select=" << Hex(pr.colors.select) << "\n";
            o << p << "active=" << (pr.active ? 1 : 0) << "\n";
            o << p << "sunken=" << (pr.sunken ? 1 : 0) << "\n";
            o << p << "grip=" << (pr.grip ? 1 : 0) << "\n";
            o << p << "scale=" << pr.scale << "\n";
            o << p << "icon=" << pr.look.iconIndex << "\n";
            o << p << "titleFromName=" << (pr.look.titleFromName ? 1 : 0) << "\n";
            o << p << "customTitle=" << Escape(pr.look.customTitle) << "\n";
            o << p << "kind=" << int(pr.look.kind) << "\n";
            o << p << "menu=" << (pr.look.menu ? 1 : 0) << "\n";
            o << p << "status=" << (pr.look.status ? 1 : 0) << "\n";
            o << p << "toolbar=" << (pr.look.toolbar ? 1 : 0) << "\n";
            o << p << "address=" << (pr.look.address ? 1 : 0) << "\n";
            o << p << "corner=" << (pr.look.corner ? 1 : 0) << "\n";
            if (pr.icon.width == 16 && pr.icon.height == 16)
            {
                std::string pixels;
                for (Color px : pr.icon.pixels)
                {
                    char buf[9];
                    std::snprintf(buf, sizeof(buf), "%08X", unsigned(px));
                    pixels += buf;
                }
                o << p << "iconData=" << pixels << "\n";
            }
        }
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
        getInt("mode", s.mode, 0, ModeCount - 1);
        int style = int(f.style);
        getInt("style", style, 0, StyleCount - 1);
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
        const char* prefixes[] = {"", "video.", "music.", "recorder."};
        for (int m = 0; m < ModeCount; ++m)
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
        getBool("music.lyrics", s.music.lyrics);
        getInt("recorder.format", s.recorder.format, 0, 5);
        getBool("recorder.square", s.recorder.square);
        getInt("recorder.canvas", s.recorder.canvas, 0, CanvasCount - 1);
        getInt("recorder.fade", s.recorder.fade, 0, 30);
        getInt("recorder.fps", s.recorder.fps, 10, 60);
        getBool("recorder.russian", s.recorder.russian);
        if (const std::string* v = get("voice"))
        {
            s.voice = ParseVoiceChain(*v);
        }
        getText("micName", s.micName);
        getInt("micQuality", s.micQuality, 0, 1);
        if (get("voicePresets") != nullptr)
        {
            int count = 0;
            getInt("voicePresets", count, 0, 200);
            s.voicePresets.clear();
            for (int i = 0; i < count; ++i)
            {
                const std::string p = "voicePreset." + std::to_string(i) + ".";
                const std::string* chain = get(p + "chain");
                if (get(p + "name") == nullptr || chain == nullptr)
                {
                    continue;
                }
                VoicePreset vp;
                getText(p + "name", vp.name);
                vp.chain = ParseVoiceChain(*chain);
                s.voicePresets.push_back(vp);
            }
        }
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
        if (!listed(s.recorder.fps))
        {
            s.recorder.fps = MusicSettings().fps;
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
        getInt("fx.palette", s.effects.palette, 0, 2);
        getColor("background", s.background);
        getBool("backgroundPicture", s.backgroundPicture);
        getBool("previewSound", s.previewSound);
        getInt("format", s.format, 0, 2);
        getText("openFolder", s.openFolder);
        getText("saveFolder", s.saveFolder);
        getText("videoFolder", s.videoFolder);
        getText("musicFolder", s.musicFolder);
        getText("libraryFolder", s.libraryFolder);
        getList("library", s.library);
        getInt("windowX", s.windowX, -100000, 100000);
        getInt("windowY", s.windowY, -100000, 100000);
        if (get("presets") != nullptr)
        {
            int count = 0;
            getInt("presets", count, 0, 200);
            s.presets.clear();
            for (int i = 0; i < count; ++i)
            {
                const std::string p = "preset." + std::to_string(i) + ".";
                if (get(p + "name") == nullptr)
                {
                    continue;
                }
                LookPreset pr;
                getText(p + "name", pr.name);
                int presetStyle = int(pr.style);
                getInt(p + "style", presetStyle, 0, StyleCount - 1);
                pr.style = static_cast<Style>(presetStyle);
                getInt(p + "scheme", pr.scheme, -1, SchemeCount() - 1);
                pr.colors = SchemeColors(std::max(0, pr.scheme), pr.style);
                getColor(p + "face", pr.colors.face);
                getColor(p + "active1", pr.colors.active1);
                getColor(p + "active2", pr.colors.active2);
                getColor(p + "inactive1", pr.colors.inactive1);
                getColor(p + "inactive2", pr.colors.inactive2);
                getColor(p + "titleText", pr.colors.titleText);
                getColor(p + "inactiveText", pr.colors.inactiveText);
                getColor(p + "text", pr.colors.text);
                getColor(p + "window", pr.colors.window);
                getColor(p + "select", pr.colors.select);
                getBool(p + "active", pr.active);
                getBool(p + "sunken", pr.sunken);
                getBool(p + "grip", pr.grip);
                getInt(p + "scale", pr.scale, 0, 4);
                getInt(p + "icon", pr.look.iconIndex, NoIconIndex, CustomIconIndex);
                getBool(p + "titleFromName", pr.look.titleFromName);
                getText(p + "customTitle", pr.look.customTitle);
                int presetKind = int(pr.look.kind);
                getInt(p + "kind", presetKind, 0, 3);
                pr.look.kind = static_cast<WindowKind>(presetKind);
                getBool(p + "menu", pr.look.menu);
                getBool(p + "status", pr.look.status);
                getBool(p + "toolbar", pr.look.toolbar);
                getBool(p + "address", pr.look.address);
                getBool(p + "corner", pr.look.corner);
                if (const std::string* data = get(p + "iconData"))
                {
                    if (data->size() == 256 * 8)
                    {
                        Image icon(16, 16);
                        bool ok = true;
                        for (std::size_t k = 0; k < 256 && ok; ++k)
                        {
                            char* end = nullptr;
                            const std::string hex = data->substr(k * 8, 8);
                            icon.pixels[k] = Color(std::strtoul(hex.c_str(), &end, 16));
                            ok = end == hex.c_str() + 8;
                        }
                        if (ok)
                        {
                            pr.icon = icon;
                        }
                    }
                }
                s.presets.push_back(pr);
            }
        }
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
