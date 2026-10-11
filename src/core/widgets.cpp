#include "widgets.h"

#include "glyphs.h"
#include "icons.h"

namespace rf::ui
{
    namespace
    {
        const Color Black = Rgb(0, 0, 0);
        const Color White = Rgb(0xFF, 0xFF, 0xFF);
        const Color Navy = Rgb(0x00, 0x00, 0x80);

        const Glyph RadioRing = {"....SSSS....", "..SSDDDDSS..", ".SDDWWWWLLH.", ".SDWWWWWWLH.", "SDWWWWWWWWLH",
                                 "SDWWWWWWWWLH", "SDWWWWWWWWLH", "SDWWWWWWWWLH", ".SDWWWWWWLH.", ".SLLWWWWLLH.",
                                 "..HHLLLLHH..", "....HHHH...."};

        int TextW(const std::wstring& s, const Font& f)
        {
            return Text().Width(s, f);
        }

        std::vector<std::wstring> SplitLines(const std::wstring& s)
        {
            std::vector<std::wstring> lines;
            std::wstring cur;
            for (wchar_t ch : s)
            {
                if (ch == L'\n')
                {
                    lines.push_back(cur);
                    cur.clear();
                }
                else
                {
                    cur.push_back(ch);
                }
            }
            lines.push_back(cur);
            return lines;
        }
    }

    const Font& UiFont()
    {
        static const Font f{FontFamily::SansSerif, false};
        return f;
    }

    const Font& UiBold()
    {
        static const Font f{FontFamily::SansSerif, true};
        return f;
    }

    void WindowBorder::Paint(Painter& p)
    {
        p.WindowEdge(Rect{0, 0, rect.w, rect.h}, Sh());
    }

    Rect TitleBar::CloseRect() const
    {
        return Rect{rect.w - 2 - 16, 2, 16, 14};
    }

    Rect TitleBar::MaxRect() const
    {
        const Rect c = CloseRect();
        return Rect{c.x - 18, 2, 16, 14};
    }

    Rect TitleBar::MinRect() const
    {
        const Rect c = showMaximize ? MaxRect() : CloseRect();
        return Rect{showMaximize ? c.x - 16 : c.x - 18, 2, 16, 14};
    }

    void TitleBar::Paint(Painter& p)
    {
        const bool act = root == nullptr || root->active;
        p.GradientH(0, 0, rect.w, rect.h, act ? Rgb(0x00, 0x00, 0x80) : Rgb(0x80, 0x80, 0x80), act ? Rgb(0x10, 0x84, 0xD0) : Rgb(0xC0, 0xC0, 0xC0));
        int tx = 3;
        if (!icon.Empty())
        {
            p.Draw(icon, 2, 1);
            tx = 21;
        }
        int left = rect.w - 2;
        const Shades& sh = Sh();
        const auto button = [&](const Rect& r, const Glyph& g, int gx, int gy, int id, bool enabledButton) {
            const bool down = pressed_ == id && inside_;
            if (down)
            {
                p.Pressed(r, sh);
            }
            else
            {
                p.Raised(r, sh);
            }
            const int o = down ? 1 : 0;
            if (enabledButton)
            {
                p.DrawGlyph(r.x + gx + o, r.y + gy + o, g, Black);
            }
            else
            {
                p.DrawGlyph(r.x + gx + 1, r.y + gy + 1, g, sh.hilite);
                p.DrawGlyph(r.x + gx, r.y + gy, g, sh.shadow);
            }
            left = std::min(left, r.x);
        };
        if (showClose)
        {
            button(CloseRect(), glyphs::Close, 4, 3, 3, true);
        }
        if (showMaximize)
        {
            button(MaxRect(), glyphs::Maximize, 3, 2, 2, false);
        }
        if (showMinimize)
        {
            button(MinRect(), glyphs::Minimize, 4, 9, 1, true);
        }
        const std::wstring t = Ellipsize(title, UiBold(), left - tx - 5);
        p.Text(tx, 2, t, UiBold(), act ? White : Rgb(0xC0, 0xC0, 0xC0));
    }

    void TitleBar::OnMouseDown(const MouseEvent& e)
    {
        pressed_ = 0;
        if (showClose && CloseRect().Contains(e.x, e.y))
        {
            pressed_ = 3;
        }
        else if (showMinimize && MinRect().Contains(e.x, e.y))
        {
            pressed_ = 1;
        }
        inside_ = pressed_ != 0;
        Invalidate();
    }

    void TitleBar::OnMouseMove(const MouseEvent& e)
    {
        if (pressed_ == 0)
        {
            return;
        }
        const Rect r = pressed_ == 3 ? CloseRect() : MinRect();
        const bool now = r.Contains(e.x, e.y);
        if (now != inside_)
        {
            inside_ = now;
            Invalidate();
        }
    }

    void TitleBar::OnMouseUp(const MouseEvent& e)
    {
        const int was = pressed_;
        pressed_ = 0;
        Invalidate();
        if (was == 3 && CloseRect().Contains(e.x, e.y) && onClose)
        {
            root->Post(onClose);
        }
        else if (was == 1 && MinRect().Contains(e.x, e.y) && onMinimize)
        {
            root->Post(onMinimize);
        }
    }

    bool TitleBar::IsCaption(int x, int y) const
    {
        if (showClose && CloseRect().Contains(x, y))
        {
            return false;
        }
        if (showMaximize && MaxRect().Contains(x, y))
        {
            return false;
        }
        if (showMinimize && MinRect().Contains(x, y))
        {
            return false;
        }
        return true;
    }

