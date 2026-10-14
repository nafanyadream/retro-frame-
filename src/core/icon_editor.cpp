#include "icon_editor.h"

#include "icons.h"

namespace rf
{
    namespace
    {
        constexpr int Cell = 16;
        constexpr Color Transparent = Color(0);

        const Color PaletteColors[28] = {
            Rgb(0x00, 0x00, 0x00), Rgb(0x80, 0x80, 0x80), Rgb(0x80, 0x00, 0x00), Rgb(0x80, 0x80, 0x00), Rgb(0x00, 0x80, 0x00), Rgb(0x00, 0x80, 0x80),
            Rgb(0x00, 0x00, 0x80), Rgb(0x80, 0x00, 0x80), Rgb(0x80, 0x80, 0x40), Rgb(0x00, 0x40, 0x40), Rgb(0x00, 0x80, 0xFF), Rgb(0x00, 0x40, 0x80),
            Rgb(0x80, 0x00, 0xFF), Rgb(0x80, 0x40, 0x00), Rgb(0xFF, 0xFF, 0xFF), Rgb(0xC0, 0xC0, 0xC0), Rgb(0xFF, 0x00, 0x00), Rgb(0xFF, 0xFF, 0x00),
            Rgb(0x00, 0xFF, 0x00), Rgb(0x00, 0xFF, 0xFF), Rgb(0x00, 0x00, 0xFF), Rgb(0xFF, 0x00, 0xFF), Rgb(0xFF, 0xFF, 0x80), Rgb(0x00, 0xFF, 0x80),
            Rgb(0x80, 0xFF, 0xFF), Rgb(0x80, 0x80, 0xFF), Rgb(0xFF, 0x00, 0x80), Rgb(0xFF, 0x80, 0x40),
        };

        Color Checker(int x, int y)
        {
            return ((x / 4) + (y / 4)) % 2 == 0 ? Rgb(0xFF, 0xFF, 0xFF) : Rgb(0xD8, 0xD8, 0xD8);
        }

        Image Swatch(Color c, int w, int h)
        {
            Image img(w, h);
            Painter p(img);
            const Shades sh;
            p.Sunken(Rect{0, 0, w, h}, sh);
            for (int y = 2; y < h - 2; ++y)
            {
                for (int x = 2; x < w - 2; ++x)
                {
                    img.Set(x, y, Alpha(c) == 0 ? Checker(x, y) : c);
                }
            }
            return img;
        }

        void FloodFill(Image& img, int sx, int sy, Color to)
        {
            const Color from = img.Get(sx, sy);
            if (from == to)
            {
                return;
            }
            std::vector<std::pair<int, int>> stack = {{sx, sy}};
            while (!stack.empty())
            {
                const auto [x, y] = stack.back();
                stack.pop_back();
                if (x < 0 || y < 0 || x >= img.width || y >= img.height || img.Get(x, y) != from)
                {
                    continue;
                }
                img.Set(x, y, to);
                stack.push_back({x + 1, y});
                stack.push_back({x - 1, y});
                stack.push_back({x, y + 1});
                stack.push_back({x, y - 1});
            }
        }

        class PaletteCell : public ui::Widget
        {
        public:
            Color color = 0;
            std::function<void(Color)> onPick;

            void Paint(Painter& p) override
            {
                const Shades& sh = Sh();
                p.Sunken(Rect{0, 0, rect.w, rect.h}, sh);
                for (int y = 2; y < rect.h - 2; ++y)
                {
                    for (int x = 2; x < rect.w - 2; ++x)
                    {
                        p.Pixel(x, y, Alpha(color) == 0 ? Checker(x, y) : color);
                    }
                }
            }

            void OnMouseUp(const ui::MouseEvent& e) override
            {
                if (onPick && e.x >= 0 && e.y >= 0 && e.x < rect.w && e.y < rect.h)
                {
                    root->Post([cb = onPick, c = color]() { cb(c); });
                }
            }
        };
    }

    class IconCanvas : public ui::Widget
    {
    public:
        const Image* icon = nullptr;
        std::function<void(int, int, bool)> onTouch;

