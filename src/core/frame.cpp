#include "frame.h"

#include "glyphs.h"
#include "icons.h"

#include <cmath>

namespace rf
{
    namespace
    {
        enum class Band
        {
            Etch,
            PlainMenu,
            MenuBand,
            ToolBand,
            AddressBand
        };

        struct BandItem
        {
            Band band;
            int y;
            int h;
        };

        struct Layout
        {
            int border = 4;
            int inset = 0;
            int width = 0;
            int height = 0;
            int statusY = 0;
            int playerY = 0;
            bool hasStatus = false;
            Rect client;
            std::vector<BandItem> bands;
        };

        std::vector<std::wstring> StatusTexts(const FrameSettings& s, const FrameVars& vars)
        {
            std::vector<std::wstring> out;
            for (const auto& f : s.statusFields)
            {
                std::wstring t = ExpandVars(f, vars);
                if (!t.empty())
                {
                    out.push_back(t);
                }
            }
            return out;
        }

        Layout Compute(const FrameSettings& s, int cw, int ch, bool hasStatus)
        {
            Layout L;
            L.border = s.kind == WindowKind::Normal ? 4 : 3;
            L.inset = s.sunken ? 2 : 0;
            L.width = cw + 2 * L.border + 2 * L.inset;
            const bool rebar = s.toolbar || s.address;
            int cur = L.border + TitleHeight(s.style) + 1;
            if (s.menu && !rebar)
            {
                L.bands.push_back(BandItem{Band::PlainMenu, cur, 18});
                cur += 19;
            }
            if (rebar)
            {
                L.bands.push_back(BandItem{Band::Etch, cur, 2});
                cur += 2;
                if (s.menu)
                {
                    L.bands.push_back(BandItem{Band::MenuBand, cur, 22});
                    cur += 22;
                    L.bands.push_back(BandItem{Band::Etch, cur, 2});
                    cur += 2;
                }
                if (s.toolbar)
                {
                    L.bands.push_back(BandItem{Band::ToolBand, cur, 38});
                    cur += 38;
                    L.bands.push_back(BandItem{Band::Etch, cur, 2});
                    cur += 2;
                }
                if (s.address)
                {
                    L.bands.push_back(BandItem{Band::AddressBand, cur, 24});
                    cur += 24;
                    L.bands.push_back(BandItem{Band::Etch, cur, 2});
                    cur += 2;
                }
                cur += 1;
            }
            const int clientY = cur;
            cur += ch + 2 * L.inset;
            if (s.player)
            {
                L.playerY = cur;
                cur += PlayerPanelHeight;
            }
            L.hasStatus = hasStatus;
            if (hasStatus)
            {
                cur += 1;
                L.statusY = cur;
                cur += 20;
            }
            L.height = cur + L.border;
            L.client = Rect{L.border + L.inset, clientY + L.inset, cw, ch};
            return L;
        }

        void TextAt(Painter& p, int x, int baseline, const std::wstring& s, const Font& f, Color c)
        {
            p.Text(x, baseline - Text().Ascent(f), s, f, c);
        }

        void UnderlinedAt(Painter& p, int x, int baseline, const std::wstring& s, const Font& f, Color c)
        {
            p.TextUnderlined(x, baseline - Text().Ascent(f), s, 0, f, c);
        }

        void CaptionButton(Painter& p, const Shades& sh, Color ink, int x, int y, const Glyph& g, int gx, int gy)
        {
            p.Raised(Rect{x, y, 16, 14}, sh);
            p.DrawGlyph(x + gx, y + gy, g, ink);
        }

        constexpr int XpButton = 21;

