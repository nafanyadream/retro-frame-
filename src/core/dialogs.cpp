#include "dialogs.h"

#include "glyphs.h"
#include "utf.h"

namespace rf
{
    namespace
    {
        std::vector<std::wstring> Lines(const std::wstring& s)
        {
            std::vector<std::wstring> out;
            std::wstring cur;
            for (wchar_t c : s)
            {
                if (c == L'\n')
                {
                    out.push_back(cur);
                    cur.clear();
                }
                else
                {
                    cur.push_back(c);
                }
            }
            out.push_back(cur);
            return out;
        }
    }

    void InstallDialogKeys(View& view, std::function<void()> onEnter, std::function<void()> onEscape)
    {
        view.root.keyHook = [onEnter, onEscape](const ui::KeyEvent& e) {
            if (e.key == ui::KeyCode::Enter && onEnter)
            {
                onEnter();
                return true;
            }
            if (e.key == ui::KeyCode::Escape && onEscape)
            {
                onEscape();
                return true;
            }
            return false;
        };
    }

    ui::TitleBar* AddDialogChrome(View& view, const std::wstring& title)
    {
        view.title = title;
        auto* border = view.root.Add<ui::WindowBorder>();
        border->rect = Rect{0, 0, view.root.width, view.root.height};
        auto* bar = view.root.Add<ui::TitleBar>();
        bar->rect = Rect{3, 3, view.root.width - 6, 18};
        bar->title = title;
        bar->showMinimize = false;
        bar->showMaximize = false;
        View* v = &view;
        bar->onClose = [v]() { v->Close(0); };
        return bar;
    }

    MessageView::MessageView(const std::wstring& title, const std::wstring& text, MessageKind kind)
        : View(300, 140)
    {
        const auto lines = Lines(text);
        int textW = 0;
        for (const auto& l : lines)
        {
            textW = std::max(textW, Text().Width(l, ui::UiFont()));
        }
        const int textH = int(lines.size()) * 13;
        const int w = std::clamp(14 + 32 + 16 + textW + 20, 260, 560);
        const int bodyH = std::max(32, textH);
        const int h = 3 + 18 + 14 + bodyH + 16 + 23 + 12 + 3;
        root.width = w;
        root.height = h;
        AddDialogChrome(*this, title);
        auto* icon = root.Add<ui::GlyphView>();
        icon->rect = Rect{14, 3 + 18 + 12, 32, 32};
        if (kind == MessageKind::Warning)
        {
            icon->glyph = &glyphs::Warning;
            icon->color = Rgb(0xFF, 0xFF, 0x00);
            icon->mark = &glyphs::WarningMark;
            icon->markColor = Rgb(0, 0, 0);
            icon->markX = 15;
            icon->markY = 12;
        }
        else
        {
            icon->glyph = &glyphs::Info;
            icon->color = Rgb(0xFF, 0xFF, 0xFF);
            icon->mark = &glyphs::InfoMark;
            icon->markColor = Rgb(0x00, 0x00, 0xFF);
            icon->markX = 14;
            icon->markY = 9;
        }
        auto* label = root.Add<ui::Label>();
        label->rect = Rect{14 + 32 + 16, 3 + 18 + 14 + (bodyH - textH) / 2, w - (14 + 32 + 16) - 14, textH};
        label->text = text;
        label->ellipsis = false;
        auto* ok = root.Add<ui::Button>();
        ok->rect = Rect{(w - 75) / 2, h - 3 - 12 - 23, 75, 23};
        ok->text = L"OK";
        ok->isDefault = true;
        View* self = this;
        ok->onClick = [self]() { self->Close(1); };
        InstallDialogKeys(*this, [self]() { self->Close(1); }, [self]() { self->Close(0); });
    }