        void Paint(Painter& p) override
        {
            const Shades& sh = Sh();
            p.Sunken(Rect{0, 0, rect.w, rect.h}, sh);
            for (int y = 0; y < 16; ++y)
            {
                for (int x = 0; x < 16; ++x)
                {
                    const Color c = icon != nullptr ? icon->Get(x, y) : Transparent;
                    const int px = 2 + x * Cell;
                    const int py = 2 + y * Cell;
                    for (int j = 0; j < Cell - 1; ++j)
                    {
                        for (int i = 0; i < Cell - 1; ++i)
                        {
                            p.Pixel(px + i, py + j, Alpha(c) == 0 ? Checker(i, j) : Opaque(Over(Checker(i, j), c)));
                        }
                    }
                    p.HLine(px, py + Cell - 1, Cell, Rgb(0xC0, 0xC0, 0xC0));
                    p.VLine(px + Cell - 1, py, Cell, Rgb(0xC0, 0xC0, 0xC0));
                }
            }
        }

        void OnMouseDown(const ui::MouseEvent& e) override { Touch(e, true); }
        void OnMouseMove(const ui::MouseEvent& e) override
        {
            if (IsCaptured())
            {
                Touch(e, false);
            }
        }

    private:
        void Touch(const ui::MouseEvent& e, bool start)
        {
            const int x = (e.x - 2) / Cell;
            const int y = (e.y - 2) / Cell;
            if (e.x >= 2 && e.y >= 2 && x < 16 && y < 16 && onTouch)
            {
                onTouch(x, y, start);
            }
        }
    };

    IconEditorView::IconEditorView(Host& host, const Image& start) : View(520, 362), host_(host)
    {
        icon_ = start.width == 16 && start.height == 16 ? start : start.Empty() ? Image(16, 16) : MakeIcon16(start);
        AddDialogChrome(*this, L"Редактор значка");
        ui::Root& r = root;
        const auto label = [&](int x, int y, int w, const std::wstring& text) {
            auto* l = r.Add<ui::Label>();
            l->rect = Rect{x, y, w, 13};
            l->text = text;
            return l;
        };
        const auto button = [&](int x, int y, int w, const std::wstring& text) {
            auto* b = r.Add<ui::Button>();
            b->rect = Rect{x, y, w, 23};
            b->text = text;
            return b;
        };
        canvas_ = r.Add<IconCanvas>();
        canvas_->rect = Rect{12, 32, 16 * Cell + 4, 16 * Cell + 4};
        canvas_->icon = &icon_;
        canvas_->onTouch = [this](int x, int y, bool start) { Touch(x, y, start); };

        label(286, 32, 120, L"Инструмент:");
        const wchar_t* names[] = {L"Карандаш", L"Заливка", L"Ластик", L"Пипетка"};
        for (int i = 0; i < 4; ++i)
        {
            tools_[i] = button(286 + (i % 2) * 112, 48 + (i / 2) * 27, 108, names[i]);
            tools_[i]->onClick = [this, i]() { SetTool(static_cast<IconTool>(i)); };
        }
        label(286, 110, 120, L"Цвет:");
        current_ = r.Add<ui::GlyphView>();
        current_->rect = Rect{286, 126, 36, 36};
        for (int i = 0; i < 28; ++i)
        {
            auto* cell = r.Add<PaletteCell>();
            cell->rect = Rect{330 + (i % 14) * 12, 126 + (i / 14) * 18, 12, 18};
            cell->color = PaletteColors[i];
            cell->onPick = [this](Color c) { SetColor(c); };
        }
        auto* clear = r.Add<PaletteCell>();
        clear->rect = Rect{286, 168, 36, 23};
        clear->color = Transparent;
        clear->onPick = [this](Color c) { SetColor(c); };
        label(330, 173, 80, L"прозрачный");
        auto* other = button(408, 168, 98, L"Другой цвет...");
        other->onClick = [this]() {
            Color c = Alpha(color_) == 0 ? Rgb(0, 0, 0) : color_;
            if (host_.AskColor(c, c))
            {
                SetColor(Opaque(c));
            }
        };
        label(286, 204, 200, L"Как будет выглядеть:");
        preview_ = r.Add<ui::GlyphView>();
        preview_->rect = Rect{286, 220, 222, 60};
        undo_ = button(286, 292, 108, L"Отменить");
        undo_->onClick = [this]() { Undo(); };
        auto* wipe = button(398, 292, 108, L"Очистить");
        wipe->onClick = [this]() { Clear(); };
        auto* ok = button(root.width - 14 - 75 - 6 - 75, root.height - 3 - 10 - 23, 75, L"OK");
        ok->isDefault = true;
        auto* cancel = button(root.width - 14 - 75, root.height - 3 - 10 - 23, 75, L"Отмена");
        View* self = this;
        ok->onClick = [self]() { self->Close(1); };
        cancel->onClick = [self]() { self->Close(0); };
        IconEditorView* me = this;
        root.keyHook = [me](const ui::KeyEvent& e) {
            if (e.ctrl && e.key == ui::KeyCode::Letter && (e.letter == L'Z' || e.letter == L'z'))
            {
                me->Undo();
                return true;
            }
            if (e.key == ui::KeyCode::Enter)
            {
                me->Close(1);
                return true;
            }
            if (e.key == ui::KeyCode::Escape)
            {
                me->Close(0);
                return true;
            }
            return false;
        };
        Sync();
    }