        void XpCaptionButton(Painter& p, int x, int y, int kind, bool active)
        {
            const Color white = Rgb(0xFF, 0xFF, 0xFF);
            Color top = kind == 0 ? Rgb(0xEB, 0x8E, 0x6E) : Rgb(0x5A, 0x9C, 0xF8);
            Color bottom = kind == 0 ? Rgb(0xC1, 0x34, 0x0B) : Rgb(0x1A, 0x52, 0xD5);
            if (!active)
            {
                top = Mix(top, white, 0.45);
                bottom = Mix(bottom, white, 0.45);
            }
            const int n = XpButton;
            for (int row = 1; row < n - 1; ++row)
            {
                p.HLine(x + 1, y + row, n - 2, Mix(top, bottom, double(row - 1) / double(n - 3)));
            }
            p.HLine(x + 2, y, n - 4, white);
            p.HLine(x + 2, y + n - 1, n - 4, white);
            p.VLine(x, y + 2, n - 4, white);
            p.VLine(x + n - 1, y + 2, n - 4, white);
            for (int cx : {x + 1, x + n - 2})
            {
                for (int cy : {y + 1, y + n - 2})
                {
                    p.Pixel(cx, cy, white);
                }
            }
            if (kind == 0)
            {
                for (int i = 0; i < 9; ++i)
                {
                    p.Fill(x + 5 + i, y + 5 + i, 2, 2, white);
                    p.Fill(x + 14 - i, y + 5 + i, 2, 2, white);
                }
            }
            else if (kind == 1)
            {
                p.Fill(x + 5, y + 5, 11, 3, white);
                p.Frame(Rect{x + 5, y + 5, 11, 11}, white);
            }
            else if (kind == 2)
            {
                p.Fill(x + 5, y + 13, 7, 3, white);
            }
            else
            {
                const Glyph& g = glyphs::Help;
                p.DrawGlyph(x + (n - GlyphWidth(g)) / 2, y + (n - GlyphHeight(g)) / 2, g, white);
                p.DrawGlyph(x + (n - GlyphWidth(g)) / 2 + 1, y + (n - GlyphHeight(g)) / 2, g, white);
            }
        }

        void DrawTitleXp(Painter& p, const FrameSettings& s, int width, int border)
        {
            const FrameColors& c = s.colors;
            const Color base = s.active ? c.active1 : c.inactive1;
            const Color hi = s.active ? c.active2 : c.inactive2;
            const Color white = Rgb(0xFF, 0xFF, 0xFF);
            const Color black = Rgb(0, 0, 0);
            const int titleH = TitleHeight(s.style);
            const int top = border + titleH;
            for (int row = 0; row < top; ++row)
            {
                const double t = double(row) / double(top - 1);
                Color col = base;
                if (row == 0)
                {
                    col = Mix(hi, white, 0.4);
                }
                else if (row < 3)
                {
                    col = hi;
                }
                else if (t < 0.45)
                {
                    col = Mix(hi, base, std::clamp((t - 0.1) / 0.35, 0.0, 1.0));
                }
                else if (t > 0.85)
                {
                    col = Mix(base, black, 0.18);
                }
                p.HLine(0, row, width, col);
            }
            const Color edge = Mix(base, black, 0.4);
            p.VLine(0, 0, top, edge);
            p.VLine(width - 1, 0, top, edge);
            int tx = border + 5;
            if (!s.icon.Empty())
            {
                p.Draw(s.icon, border + 3, border + (titleH - 16) / 2);
                tx = border + 24;
            }
            const int bx = width - border - 2 - XpButton;
            const int by = border + (titleH - XpButton) / 2;
            int left = width - border - 2;
            if (s.kind != WindowKind::Bare)
            {
                XpCaptionButton(p, bx, by, 0, s.active);
                left = bx;
            }
            if (s.kind == WindowKind::Normal)
            {
                XpCaptionButton(p, bx - XpButton - 2, by, 1, s.active);
                XpCaptionButton(p, bx - 2 * (XpButton + 2), by, 2, s.active);
                left = bx - 2 * (XpButton + 2);
            }
            else if (s.kind == WindowKind::Help)
            {
                XpCaptionButton(p, bx - XpButton - 2, by, 3, s.active);
                left = bx - XpButton - 2;
            }
            const Font fb{TitleFamilyFor(s.style), true};
            const std::wstring title = Ellipsize(s.title, fb, left - tx - 6);
            const int baseline = border + (titleH + Text().Ascent(fb)) / 2 - 1;
            TextAt(p, tx + 1, baseline + 1, title, fb, Mix(base, black, 0.55));
            TextAt(p, tx, baseline, title, fb, s.active ? c.titleText : c.inactiveText);
        }

