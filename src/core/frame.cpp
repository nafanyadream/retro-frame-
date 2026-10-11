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
            int cur = L.border + 18 + 1;
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
        p.WindowEdge(Rect{0, 0, L.width, L.height}, sh);
        DrawTitle(p, s, sh, L.border, L.border, L.width - 2 * L.border);
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
        p.Fill(L.client, s.colors.window);
        if (L.hasStatus)
        {
            DrawStatus(p, s, sh, statusTexts, left, L.statusY, right - left);
        }
        if (client != nullptr)
        {
            *client = L.client;
        }
        return img;
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

    Image ComposeFramed(const Image& content, const FrameSettings& settings, const FrameVars& vars, int scale)
    {
        const int k = std::max(1, scale);
        if (content.Empty())
        {
            return ScaleNearest(RenderFrame(settings, 64, 48, vars, nullptr), k);
        }
        const int cw = (content.width + k - 1) / k;
        const int ch = (content.height + k - 1) / k;
        Rect client;
        const Image frame = RenderFrame(settings, cw, ch, vars, &client);
        Image out = ScaleNearest(frame, k);
        const int ox = client.x * k;
        const int oy = client.y * k;
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