    void IconEditorView::Snapshot()
    {
        history_.push_back(icon_);
        if (history_.size() > 50)
        {
            history_.erase(history_.begin());
        }
    }

    void IconEditorView::SetTool(IconTool tool)
    {
        tool_ = tool;
        Sync();
    }

    void IconEditorView::SetColor(Color color)
    {
        color_ = color;
        if (tool_ == IconTool::Eraser || tool_ == IconTool::Picker)
        {
            tool_ = IconTool::Pencil;
        }
        Sync();
    }

    void IconEditorView::Touch(int x, int y, bool startStroke)
    {
        if (x < 0 || y < 0 || x >= 16 || y >= 16)
        {
            return;
        }
        if (tool_ == IconTool::Picker)
        {
            if (startStroke)
            {
                color_ = icon_.Get(x, y);
                tool_ = IconTool::Pencil;
                Sync();
            }
            return;
        }
        if (startStroke)
        {
            Snapshot();
        }
        if (tool_ == IconTool::Fill)
        {
            if (startStroke)
            {
                FloodFill(icon_, x, y, color_);
            }
        }
        else
        {
            icon_.Set(x, y, tool_ == IconTool::Eraser ? Transparent : color_);
        }
        Sync();
    }

    void IconEditorView::Undo()
    {
        if (history_.empty())
        {
            return;
        }
        icon_ = history_.back();
        history_.pop_back();
        Sync();
    }

    void IconEditorView::Clear()
    {
        Snapshot();
        icon_ = Image(16, 16);
        Sync();
    }

    void IconEditorView::Sync()
    {
        for (int i = 0; i < 4; ++i)
        {
            tools_[i]->toggled = int(tool_) == i;
        }
        current_->image = Swatch(color_, 36, 36);
        Image pv(preview_->rect.w, preview_->rect.h, ShadesFor(Rgb(0xC0, 0xC0, 0xC0), Style::Win98).face);
        Painter p(pv);
        p.Draw(icon_, 2, 2);
        p.Draw(ScaleNearest(icon_, 2), 24, 2);
        FrameColors colors = SchemeColors(0, Style::Win98);
        p.GradientH(70, 4, pv.width - 72, 18, colors.active1, colors.active2);
        p.Draw(icon_, 72, 5);
        p.Text(92, 7, L"Мой значок", Font{FontFamily::SansSerif, true}, colors.titleText);
        p.GradientH(70, 34, pv.width - 72, 18, Rgb(0x00, 0x54, 0xE3), Rgb(0x3D, 0x95, 0xFF));
        p.Draw(icon_, 72, 35);
        p.Text(92, 37, L"Мой значок", Font{FontFamily::Trebuchet, true}, colors.titleText);
        preview_->image = std::move(pv);
        undo_->enabled = !history_.empty();
        root.dirty = true;
    }
}