        void DrawEdgesXp(Painter& p, const FrameSettings& s, int width, int height, int border)
        {
            const Color base = s.active ? s.colors.active1 : s.colors.inactive1;
            const Color edge = Mix(base, Rgb(0, 0, 0), 0.4);
            const Color inner = Mix(base, Rgb(0xFF, 0xFF, 0xFF), 0.12);
            const int top = border + TitleHeight(s.style);
            for (int i = 0; i < border; ++i)
            {
                const Color c = i == 0 ? edge : i == border - 1 ? inner : base;
                p.VLine(i, top, height - top, c);
                p.VLine(width - 1 - i, top, height - top, c);
                p.HLine(i, height - 1 - i, width - 2 * i, c);
            }
        }

        void CutCornersXp(Image& img)
        {
            const int skip[] = {5, 3, 2, 1, 1};
            for (int row = 0; row < 5 && row < img.height; ++row)
            {
                for (int i = 0; i < skip[row] && i < img.width; ++i)
                {
                    img.Set(i, row, Color(0));
                    img.Set(img.width - 1 - i, row, Color(0));
                }
            }
        }

        void DrawTitle(Painter& p, const FrameSettings& s, const Shades& sh, int x, int y, int w)
        {
            const FrameColors& c = s.colors;
            Color c1 = s.active ? c.active1 : c.inactive1;
            Color c2 = s.active ? c.active2 : c.inactive2;
            if (!HasGradient(s.style))
            {
                c2 = c1;
            }
            p.GradientH(x, y, w, 18, c1, c2);
            int tx = x + 3;
            if (!s.icon.Empty())
            {
                p.Draw(s.icon, x + 2, y + 1);
                tx = x + 21;
            }
            const int bx = x + w - 2 - 16;
            const int by = y + 2;
            int left = x + w - 2;
            if (s.kind != WindowKind::Bare)
            {
                CaptionButton(p, sh, c.text, bx, by, glyphs::Close, 4, 3);
                left = bx;
            }
            if (s.kind == WindowKind::Normal)
            {
                CaptionButton(p, sh, c.text, bx - 18, by, glyphs::Maximize, 3, 2);
                CaptionButton(p, sh, c.text, bx - 34, by, glyphs::Minimize, 4, 9);
                left = bx - 34;
            }
            else if (s.kind == WindowKind::Help)
            {
                CaptionButton(p, sh, c.text, bx - 18, by, glyphs::Help, 5, 2);
                left = bx - 18;
            }
            const Font fb{FamilyFor(s.style), true};
            const std::wstring title = Ellipsize(s.title, fb, left - tx - 5);
            TextAt(p, tx, y + 13, title, fb, s.active ? c.titleText : c.inactiveText);
        }

        void DrawCorner(Painter& p, const FrameSettings& s, const Shades& sh, int right, int y, int h, int slotWidth)
        {
            const int lx = right - slotWidth - 1;
            p.ThinSunken(Rect{lx, y, slotWidth, h}, sh);
            p.Fill(Rect{lx + 1, y + 1, slotWidth - 2, h - 2}, s.colors.window);
            const Image icon = (s.cornerImage.width == 16 && s.cornerImage.height == 16) ? s.cornerImage : MakeIcon16(s.cornerImage);
            p.Draw(icon, lx + (slotWidth - 16) / 2, y + (h - 16) / 2);
        }

        void DrawMenuItems(Painter& p, const FrameSettings& s, int x, int right, int baseline)
        {
            const Font f{FamilyFor(s.style), false};
            int mx = x;
            for (const auto& item : s.menuItems)
            {
                if (item.empty())
                {
                    continue;
                }
                const int w = Text().Width(item, f);
                if (mx + w > right)
                {
                    break;
                }
                UnderlinedAt(p, mx, baseline, item, f, s.colors.text);
                mx += w + 14;
            }
        }