    TextsView::TextsView(const FrameSettings& s)
        : View(420, 318)
    {
        AddDialogChrome(*this, L"Тексты окна");
        const int left = 14;
        const int fieldX = 82;
        const int fieldW = root.width - fieldX - 14;
        auto* g1 = root.Add<ui::GroupBox>();
        g1->rect = Rect{8, 28, root.width - 16, 50};
        g1->text = L"Строка меню";
        auto* l1 = root.Add<ui::Label>();
        l1->rect = Rect{left + 4, 50, 64, 13};
        l1->text = L"Пункты:";
        menu_ = root.Add<ui::TextField>();
        menu_->rect = Rect{fieldX, 46, fieldW - 4, 21};
        menu_->SetText(JoinList(s.menuItems, L", "));

        auto* g2 = root.Add<ui::GroupBox>();
        g2->rect = Rect{8, 84, root.width - 16, 112};
        g2->text = L"Строка состояния";
        for (int i = 0; i < 3; ++i)
        {
            auto* l = root.Add<ui::Label>();
            l->rect = Rect{left + 4, 106 + i * 24, 64, 13};
            l->text = L"Поле " + std::to_wstring(i + 1) + L":";
            fields_[i] = root.Add<ui::TextField>();
            fields_[i]->rect = Rect{fieldX, 102 + i * 24, fieldW - 4, 21};
            fields_[i]->SetText(i < int(s.statusFields.size()) ? s.statusFields[std::size_t(i)] : std::wstring());
        }
        icon_ = root.Add<ui::CheckBox>();
        icon_->rect = Rect{left + 4, 174, 280, 16};
        icon_->text = L"Значок компьютера в последнем поле";
        icon_->checked = s.statusIcon;

        auto* g3 = root.Add<ui::GroupBox>();
        g3->rect = Rect{8, 202, root.width - 16, 50};
        g3->text = L"Адресная строка";
        auto* l3 = root.Add<ui::Label>();
        l3->rect = Rect{left + 4, 224, 64, 13};
        l3->text = L"Адрес:";
        address_ = root.Add<ui::TextField>();
        address_->rect = Rect{fieldX, 220, fieldW - 4, 21};
        address_->SetText(s.addressText);

        auto* hint = root.Add<ui::Label>();
        hint->rect = Rect{left, 260, root.width - 28, 13};
        hint->text = L"В текстах можно писать {имя}, {ширина}, {высота}.";
        hint->color = Rgb(0x40, 0x40, 0x40);

        auto* ok = root.Add<ui::Button>();
        ok->rect = Rect{root.width - 14 - 75 - 6 - 75, root.height - 3 - 10 - 23, 75, 23};
        ok->text = L"OK";
        ok->isDefault = true;
        auto* cancel = root.Add<ui::Button>();
        cancel->rect = Rect{root.width - 14 - 75, root.height - 3 - 10 - 23, 75, 23};
        cancel->text = L"Отмена";
        View* self = this;
        ok->onClick = [self]() { self->Close(1); };
        cancel->onClick = [self]() { self->Close(0); };
        InstallDialogKeys(*this, [self]() { self->Close(1); }, [self]() { self->Close(0); });
        root.SetFocus(menu_);
        menu_->SelectAll();
    }

    void TextsView::Apply(FrameSettings& s) const
    {
        std::vector<std::wstring> items;
        for (const auto& item : SplitList(menu_->text, L','))
        {
            if (!item.empty())
            {
                items.push_back(item);
            }
        }
        s.menuItems = items;
        s.statusFields.clear();
        for (int i = 0; i < 3; ++i)
        {
            s.statusFields.push_back(fields_[i]->text);
        }
        s.statusIcon = icon_->checked;
        s.addressText = address_->text;
    }

    ProgressView::ProgressView(const std::wstring& title)
        : View(380, 136)
    {
        AddDialogChrome(*this, title);
        line1_ = root.Add<ui::Label>();
        line1_->rect = Rect{14, 34, root.width - 28, 13};
        line2_ = root.Add<ui::Label>();
        line2_->rect = Rect{14, 50, root.width - 28, 13};
        bar_ = root.Add<ui::ProgressBar>();
        bar_->rect = Rect{14, 70, root.width - 28, 18};
        auto* cancel = root.Add<ui::Button>();
        cancel->rect = Rect{(root.width - 75) / 2, 98, 75, 23};
        cancel->text = L"Отмена";
        ProgressView* self = this;
        cancel->onClick = [self]() { self->cancelled = true; };
        InstallDialogKeys(*this, nullptr, [self]() { self->cancelled = true; });
    }

    void ProgressView::Update(const std::wstring& line1, const std::wstring& line2, double value)
    {
        line1_->text = line1;
        line2_->text = line2;
        bar_->value = value;
        root.dirty = true;
    }

    void ShowMessage(Host& host, const std::wstring& title, const std::wstring& text, MessageKind kind)
    {
        MessageView view(title, text, kind);
        host.RunModal(view);
    }
}