    void Label::Paint(Painter& p)
    {
        const auto lines = SplitLines(text);
        int y = 0;
        for (const auto& line : lines)
        {
            const std::wstring s = ellipsis ? Ellipsize(line, font, rect.w) : line;
            const int w = TextW(s, font);
            int x = 0;
            if (align == Align::Center)
            {
                x = (rect.w - w) / 2;
            }
            else if (align == Align::Right)
            {
                x = rect.w - w;
            }
            if (enabled)
            {
                p.Text(x, y, s, font, color);
            }
            else
            {
                p.TextEmbossed(x, y, s, font, Sh());
            }
            y += lineHeight;
        }
    }

    void Button::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        const Rect r{0, 0, rect.w, rect.h};
        const bool down = (down_ && inside_) || toggled;
        if (flat)
        {
            p.Fill(r, sh.face);
            if (down)
            {
                p.ThinSunken(r, sh);
            }
            else if (IsHover() && enabled)
            {
                p.ThinRaised(r, sh);
            }
        }
        else if (isDefault)
        {
            p.Frame(r, Black);
            if (down)
            {
                p.Fill(r.Inflate(-1), sh.face);
                p.Frame(r.Inflate(-1), sh.shadow);
            }
            else
            {
                p.Raised(r.Inflate(-1), sh);
            }
        }
        else if (down)
        {
            p.Pressed(r, sh);
        }
        else
        {
            p.Raised(r, sh);
        }
        const int o = down ? 1 : 0;
        const int tw = text.empty() ? 0 : TextW(text, UiFont());
        const int iw = icon.Empty() ? 0 : icon.width;
        const int gap = (iw > 0 && tw > 0) ? 4 : 0;
        const int total = iw + gap + tw;
        int x = (rect.w - total) / 2 + o;
        if (iw > 0)
        {
            const Image img = enabled ? icon : DisabledIcon(icon, sh);
            p.Draw(img, x, (rect.h - icon.height) / 2 + o);
            x += iw + gap;
        }
        if (tw > 0)
        {
            const int ty = (rect.h - 13) / 2 + o;
            if (enabled)
            {
                p.TextUnderlined(x, ty, text, underline, UiFont(), Black);
            }
            else
            {
                p.TextEmbossed(x, ty, text, UiFont(), sh);
            }
        }
    }

    void Button::OnMouseDown(const MouseEvent&)
    {
        down_ = true;
        inside_ = true;
        Invalidate();
    }

    void Button::OnMouseMove(const MouseEvent& e)
    {
        if (!down_)
        {
            return;
        }
        const bool now = e.x >= 0 && e.y >= 0 && e.x < rect.w && e.y < rect.h;
        if (now != inside_)
        {
            inside_ = now;
            Invalidate();
        }
    }

    void Button::OnMouseUp(const MouseEvent& e)
    {
        const bool fire = down_ && e.x >= 0 && e.y >= 0 && e.x < rect.w && e.y < rect.h;
        down_ = false;
        inside_ = false;
        Invalidate();
        if (fire && onClick)
        {
            root->Post(onClick);
        }
    }

    void CheckBox::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        const int by = (rect.h - 13) / 2;
        const Color fill = (!enabled || (down_ && IsCaptured())) ? sh.face : White;
        p.Sunken(Rect{0, by, 13, 13}, sh, &fill);
        if (checked)
        {
            p.DrawGlyph(3, by + 3, glyphs::Check, enabled ? Black : sh.shadow);
        }
        if (enabled)
        {
            p.Text(18, by, text, UiFont(), Black);
        }
        else
        {
            p.TextEmbossed(18, by, text, UiFont(), sh);
        }
    }

    void CheckBox::OnMouseDown(const MouseEvent&)
    {
        down_ = true;
        Invalidate();
    }

    void CheckBox::OnMouseUp(const MouseEvent& e)
    {
        const bool fire = down_ && e.x >= 0 && e.y >= 0 && e.x < rect.w && e.y < rect.h;
        down_ = false;
        Invalidate();
        if (fire)
        {
            checked = !checked;
            if (onChange)
            {
                const bool v = checked;
                auto cb = onChange;
                root->Post([cb, v]() { cb(v); });
            }
        }
    }

    void RadioButton::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        const int by = (rect.h - 12) / 2;
        for (std::size_t j = 0; j < RadioRing.size(); ++j)
        {
            const char* row = RadioRing[j];
            for (int i = 0; row[i] != '\0'; ++i)
            {
                Color c = 0;
                switch (row[i])
                {
                case 'S': c = sh.shadow; break;
                case 'D': c = sh.dark; break;
                case 'W': c = enabled ? White : sh.face; break;
                case 'L': c = sh.light; break;
                case 'H': c = sh.hilite; break;
                default: continue;
                }
                p.Pixel(i, by + int(j), c);
            }
        }
        const bool on = group != nullptr && *group == value;
        if (on)
        {
            p.DrawGlyph(4, by + 4, glyphs::Dot, enabled ? Black : sh.shadow);
        }
        const int ty = (rect.h - 13) / 2;
        if (enabled)
        {
            p.Text(17, ty, text, UiFont(), Black);
        }
        else
        {
            p.TextEmbossed(17, ty, text, UiFont(), sh);
        }
    }

    void RadioButton::OnMouseUp(const MouseEvent& e)
    {
        if (e.x < 0 || e.y < 0 || e.x >= rect.w || e.y >= rect.h || group == nullptr)
        {
            return;
        }
        if (*group != value)
        {
            *group = value;
            Invalidate();
            if (onChange)
            {
                auto cb = onChange;
                const int v = value;
                root->Post([cb, v]() { cb(v); });
            }
        }
    }

    void GroupBox::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        p.EtchedFrame(Rect{0, 6, rect.w, rect.h - 6}, sh);
        const int tw = TextW(text, UiFont());
        p.Fill(Rect{6, 0, tw + 4, 13}, sh.face);
        p.Text(8, 0, text, UiFont(), enabled ? Black : sh.shadow);
    }

    void TextField::SetText(const std::wstring& value)
    {
        text = value;
        caret_ = anchor_ = int(text.size());
        scroll_ = 0;
        EnsureVisible();
        Invalidate();
    }

    void TextField::SelectAll()
    {
        anchor_ = 0;
        caret_ = int(text.size());
        EnsureVisible();
        Invalidate();
    }

    int TextField::PrefixWidth(int index) const
    {
        index = std::clamp(index, 0, int(text.size()));
        return TextW(text.substr(0, std::size_t(index)), UiFont());
    }

    int TextField::IndexAt(int localX) const
    {
        const int x = localX - 4 + scroll_;
        const int n = int(text.size());
        int prev = 0;
        for (int i = 1; i <= n; ++i)
        {
            const int cur = PrefixWidth(i);
            if (x < (prev + cur) / 2)
            {
                return i - 1;
            }
            prev = cur;
        }
        return n;
    }

    void TextField::EnsureVisible()
    {
        const int view = std::max(1, rect.w - 8);
        const int cx = PrefixWidth(caret_);
        const int total = PrefixWidth(int(text.size()));
        if (total <= view)
        {
            scroll_ = 0;
            return;
        }
        if (cx - scroll_ > view)
        {
            scroll_ = cx - view;
        }
        if (cx - scroll_ < 0)
        {
            scroll_ = cx;
        }
        scroll_ = std::clamp(scroll_, 0, std::max(0, total - view + 1));
    }

    void TextField::DeleteSelection()
    {
        const int a = std::min(caret_, anchor_);
        const int b = std::max(caret_, anchor_);
        text.erase(std::size_t(a), std::size_t(b - a));
        caret_ = anchor_ = a;
    }

    void TextField::Insert(const std::wstring& s)
    {
        if (HasSelection())
        {
            DeleteSelection();
        }
        std::wstring clean;
        for (wchar_t ch : s)
        {
            if (ch >= 32 && ch != 127)
            {
                clean.push_back(ch);
            }
        }
        const int room = maxLength - int(text.size());
        if (room <= 0)
        {
            return;
        }
        if (int(clean.size()) > room)
        {
            clean.resize(std::size_t(room));
        }
        text.insert(std::size_t(caret_), clean);
        caret_ += int(clean.size());
        anchor_ = caret_;
    }

    void TextField::Changed()
    {
        EnsureVisible();
        Blink();
        Invalidate();
        if (onChange)
        {
            auto cb = onChange;
            const std::wstring v = text;
            root->Post([cb, v]() { cb(v); });
        }
    }

    void TextField::Blink()
    {
        caretOn_ = true;
        lastBlink_ = root != nullptr ? root->now : 0;
    }

    void TextField::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        const Color fill = enabled ? White : sh.face;
        p.Sunken(Rect{0, 0, rect.w, rect.h}, sh, &fill);
        const Rect inner{2, 2, rect.w - 4, rect.h - 4};
        Painter c = p.Clipped(inner);
        const int ty = (rect.h - 13) / 2;
        const int tx = 4 - scroll_;
        if (!enabled)
        {
            c.TextEmbossed(tx, ty, text, UiFont(), sh);
            return;
        }
        c.Text(tx, ty, text, UiFont(), Black);
        if (HasSelection() && IsFocused())
        {
            const int a = std::min(caret_, anchor_);
            const int b = std::max(caret_, anchor_);
            const int sx = tx + PrefixWidth(a);
            const int ex = tx + PrefixWidth(b);
            const Rect sel{sx, ty, ex - sx, 13};
            c.Fill(sel, Navy);
            Painter s = c.Clipped(sel);
            s.Text(tx, ty, text, UiFont(), White);
        }
        if (IsFocused() && caretOn_)
        {
            c.VLine(tx + PrefixWidth(caret_), ty, 13, Black);
        }
    }

    void TextField::OnMouseDown(const MouseEvent& e)
    {
        if (e.clicks >= 2)
        {
            SelectAll();
            dragging_ = false;
            return;
        }
        caret_ = IndexAt(e.x);
        if (!e.shift)
        {
            anchor_ = caret_;
        }
        dragging_ = true;
        EnsureVisible();
        Blink();
        Invalidate();
    }

    void TextField::OnMouseMove(const MouseEvent& e)
    {
        if (!dragging_)
        {
            return;
        }
        const int c = IndexAt(e.x);
        if (c != caret_)
        {
            caret_ = c;
            EnsureVisible();
            Invalidate();
        }
    }

    void TextField::OnMouseUp(const MouseEvent&)
    {
        dragging_ = false;
    }

    bool TextField::OnKey(const KeyEvent& e)
    {
        const int n = int(text.size());
        const auto move = [&](int to) {
            if (!e.shift && HasSelection() && (e.key == KeyCode::Left || e.key == KeyCode::Right))
            {
                caret_ = e.key == KeyCode::Left ? std::min(caret_, anchor_) : std::max(caret_, anchor_);
            }
            else
            {
                caret_ = std::clamp(to, 0, n);
            }
            if (!e.shift)
            {
                anchor_ = caret_;
            }
            EnsureVisible();
            Blink();
            Invalidate();
        };
        switch (e.key)
        {
        case KeyCode::Left:
            move(caret_ - 1);
            return true;
        case KeyCode::Right:
            move(caret_ + 1);
            return true;
        case KeyCode::Home:
            move(0);
            return true;
        case KeyCode::End:
            move(n);
            return true;
        case KeyCode::Backspace:
            if (HasSelection())
            {
                DeleteSelection();
            }
            else if (caret_ > 0)
            {
                text.erase(std::size_t(caret_ - 1), 1);
                caret_ = anchor_ = caret_ - 1;
            }
            Changed();
            return true;
        case KeyCode::Delete:
            if (HasSelection())
            {
                DeleteSelection();
            }
            else if (caret_ < n)
            {
                text.erase(std::size_t(caret_), 1);
            }
            Changed();
            return true;
        case KeyCode::Enter:
            if (onEnter)
            {
                root->Post(onEnter);
                return true;
            }
            return false;
        case KeyCode::Letter:
            if (!e.ctrl)
            {
                return false;
            }
            if (e.letter == L'A')
            {
                SelectAll();
                return true;
            }
            if ((e.letter == L'C' || e.letter == L'X') && HasSelection())
            {
                const int a = std::min(caret_, anchor_);
                const int b = std::max(caret_, anchor_);
                if (GetPlatform() != nullptr)
                {
                    GetPlatform()->SetClipboardText(text.substr(std::size_t(a), std::size_t(b - a)));
                }
                if (e.letter == L'X')
                {
                    DeleteSelection();
                    Changed();
                }
                return true;
            }
            if (e.letter == L'V')
            {
                if (GetPlatform() != nullptr)
                {
                    Insert(GetPlatform()->ClipboardText());
                    Changed();
                }
                return true;
            }
            return e.letter == L'C' || e.letter == L'X';
        default:
            return false;
        }
    }

    bool TextField::OnChar(wchar_t ch)
    {
        if (ch < 32 || ch == 127)
        {
            return false;
        }
        Insert(std::wstring(1, ch));
        Changed();
        return true;
    }

    void TextField::OnFocus(bool focused)
    {
        if (focused)
        {
            Blink();
        }
        else
        {
            anchor_ = caret_;
            dragging_ = false;
        }
        Invalidate();
    }

    void TextField::OnTick(unsigned long long ms)
    {
        if (ms - lastBlink_ >= 530)
        {
            caretOn_ = !caretOn_;
            lastBlink_ = ms;
            Invalidate();
        }
    }

    void ComboBox::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        const Color fill = enabled ? White : sh.face;
        p.Sunken(Rect{0, 0, rect.w, rect.h}, sh, &fill);
        const std::wstring s = (index >= 0 && index < int(items.size())) ? items[std::size_t(index)] : std::wstring();
        const int ty = (rect.h - 13) / 2;
        const std::wstring t = Ellipsize(s, UiFont(), rect.w - 26);
        if (enabled)
        {
            p.Text(4, ty, t, UiFont(), Black);
        }
        else
        {
            p.TextEmbossed(4, ty, t, UiFont(), sh);
        }
        const Rect b{rect.w - 18, 2, 16, rect.h - 4};
        if (open)
        {
            p.Pressed(b, sh);
        }
        else
        {
            p.Raised(b, sh);
        }
        const int o = open ? 1 : 0;
        p.DrawGlyph(b.x + 4 + o, b.y + (b.h - 4) / 2 + o, glyphs::Down, enabled ? Black : sh.shadow);
    }

    void ComboBox::OnMouseDown(const MouseEvent&)
    {
        if (items.empty())
        {
            return;
        }
        auto popup = std::make_unique<ListPopup>();
        popup->items = items;
        popup->hoverIndex = index;
        const int n = int(items.size());
        popup->visibleCount = std::min(n, 12);
        popup->offset = std::clamp(index - popup->visibleCount + 1, 0, std::max(0, n - popup->visibleCount));
        const int h = popup->visibleCount * 16 + 2;
        int y = rect.Bottom();
        if (y + h > root->height)
        {
            y = rect.y - h;
        }
        popup->rect = Rect{rect.x, y, rect.w, h};
        ComboBox* self = this;
        popup->onPick = [self](int i) {
            self->index = i;
            self->Invalidate();
            if (self->onChange)
            {
                self->onChange(i);
            }
        };
        popup->onClosed = [self]() {
            self->open = false;
            self->Invalidate();
        };
        open = true;
        root->AddOverlay(std::move(popup));
        Invalidate();
    }

    void ComboBox::OnWheel(int delta)
    {
        if (items.empty())
        {
            return;
        }
        const int next = std::clamp(index + (delta > 0 ? -1 : 1), 0, int(items.size()) - 1);
        if (next != index)
        {
            index = next;
            Invalidate();
            if (onChange)
            {
                auto cb = onChange;
                root->Post([cb, next]() { cb(next); });
            }
        }
    }

    ListPopup::~ListPopup()
    {
        if (onClosed)
        {
            onClosed();
        }
    }

    void ListPopup::Paint(Painter& p)
    {
        p.Fill(Rect{0, 0, rect.w, rect.h}, White);
        p.Frame(Rect{0, 0, rect.w, rect.h}, Black);
        for (int k = 0; k < visibleCount; ++k)
        {
            const int i = offset + k;
            if (i >= int(items.size()))
            {
                break;
            }
            const Rect r{1, 1 + k * 16, rect.w - 2, 16};
            const bool hot = i == hoverIndex;
            if (hot)
            {
                p.Fill(r, Navy);
            }
            p.Text(r.x + 3, r.y + 2, Ellipsize(items[std::size_t(i)], UiFont(), r.w - 6), UiFont(), hot ? White : Black);
        }
    }

    void ListPopup::OnMouseMove(const MouseEvent& e)
    {
        const int k = (e.y - 1) / 16;
        const int i = offset + k;
        if (e.y >= 1 && k < visibleCount && i < int(items.size()) && i != hoverIndex)
        {
            hoverIndex = i;
            Invalidate();
        }
    }

    void ListPopup::OnMouseUp(const MouseEvent& e)
    {
        if (e.x < 0 || e.y < 1 || e.x >= rect.w || e.y >= rect.h - 1)
        {
            return;
        }
        const int i = offset + (e.y - 1) / 16;
        if (i >= 0 && i < int(items.size()))
        {
            auto cb = onPick;
            root->CloseOverlays();
            if (cb)
            {
                root->Post([cb, i]() { cb(i); });
            }
        }
    }

    void ListPopup::OnWheel(int delta)
    {
        const int n = int(items.size());
        offset = std::clamp(offset + (delta > 0 ? -1 : 1), 0, std::max(0, n - visibleCount));
        Invalidate();
    }

    bool ListPopup::OnKey(const KeyEvent& e)
    {
        const int n = int(items.size());
        if (e.key == KeyCode::Down || e.key == KeyCode::Up)
        {
            hoverIndex = std::clamp(hoverIndex + (e.key == KeyCode::Down ? 1 : -1), 0, n - 1);
            if (hoverIndex < offset)
            {
                offset = hoverIndex;
            }
            if (hoverIndex >= offset + visibleCount)
            {
                offset = hoverIndex - visibleCount + 1;
            }
            Invalidate();
            return true;
        }
        if (e.key == KeyCode::Enter && hoverIndex >= 0 && hoverIndex < n)
        {
            auto cb = onPick;
            const int i = hoverIndex;
            root->CloseOverlays();
            if (cb)
            {
                root->Post([cb, i]() { cb(i); });
            }
            return true;
        }
        return false;
    }

    void Swatch::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        const Rect r{0, 0, rect.w, rect.h};
        const bool down = down_ && IsCaptured();
        if (down)
        {
            p.Pressed(r, sh);
        }
        else
        {
            p.Raised(r, sh);
        }
        const int o = down ? 1 : 0;
        const Rect inner{4 + o, 4 + o, rect.w - 8, rect.h - 8};
        if (enabled)
        {
            p.Fill(inner, color);
            p.Frame(inner, Black);
        }
        else
        {
            p.Frame(inner, sh.shadow);
        }
    }

    void Swatch::OnMouseDown(const MouseEvent&)
    {
        down_ = true;
        Invalidate();
    }

    void Swatch::OnMouseUp(const MouseEvent& e)
    {
        const bool fire = down_ && e.x >= 0 && e.y >= 0 && e.x < rect.w && e.y < rect.h;
        down_ = false;
        Invalidate();
        if (fire && onClick)
        {
            root->Post(onClick);
        }
    }

    void IconCell::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        const Rect r{0, 0, rect.w, rect.h};
        const int o = selected ? 1 : 0;
        if (selected)
        {
            p.Pressed(r, sh);
            p.Dither(r.Inflate(-2), sh.face, sh.hilite);
        }
        else
        {
            p.Fill(r, sh.face);
        }
        if (!icon.Empty())
        {
            const Image img = enabled ? icon : DisabledIcon(icon, sh);
            p.Draw(img, (rect.w - icon.width) / 2 + o, (rect.h - icon.height) / 2 + o);
        }
        else if (!emptyText.empty())
        {
            const int tw = TextW(emptyText, UiFont());
            if (enabled)
            {
                p.Text((rect.w - tw) / 2 + o, (rect.h - 13) / 2 + o, emptyText, UiFont(), Black);
            }
            else
            {
                p.TextEmbossed((rect.w - tw) / 2, (rect.h - 13) / 2, emptyText, UiFont(), sh);
            }
        }
    }

    void IconCell::OnMouseUp(const MouseEvent& e)
    {
        if (e.x >= 0 && e.y >= 0 && e.x < rect.w && e.y < rect.h && onClick)
        {
            root->Post(onClick);
        }
    }

    std::vector<Rect> TabStrip::Cells() const
    {
        std::vector<Rect> cells;
        int x = 2;
        for (const auto& t : tabs)
        {
            const int w = std::max(56, TextW(t, UiFont()) + 16);
            cells.push_back(Rect{x, 2, w, rect.h - 3});
            x += w;
        }
        return cells;
    }

    void TabStrip::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        const auto cells = Cells();
        const auto drawTab = [&](Rect r, bool selected, const std::wstring& label) {
            if (selected)
            {
                r = Rect{r.x - 2, 0, r.w + 4, rect.h};
            }
            p.Fill(Rect{r.x + 1, r.y + 1, r.w - 2, r.h - 1}, sh.face);
            p.VLine(r.x, r.y + 2, r.h - 2, sh.hilite);
            p.Pixel(r.x + 1, r.y + 1, sh.hilite);
            p.HLine(r.x + 2, r.y, r.w - 4, sh.hilite);
            p.VLine(r.Right() - 2, r.y + 2, r.h - 2, sh.shadow);
            p.VLine(r.Right() - 1, r.y + 2, r.h - 2, sh.dark);
            p.Pixel(r.Right() - 2, r.y + 1, sh.dark);
            const int tw = TextW(label, UiFont());
            p.Text(r.x + (r.w - tw) / 2, r.y + (selected ? 4 : 3), label, UiFont(), Black);
        };
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            if (int(i) != index)
            {
                drawTab(cells[i], false, tabs[i]);
            }
        }
        if (index >= 0 && index < int(cells.size()))
        {
            drawTab(cells[std::size_t(index)], true, tabs[std::size_t(index)]);
        }
    }

    void TabStrip::OnMouseDown(const MouseEvent& e)
    {
        const auto cells = Cells();
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            if (cells[i].Inflate(1).Contains(e.x, e.y) && int(i) != index)
            {
                index = int(i);
                Invalidate();
                if (onChange)
                {
                    auto cb = onChange;
                    const int v = index;
                    root->Post([cb, v]() { cb(v); });
                }
                return;
            }
        }
    }

    void Panel::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        const Rect r{0, 0, rect.w, rect.h};
        switch (kind)
        {
        case Kind::Raised:
            if (useFill)
            {
                p.Fill(r, fillColor);
            }
            p.Edge(r, sh.hilite, sh.dark);
            p.Edge(r.Inflate(-1), sh.light, sh.shadow);
            break;
        case Kind::Sunken:
            p.Sunken(r, sh, useFill ? &fillColor : nullptr);
            break;
        case Kind::ThinSunken:
            if (useFill)
            {
                p.Fill(r, fillColor);
            }
            p.ThinSunken(r, sh);
            break;
        case Kind::EtchedH:
            p.EtchedH(0, 0, rect.w, sh);
            break;
        case Kind::EtchedV:
            p.EtchedV(0, 0, rect.h, sh);
            break;
        }
    }

    void PreviewPane::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        p.Sunken(Rect{0, 0, rect.w, rect.h}, sh, &background);
        Painter c = p.Clipped(Rect{2, 2, rect.w - 4, rect.h - 4});
        if (!image.Empty())
        {
            c.Draw(image, (rect.w - image.width) / 2, (rect.h - image.height) / 2);
            return;
        }
        const auto lines = SplitLines(message);
        int y = (rect.h - int(lines.size()) * 15) / 2;
        for (const auto& line : lines)
        {
            const int tw = TextW(line, UiFont());
            c.Text((rect.w - tw) / 2, y, line, UiFont(), White);
            y += 15;
        }
    }

    namespace
    {
        constexpr int ThumbItemW = 104;
    }

    int ThumbList::ContentWidth() const
    {
        return int(items.size()) * ThumbItemW + 8;
    }

    int ThumbList::ViewWidth() const
    {
        return rect.w - 4;
    }

    bool ThumbList::NeedsScroll() const
    {
        return ContentWidth() > ViewWidth();
    }

    Rect ThumbList::TrackRect() const
    {
        return Rect{2 + 16, rect.h - 2 - 16, rect.w - 4 - 32, 16};
    }

    Rect ThumbList::ThumbRect() const
    {
        const Rect t = TrackRect();
        const int content = ContentWidth();
        const int view = ViewWidth();
        const int size = std::max(8, t.w * view / std::max(1, content));
        const int range = std::max(1, content - view);
        const int pos = (t.w - size) * scroll_ / range;
        return Rect{t.x + pos, t.y, size, t.h};
    }

    void ThumbList::ScrollTo(int value)
    {
        const int maxScroll = std::max(0, ContentWidth() - ViewWidth());
        const int v = std::clamp(value, 0, maxScroll);
        if (v != scroll_)
        {
            scroll_ = v;
            Invalidate();
        }
    }

    void ThumbList::EnsureVisible(int index)
    {
        if (!NeedsScroll())
        {
            scroll_ = 0;
            return;
        }
        if (index < 0)
        {
            return;
        }
        const int left = 4 + index * ThumbItemW;
        const int right = left + ThumbItemW;
        if (left - scroll_ < 0)
        {
            ScrollTo(left - 4);
        }
        else if (right - scroll_ > ViewWidth())
        {
            ScrollTo(right - ViewWidth() + 4);
        }
    }

    void ThumbList::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        p.Sunken(Rect{0, 0, rect.w, rect.h}, sh, &White);
        const bool scroll = NeedsScroll();
        if (!scroll)
        {
            scroll_ = 0;
        }
        const int areaH = rect.h - 4 - (scroll ? 16 : 0);
        Painter c = p.Clipped(Rect{2, 2, rect.w - 4, areaH});
        for (int i = 0; i < int(items.size()); ++i)
        {
            const int x = 2 + 4 + i * ThumbItemW - scroll_;
            const int y = 2 + 4;
            if (x + ThumbItemW < 0 || x > rect.w)
            {
                continue;
            }
            const Rect box{x + 12, y, 80, 52};
            const bool sel = i == selected;
            if (sel)
            {
                c.Frame(box, Navy);
                c.Frame(box.Inflate(-1), Navy);
            }
            else
            {
                c.Frame(box, sh.shadow);
            }
            const Image& t = items[std::size_t(i)].thumb;
            c.Draw(t, box.x + (box.w - t.width) / 2, box.y + (box.h - t.height) / 2);
            const std::wstring name = Ellipsize(items[std::size_t(i)].name, UiFont(), ThumbItemW - 6);
            const int tw = TextW(name, UiFont());
            const int nx = x + (ThumbItemW - tw) / 2;
            const int ny = y + 56;
            if (sel)
            {
                c.Fill(Rect{nx - 2, ny, tw + 4, 14}, Navy);
                c.Text(nx, ny, name, UiFont(), White);
            }
            else
            {
                c.Text(nx, ny, name, UiFont(), Black);
            }
        }
        if (scroll)
        {
            const Rect lb{2, rect.h - 2 - 16, 16, 16};
            const Rect rb{rect.w - 2 - 16, rect.h - 2 - 16, 16, 16};
            p.Raised(lb, sh);
            p.DrawGlyph(lb.x + 6, lb.y + 4, glyphs::Left, Black);
            p.Raised(rb, sh);
            p.DrawGlyph(rb.x + 6, rb.y + 4, glyphs::Right, Black);
            p.Dither(TrackRect(), sh.hilite, sh.face);
            p.Raised(ThumbRect(), sh);
        }
    }

    void ThumbList::OnMouseDown(const MouseEvent& e)
    {
        if (NeedsScroll() && e.y >= rect.h - 2 - 16)
        {
            if (e.x < 2 + 16)
            {
                ScrollTo(scroll_ - ThumbItemW);
            }
            else if (e.x >= rect.w - 2 - 16)
            {
                ScrollTo(scroll_ + ThumbItemW);
            }
            else
            {
                const Rect th = ThumbRect();
                if (th.Contains(e.x, e.y))
                {
                    draggingThumb_ = true;
                    dragStartX_ = e.x;
                    dragStartScroll_ = scroll_;
                }
                else if (e.x < th.x)
                {
                    ScrollTo(scroll_ - ViewWidth());
                }
                else
                {
                    ScrollTo(scroll_ + ViewWidth());
                }
            }
            return;
        }
        const int i = (e.x - 6 + scroll_) / ThumbItemW;
        if (e.x - 6 + scroll_ >= 0 && i >= 0 && i < int(items.size()) && i != selected)
        {
            selected = i;
            Invalidate();
            if (onSelect)
            {
                auto cb = onSelect;
                root->Post([cb, i]() { cb(i); });
            }
        }
    }

    void ThumbList::OnMouseMove(const MouseEvent& e)
    {
        if (!draggingThumb_)
        {
            return;
        }
        const Rect t = TrackRect();
        const int size = ThumbRect().w;
        const int range = std::max(1, ContentWidth() - ViewWidth());
        const int dx = e.x - dragStartX_;
        ScrollTo(dragStartScroll_ + dx * range / std::max(1, t.w - size));
    }

    void ThumbList::OnMouseUp(const MouseEvent&)
    {
        draggingThumb_ = false;
    }

    void ThumbList::OnWheel(int delta)
    {
        ScrollTo(scroll_ + (delta > 0 ? -ThumbItemW / 2 : ThumbItemW / 2));
    }

    void StatusBar::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        int fixed = 0;
        for (std::size_t i = 1; i < fields.size(); ++i)
        {
            fixed += fields[i].width;
        }
        int x = 0;
        for (std::size_t i = 0; i < fields.size(); ++i)
        {
            int w = fields[i].width;
            if (i == 0)
            {
                w = rect.w - fixed - 2 * int(fields.size() - 1);
            }
            if (i + 1 == fields.size())
            {
                w = rect.w - x;
            }
            p.ThinSunken(Rect{x, 2, w, rect.h - 2}, sh);
            const int reserve = (grip && i + 1 == fields.size()) ? 16 : 4;
            p.Text(x + 3, 4, Ellipsize(fields[i].text, UiFont(), w - 3 - reserve), UiFont(), Black);
            x += w + 2;
        }
        if (grip)
        {
            const int gx = rect.w - 13;
            const int gy = rect.h - 13;
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
    }

    std::vector<Rect> MenuBar::Cells() const
    {
        std::vector<Rect> cells;
        int x = 0;
        for (const auto& m : menus)
        {
            const int w = TextW(m.title, UiFont()) + 12;
            cells.push_back(Rect{x, 0, w, rect.h});
            x += w;
        }
        return cells;
    }

    void MenuBar::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        const auto cells = Cells();
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            const Rect& c = cells[i];
            const bool open = int(i) == openIndex;
            if (open)
            {
                p.ThinSunken(c, sh);
            }
            else if (int(i) == hot_ && openIndex < 0)
            {
                p.ThinRaised(c, sh);
            }
            const int o = open ? 1 : 0;
            p.TextUnderlined(c.x + 6 + o, (rect.h - 13) / 2 + o, menus[i].title, 0, UiFont(), Black);
        }
    }

    void MenuBar::OpenMenu(int index)
    {
        if (index < 0 || index >= int(menus.size()))
        {
            return;
        }
        const auto cells = Cells();
        auto popup = std::make_unique<MenuPopup>();
        popup->items = menus[std::size_t(index)].items;
        popup->owner = this;
        popup->rect = MenuPopup::Measure(popup->items, rect.x + cells[std::size_t(index)].x, rect.Bottom());
        openIndex = index;
        root->ReplaceOverlay(std::move(popup));
        Invalidate();
    }

    void MenuBar::OnMouseDown(const MouseEvent& e)
    {
        const auto cells = Cells();
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            if (cells[i].Contains(e.x, e.y))
            {
                OpenMenu(int(i));
                return;
            }
        }
    }

    void MenuBar::OnMouseMove(const MouseEvent& e)
    {
        const auto cells = Cells();
        int h = -1;
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            if (cells[i].Contains(e.x, e.y))
            {
                h = int(i);
            }
        }
        if (h != hot_)
        {
            hot_ = h;
            Invalidate();
        }
        if (openIndex >= 0 && h >= 0 && h != openIndex)
        {
            OpenMenu(h);
        }
    }

    void MenuBar::OnMouseLeave()
    {
        hot_ = -1;
        Invalidate();
    }

    MenuPopup::~MenuPopup()
    {
        if (owner != nullptr && owner->openIndex >= 0)
        {
            bool mine = false;
            if (owner->root != nullptr)
            {
                mine = owner->root->TopOverlay() == this || owner->root->TopOverlay() == nullptr;
            }
            if (mine)
            {
                owner->openIndex = -1;
                owner->Invalidate();
            }
        }
    }

    Rect MenuPopup::Measure(const std::vector<MenuItem>& items, int x, int y)
    {
        int textW = 0;
        int keyW = 0;
        int h = 6;
        for (const auto& it : items)
        {
            if (it.separator)
            {
                h += 9;
                continue;
            }
            textW = std::max(textW, TextW(it.text, UiFont()));
            keyW = std::max(keyW, TextW(it.shortcut, UiFont()));
            h += 17;
        }
        const int w = 22 + textW + (keyW > 0 ? keyW + 24 : 0) + 18;
        return Rect{x, y, std::max(w, 120), h};
    }

    int MenuPopup::ItemTop(int index) const
    {
        int y = 3;
        for (int i = 0; i < index; ++i)
        {
            y += items[std::size_t(i)].separator ? 9 : 17;
        }
        return y;
    }

    int MenuPopup::ItemAt(int y) const
    {
        int top = 3;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const int h = items[i].separator ? 9 : 17;
            if (y >= top && y < top + h)
            {
                return items[i].separator ? -1 : int(i);
            }
            top += h;
        }
        return -1;
    }

    void MenuPopup::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        p.Fill(Rect{0, 0, rect.w, rect.h}, sh.face);
        p.WindowEdge(Rect{0, 0, rect.w, rect.h}, sh);
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const MenuItem& it = items[i];
            const int top = ItemTop(int(i));
            if (it.separator)
            {
                p.EtchedH(2, top + 3, rect.w - 4, sh);
                continue;
            }
            const bool hot = int(i) == hoverIndex && it.enabled;
            if (hot)
            {
                p.Fill(Rect{3, top, rect.w - 6, 17}, Navy);
            }
            const int ty = top + 2;
            if (!it.enabled)
            {
                p.TextEmbossed(20, ty, it.text, UiFont(), sh);
                if (!it.shortcut.empty())
                {
                    p.TextEmbossed(rect.w - 12 - TextW(it.shortcut, UiFont()), ty, it.shortcut, UiFont(), sh);
                }
                continue;
            }
            const Color c = hot ? White : Black;
            p.Text(20, ty, it.text, UiFont(), c);
            if (!it.shortcut.empty())
            {
                p.Text(rect.w - 12 - TextW(it.shortcut, UiFont()), ty, it.shortcut, UiFont(), c);
            }
        }
    }

    void MenuPopup::OnMouseMove(const MouseEvent& e)
    {
        const int i = ItemAt(e.y);
        if (i != hoverIndex)
        {
            hoverIndex = i;
            Invalidate();
        }
    }

    void MenuPopup::OnMouseLeave()
    {
        hoverIndex = -1;
        Invalidate();
    }

    void MenuPopup::OnMouseUp(const MouseEvent& e)
    {
        if (e.x < 0 || e.x >= rect.w)
        {
            return;
        }
        const int i = ItemAt(e.y);
        if (i < 0 || !items[std::size_t(i)].enabled)
        {
            return;
        }
        auto action = items[std::size_t(i)].action;
        root->CloseOverlays();
        if (action)
        {
            root->Post(action);
        }
    }

    bool MenuPopup::OnKey(const KeyEvent& e)
    {
        const int n = int(items.size());
        if (e.key == KeyCode::Down || e.key == KeyCode::Up)
        {
            const int step = e.key == KeyCode::Down ? 1 : -1;
            int i = hoverIndex;
            for (int k = 0; k < n; ++k)
            {
                i = (i + step + n) % n;
                if (!items[std::size_t(i)].separator && items[std::size_t(i)].enabled)
                {
                    break;
                }
            }
            hoverIndex = i;
            Invalidate();
            return true;
        }
        if (e.key == KeyCode::Enter && hoverIndex >= 0 && hoverIndex < n)
        {
            auto action = items[std::size_t(hoverIndex)].action;
            root->CloseOverlays();
            if (action)
            {
                root->Post(action);
            }
            return true;
        }
        if ((e.key == KeyCode::Left || e.key == KeyCode::Right) && owner != nullptr)
        {
            const int m = int(owner->menus.size());
            const int next = (owner->openIndex + (e.key == KeyCode::Right ? 1 : -1) + m) % m;
            owner->OpenMenu(next);
            return true;
        }
        return false;
    }

    void ProgressBar::Paint(Painter& p)
    {
        const Shades& sh = Sh();
        p.ThinSunken(Rect{0, 0, rect.w, rect.h}, sh);
        const int innerW = rect.w - 4;
        const int bw = std::max(4, (rect.h - 4) * 2 / 3);
        const int count = int(std::clamp(value, 0.0, 1.0) * innerW) / (bw + 2);
        for (int i = 0; i < count; ++i)
        {
            p.Fill(Rect{2 + i * (bw + 2), 2, bw, rect.h - 4}, Navy);
        }
    }

    void GlyphView::Paint(Painter& p)
    {
        if (!image.Empty())
        {
            p.Draw(image, 0, 0);
        }
        if (glyph != nullptr)
        {
            p.DrawGlyph(0, 0, *glyph, color);
        }
        if (mark != nullptr)
        {
            p.DrawGlyph(markX, markY, *mark, markColor);
        }
    }
}