        void DrawToolbar(Painter& p, const FrameSettings& s, const Shades& sh, int left, int right, int y, int h)
        {
            const Font f{FamilyFor(s.style), false};
            struct Tool
            {
                const wchar_t* label;
                IconId icon;
                bool enabled;
                bool separator;
            };
            const Tool tools[] = {
                {L"Назад", IconId::Back, true, false},
                {L"Вперёд", IconId::Forward, false, false},
                {L"Вверх", IconId::Up, true, false},
                {L"", IconId::Picture, true, true},
                {L"Вырезать", IconId::Cut, true, false},
                {L"Копировать", IconId::Copy, true, false},
                {L"Вставить", IconId::Paste, true, false},
            };
            int bx = left + 9;
            bool first = true;
            for (const Tool& t : tools)
            {
                if (t.separator)
                {
                    if (bx + 6 > right - 4)
                    {
                        break;
                    }
                    p.EtchedV(bx + 2, y + 3, h - 6, sh);
                    bx += 7;
                    continue;
                }
                const int tw = Text().Width(t.label, f);
                const int bw = std::max(tw + 10, 38);
                if (bx + bw > right - 3)
                {
                    break;
                }
                if (first)
                {
                    p.ThinRaised(Rect{bx, y + 1, bw, h - 2}, sh);
                    first = false;
                }
                const Image& icon = BuiltinIcon(t.icon);
                p.Draw(t.enabled ? icon : DisabledIcon(icon, sh), bx + (bw - 16) / 2, y + 4);
                const int lx = bx + (bw - tw) / 2;
                if (t.enabled)
                {
                    TextAt(p, lx, y + 32, t.label, f, s.colors.text);
                }
                else
                {
                    p.TextEmbossed(lx, y + 32 - Text().Ascent(f), t.label, f, sh);
                }
                bx += bw;
            }
        }

        void DrawAddress(Painter& p, const FrameSettings& s, const Shades& sh, int left, int right, int y)
        {
            const Font f{FamilyFor(s.style), false};
            const std::wstring label = L"Адрес";
            UnderlinedAt(p, left + 11, y + 16, label, f, s.colors.text);
            const int fx = left + 11 + Text().Width(label, f) + 6;
            const int fw = right - 3 - fx;
            if (fw < 40)
            {
                return;
            }
            const Color window = s.colors.window;
            p.Sunken(Rect{fx, y + 2, fw, 20}, sh, &window);
            p.Draw(BuiltinIcon(IconId::Folder), fx + 3, y + 4);
            TextAt(p, fx + 22, y + 16, Ellipsize(s.addressText, f, fw - 44), f, s.colors.text);
            p.Raised(Rect{fx + fw - 18, y + 4, 16, 16}, sh);
            p.DrawGlyph(fx + fw - 18 + 4, y + 4 + 6, glyphs::Down, s.colors.text);
        }

        void DrawGrip(Painter& p, const Shades& sh, int gx, int gy)
        {
            for (int base : {0, 4, 8})
            {
                for (int k = 0; k < 3; ++k)
                {
                    const int off = base + k;
                    if (off > 11)
                    {
                        continue;
                    }
                    const Color c = k == 0 ? sh.hilite : sh.shadow;
                    for (int t = 0; t <= 11 - off; ++t)
                    {
                        p.Pixel(gx + off + t, gy + 11 - t, c);
                    }
                }
            }
        }

        void DrawStatus(Painter& p, const FrameSettings& s, const Shades& sh, const std::vector<std::wstring>& texts, int x, int y, int w)
        {
            const Font f{FamilyFor(s.style), false};
            const int n = int(texts.size());
            std::vector<int> widths(std::size_t(n), 0);
            int fixed = 0;
            for (int i = 1; i < n; ++i)
            {
                int fw = Text().Width(texts[std::size_t(i)], f) + 10;
                if (s.statusIcon && i == n - 1)
                {
                    fw += 18;
                }
                if (s.grip && i == n - 1)
                {
                    fw += 12;
                }
                widths[std::size_t(i)] = std::max(fw, 40);
                fixed += widths[std::size_t(i)];
            }
            widths[0] = std::max(20, w - fixed - 2 * (n - 1));
            int px = x;
            for (int i = 0; i < n; ++i)
            {
                int pw = widths[std::size_t(i)];
                if (i == n - 1)
                {
                    pw = x + w - px;
                }
                if (pw <= 4)
                {
                    break;
                }
                p.ThinSunken(Rect{px, y + 2, pw, 18}, sh);
                int tx = px + 3;
                if (s.statusIcon && i == n - 1 && n > 1)
                {
                    p.Draw(BuiltinIcon(IconId::Monitor), px + 2, y + 3);
                    tx = px + 21;
                }
                const int reserve = (s.grip && i == n - 1) ? 16 : 4;
                const std::wstring t = Ellipsize(texts[std::size_t(i)], f, pw - (tx - px) - reserve);
                TextAt(p, tx, y + 15, t, f, s.colors.text);
                px += pw + 2;
            }
            if (s.grip)
            {
                DrawGrip(p, sh, x + w - 13, y + 7);
            }
        }

        void PlayerButton(Painter& p, const Shades& sh, Color ink, const Rect& r, const Glyph& g, bool pressed)
        {
            const int o = pressed ? 1 : 0;
            if (pressed)
            {
                p.Dither(r.Inflate(-1), sh.face, sh.hilite);
                p.Edge(r, sh.dark, sh.hilite);
                p.Edge(r.Inflate(-1), sh.shadow, sh.light);
            }
            p.DrawGlyph(r.x + (r.w - GlyphWidth(g)) / 2 + o, r.y + (r.h - GlyphHeight(g)) / 2 + o, g, ink);
        }

        void DrawPlayer(Painter& p, const FrameSettings& s, const Shades& sh, int left, int right, int y)
        {
            const Color ink = s.colors.text;
            const int x0 = left + 8;
            const int w = std::max(20, right - left - 16);
            p.Sunken(Rect{x0, y + 9, w, 4}, sh);
            const int tw = 11;
            const double pos = std::clamp(s.playerPosition, 0.0, 1.0);
            const int tx = x0 + int(std::lround(pos * (w - tw)));
            p.Raised(Rect{tx, y + 2, tw, 18}, sh);
            const int by = y + 26;
            const Glyph* glyphsRow[] = {&glyphs::Play, &glyphs::Pause, &glyphs::Stop, nullptr, &glyphs::Previous, &glyphs::Rewind, &glyphs::FastForward, &glyphs::Next};
            const int room = right - left - 12;
            const std::size_t count = room >= 7 * 24 + 9 ? 8 : room >= 3 * 24 ? 3 : room >= 2 * 24 ? 2 : 1;
            int bx = left + 6;
            for (std::size_t i = 0; i < count; ++i)
            {
                if (glyphsRow[i] == nullptr)
                {
                    p.EtchedV(bx + 3, by + 2, 18, sh);
                    bx += 9;
                    continue;
                }
                PlayerButton(p, sh, ink, Rect{bx, by, 23, 22}, *glyphsRow[i], i == 0);
                bx += 24;
            }
            const int vw = 44;
            const int vx = right - 8 - vw;
            if (vx - 22 > bx + 4)
            {
                p.Draw(BuiltinIcon(IconId::Speaker), vx - 22, by + 3);
                const int vy = by + 4;
                const int vh = 14;
                for (int i = 0; i < vw; ++i)
                {
                    const int top = vy + vh - 1 - (i * (vh - 1)) / (vw - 1);
                    p.Pixel(vx + i, top, sh.shadow);
                    p.Pixel(vx + i, vy + vh, sh.hilite);
                }
                p.VLine(vx + vw, vy, vh + 1, sh.hilite);
                p.Raised(Rect{vx + vw * 3 / 4 - 3, vy - 2, 7, vh + 4}, sh);
            }
        }
    }

    std::wstring ExpandVars(const std::wstring& text, const FrameVars& vars)
    {
        std::wstring out = text;
        const auto replace = [&out](const std::wstring& key, const std::wstring& value) {
            std::size_t pos = 0;
            while ((pos = out.find(key, pos)) != std::wstring::npos)
            {
                out.replace(pos, key.size(), value);
                pos += value.size();
            }
        };
        replace(L"{имя}", vars.name);
        replace(L"{ширина}", std::to_wstring(vars.width));
        replace(L"{высота}", std::to_wstring(vars.height));
        replace(L"{время}", vars.time);
        return out;
    }

    FrameGeometry MeasureFrame(const FrameSettings& settings, int contentWidth, int contentHeight)
    {
        bool hasStatus = false;
        if (settings.status)
        {
            for (const auto& f : settings.statusFields)
            {
                if (!f.empty())
                {
                    hasStatus = true;
                }
            }
        }
        const Layout L = Compute(settings, contentWidth, contentHeight, hasStatus);
        return FrameGeometry{L.width, L.height, L.client};
    }

    Image RenderFrame(const FrameSettings& s, int cw, int ch, const FrameVars& vars, Rect* client)
    {
        const std::vector<std::wstring> statusTexts = s.status ? StatusTexts(s, vars) : std::vector<std::wstring>();
        const Layout L = Compute(s, cw, ch, !statusTexts.empty());
        const Shades sh = ShadesFor(s.colors.face, s.style);
        Image img(L.width, L.height, sh.face);
        Painter p(img);
        const bool xp = s.style == Style::WinXP;
        if (xp)
        {
            DrawEdgesXp(p, s, L.width, L.height, L.border);
            DrawTitleXp(p, s, L.width, L.border);
        }
        else
        {
            p.WindowEdge(Rect{0, 0, L.width, L.height}, sh);
            DrawTitle(p, s, sh, L.border, L.border, L.width - 2 * L.border);
        }
        const int left = L.border;
        const int right = L.width - L.border;
        const bool hasCorner = s.corner && !s.cornerImage.Empty();
        for (const BandItem& b : L.bands)
        {
            switch (b.band)
            {
            case Band::Etch:
                p.EtchedH(left, b.y, right - left, sh);
                break;
            case Band::PlainMenu:
            {
                int menuRight = right;
                if (hasCorner)
                {
                    DrawCorner(p, s, sh, right, b.y, b.h, 26);
                    menuRight = right - 30;
                }
                DrawMenuItems(p, s, left + 6, menuRight, b.y + 13);
                break;
            }
            case Band::MenuBand:
            {
                p.ThinRaised(Rect{left + 2, b.y + 2, 3, b.h - 4}, sh);
                int menuRight = right;
                if (hasCorner)
                {
                    DrawCorner(p, s, sh, right, b.y + 1, b.h - 2, 30);
                    p.EtchedV(right - 31 - 4, b.y + 2, b.h - 4, sh);
                    menuRight = right - 38;
                }
                DrawMenuItems(p, s, left + 11, menuRight, b.y + 15);
                break;
            }
            case Band::ToolBand:
                p.ThinRaised(Rect{left + 2, b.y + 2, 3, b.h - 4}, sh);
                DrawToolbar(p, s, sh, left, right, b.y, b.h);
                break;
            case Band::AddressBand:
                p.ThinRaised(Rect{left + 2, b.y + 2, 3, b.h - 4}, sh);
                DrawAddress(p, s, sh, left, right, b.y);
                break;
            }
        }
        if (s.sunken)
        {
            p.Sunken(Rect{L.client.x - 2, L.client.y - 2, cw + 4, ch + 4}, sh);
        }
        p.Fill(L.client, s.player ? Rgb(0, 0, 0) : s.colors.window);
        if (s.player)
        {
            DrawPlayer(p, s, sh, left, right, L.playerY);
        }
        if (L.hasStatus)
        {
            DrawStatus(p, s, sh, statusTexts, left, L.statusY, right - left);
        }
        if (xp)
        {
            CutCornersXp(img);
        }
        if (client != nullptr)
        {
            *client = L.client;
        }
        return img;
    }

    void DrawPushButton(Painter& p, const Rect& r, const FrameSettings& s, const Shades& sh, bool isDefault)
    {
        if (s.style != Style::WinXP)
        {
            if (isDefault)
            {
                p.Frame(r, Rgb(0, 0, 0));
                p.Raised(r.Inflate(-1), sh);
                p.Dotted(r.Inflate(-4), s.colors.text);
            }
            else
            {
                p.Raised(r, sh);
            }
            return;
        }
        const Color border = Rgb(0x00, 0x3C, 0x74);
        for (int row = 1; row < r.h - 1; ++row)
        {
            const double t = double(row) / double(std::max(1, r.h - 2));
            const Color c = t > 0.8 ? Mix(Rgb(0xE3, 0xE3, 0xDB), Rgb(0xD6, 0xD0, 0xC5), (t - 0.8) / 0.2) : Mix(Rgb(0xFF, 0xFF, 0xFF), Rgb(0xEC, 0xEB, 0xE6), t / 0.8);
            p.HLine(r.x + 1, r.y + row, r.w - 2, c);
        }
        p.HLine(r.x + 2, r.y, r.w - 4, border);
        p.HLine(r.x + 2, r.Bottom() - 1, r.w - 4, border);
        p.VLine(r.x, r.y + 2, r.h - 4, border);
        p.VLine(r.Right() - 1, r.y + 2, r.h - 4, border);
        p.Pixel(r.x + 1, r.y + 1, border);
        p.Pixel(r.Right() - 2, r.y + 1, border);
        p.Pixel(r.x + 1, r.Bottom() - 2, border);
        p.Pixel(r.Right() - 2, r.Bottom() - 2, border);
        if (isDefault)
        {
            const Rect in = r.Inflate(-1);
            p.HLine(in.x + 1, in.y, in.w - 2, Rgb(0xCE, 0xE7, 0xFF));
            p.HLine(in.x + 1, in.Bottom() - 1, in.w - 2, Rgb(0x69, 0x82, 0xEE));
            p.VLine(in.x, in.y + 1, in.h - 2, Rgb(0xBC, 0xD4, 0xF6));
            p.VLine(in.Right() - 1, in.y + 1, in.h - 2, Rgb(0x89, 0xAD, 0xE4));
            p.VLine(in.x + 1, in.y + 1, in.h - 2, Rgb(0xBC, 0xD4, 0xF6));
            p.VLine(in.Right() - 2, in.y + 1, in.h - 2, Rgb(0x89, 0xAD, 0xE4));
        }
    }

    Rect CloseButtonRect(const FrameSettings& settings, int frameWidth)
    {
        const int border = settings.kind == WindowKind::Normal ? 4 : 3;
        if (settings.style == Style::WinXP)
        {
            return Rect{frameWidth - border - 2 - XpButton, border + (TitleHeight(settings.style) - XpButton) / 2, XpButton, XpButton};
        }
        return Rect{frameWidth - border - 2 - 16, border + 2, 16, 14};
    }

    int AutoScale(int contentWidth, int contentHeight)
    {
        const int side = std::max(contentWidth, contentHeight);
        return std::clamp(int(std::lround(side / 800.0)), 1, 4);
    }

    int EffectiveScale(const FrameSettings& settings, int contentWidth, int contentHeight)
    {
        if (settings.scale >= 1 && settings.scale <= 4)
        {
            return settings.scale;
        }
        return AutoScale(contentWidth, contentHeight);
    }

    Image ComposeFramed(const Image& content, const FrameSettings& settings, const FrameVars& vars, int scale, Rect* area)
    {
        const int k = std::max(1, scale);
        if (content.Empty())
        {
            Rect client;
            Image empty = ScaleNearest(RenderFrame(settings, 64, 48, vars, &client), k);
            if (area != nullptr)
            {
                *area = Rect{client.x * k, client.y * k, client.w * k, client.h * k};
            }
            return empty;
        }
        const int cw = (content.width + k - 1) / k;
        const int ch = (content.height + k - 1) / k;
        Rect client;
        const Image frame = RenderFrame(settings, cw, ch, vars, &client);
        Image out = ScaleNearest(frame, k);
        const int ox = client.x * k;
        const int oy = client.y * k;
        if (area != nullptr)
        {
            *area = Rect{ox, oy, cw * k, ch * k};
        }
        const Color back = Opaque(settings.colors.window);
        for (int y = 0; y < ch * k; ++y)
        {
            const Color* src = content.Row(std::min(y, content.height - 1));
            Color* dst = out.Row(oy + y) + ox;
            for (int x = 0; x < cw * k; ++x)
            {
                const Color c = src[std::min(x, content.width - 1)];
                dst[x] = Alpha(c) == 255 ? c : Opaque(Over(back, c));
            }
        }
        return out;
    }
}
