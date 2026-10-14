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

    PlayerTextsView::PlayerTextsView(const std::vector<std::wstring>& menu, const std::wstring& status, bool time, const std::wstring& address)
        : View(420, 262)
    {
        AddDialogChrome(*this, L"Тексты плеера");
        const int left = 14;
        const int fieldX = 82;
        const int fieldW = root.width - fieldX - 14;
        const auto group = [&](int y, int h, const std::wstring& text) {
            auto* g = root.Add<ui::GroupBox>();
            g->rect = Rect{8, y, root.width - 16, h};
            g->text = text;
        };
        const auto label = [&](int y, const std::wstring& text) {
            auto* l = root.Add<ui::Label>();
            l->rect = Rect{left + 4, y, 64, 13};
            l->text = text;
        };
        const auto field = [&](int y, const std::wstring& text) {
            auto* f = root.Add<ui::TextField>();
            f->rect = Rect{fieldX, y, fieldW - 4, 21};
            f->SetText(text);
            return f;
        };
        group(28, 50, L"Меню плеера");
        label(50, L"Пункты:");
        menu_ = field(46, JoinList(menu, L", "));
        group(84, 72, L"Строка состояния");
        label(106, L"Текст:");
        status_ = field(102, status);
        time_ = root.Add<ui::CheckBox>();
        time_->rect = Rect{fieldX, 130, fieldW - 4, 16};
        time_->text = L"Время ролика справа";
        time_->checked = time;
        group(162, 50, L"Адресная строка");
        label(184, L"Адрес:");
        address_ = field(180, address);
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

    EffectsView::EffectsView(const EffectSettings& e)
        : View(760, 352), base_(e)
    {
        AddDialogChrome(*this, L"Эффекты");
        const int colW = 368;
        const int leftX = 8;
        const int rightX = leftX + colW + 8;
        const int labelW = 84;
        const auto group = [&](int x, int y, int h, const std::wstring& text) {
            auto* g = root.Add<ui::GroupBox>();
            g->rect = Rect{x, y, colW, h};
            g->text = text;
        };
        const auto label = [&](int x, int y, int w, const std::wstring& text) {
            auto* l = root.Add<ui::Label>();
            l->rect = Rect{x, y + 4, w, 13};
            l->text = text;
            return l;
        };
        const auto combo = [&](int x, int y, int w, std::vector<std::wstring> items, int index) {
            auto* c = root.Add<ui::ComboBox>();
            c->rect = Rect{x, y, w, 21};
            c->items = std::move(items);
            c->index = std::clamp(index, 0, int(c->items.size()) - 1);
            return c;
        };
        const auto field = [&](int x, int y, int w, const std::wstring& text) {
            auto* f = root.Add<ui::TextField>();
            f->rect = Rect{x, y, w, 21};
            f->SetText(text);
            return f;
        };
        const auto check = [&](int x, int y, const std::wstring& text, bool on) {
            auto* c = root.Add<ui::CheckBox>();
            c->rect = Rect{x, y, colW - 20, 16};
            c->text = text;
            c->checked = on;
            return c;
        };
        const int lx = leftX + 10;
        const int lf = leftX + 10 + labelW;
        const int lw = colW - 20 - labelW;
        group(leftX, 28, 120, L"Окно в видео и музыке");
        intro_ = check(lx, 46, L"Анимация открытия окна в начале", e.intro);
        label(lx, 68, labelW, L"В конце:");
        outro_ = combo(lf, 68, lw, {L"ничего", L"окно закрывается", L"окно сворачивается"}, e.outro);
        label(lx, 94, labelW, L"Во время:");
        motion_ = combo(lf, 94, lw, {L"стоит на месте", L"дрожит", L"плавает по экрану"}, e.motion);
        label(lx, 120, colW - 20, L"Конец и движение видны в режиме «На фоне».");
        group(leftX, 154, 74, L"Курсор мыши");
        label(lx, 172, labelW, L"Вид:");
        cursor_ = combo(lf, 172, lw, {L"нет", L"стрелка", L"песочные часы", L"стрелка с часиками"}, e.cursor);
        label(lx, 198, labelW, L"Где:");
        place_ = combo(lf, 198, lw, {L"у крестика", L"в центре окна", L"справа внизу", L"на кнопке окна-ошибки", L"ездит сам и жмёт кнопки"}, e.cursorPlace);
        group(leftX, 234, 74, L"Экран (только картинка внутри окна)");
        label(lx, 252, labelW, L"Вид:");
        screen_ = combo(lf, 252, lw, {L"обычный", L"ЭЛТ-монитор", L"VHS-кассета"}, e.screen);
        label(lx, 278, labelW, L"Цвета:");
        palette_ = combo(lf, 278, lw, {L"все", L"256 цветов", L"16 цветов"}, e.palette);

        const int rx = rightX + 10;
        const int rf = rightX + 10 + labelW;
        const int rw = colW - 20 - labelW;
        group(rightX, 28, 164, L"Окна-ошибки поверх окна");
        label(rx, 46, labelW, L"Показывать:");
        popups_ = combo(rf, 46, rw, {L"нет", L"одно окно", L"каскад из 5 окон", L"каскад, по очереди в видео"}, e.popups);
        label(rx, 72, labelW, L"Значок:");
        icon_ = combo(rf, 72, 120, {L"ошибка", L"предупреждение", L"информация", L"вопрос"}, e.popupIcon);
        label(rf + 128, 72, 50, L"Кнопки:");
        buttons_ = combo(rf + 178, 72, rw - 178, {L"OK", L"OK и Отмена", L"Да и Нет"}, e.popupButtons);
        label(rx, 98, labelW, L"Заголовок:");
        title_ = field(rf, 98, rw, e.popupTitle);
        std::wstring first = e.popupText;
        std::wstring second;
        const std::size_t nl = first.find(L'\n');
        if (nl != std::wstring::npos)
        {
            second = first.substr(nl + 1);
            first.resize(nl);
            std::replace(second.begin(), second.end(), L'\n', L' ');
        }
        label(rx, 124, labelW, L"Текст:");
        line1_ = field(rf, 124, rw, first);
        line2_ = field(rf, 150, rw, second);
        group(rightX, 198, 110, L"Рабочий стол (в режиме «На фоне»)");
        taskbar_ = check(rx, 216, L"Панель задач: «Пуск», кнопка окна и часы", e.desktop);
        desktopIcons_ = check(rx, 238, L"Значки: «Мой компьютер», «Корзина»...", e.desktopIcons);
        label(rx, 260, labelW, L"Часы:");
        clock_ = field(rf, 260, 70, e.clock);
        label(rf + 78, 260, rw - 78, L"пусто — время сейчас");
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
    }

    EffectSettings EffectsView::Result() const
    {
        EffectSettings e = base_;
        e.intro = intro_->checked;
        e.outro = outro_->index;
        e.motion = motion_->index;
        e.cursor = cursor_->index;
        e.cursorPlace = place_->index;
        e.popups = popups_->index;
        e.popupIcon = icon_->index;
        e.popupButtons = buttons_->index;
        e.popupTitle = title_->text;
        e.popupText = line2_->text.empty() ? line1_->text : line1_->text + L"\n" + line2_->text;
        e.screen = screen_->index;
        e.palette = palette_->index;
        e.desktop = taskbar_->checked;
        e.desktopIcons = desktopIcons_->checked;
        e.clock = clock_->text;
        return e;
    }

    PresetsView::PresetsView(std::function<std::vector<ui::ListItem>()> items, std::function<void(int)> apply, std::function<bool(const std::wstring&)> save,
                             std::function<void(int)> remove)
        : View(420, 318), items_(std::move(items))
    {
        AddDialogChrome(*this, L"Наборы оформления");
        auto* hint = root.Add<ui::Label>();
        hint->rect = Rect{14, 30, 270, 13};
        hint->text = L"Наборы (двойной щелчок применяет):";
        list_ = root.Add<ui::ListBox>();
        list_->rect = Rect{14, 46, 272, 170};
        apply_ = root.Add<ui::Button>();
        apply_->rect = Rect{296, 46, 110, 23};
        apply_->text = L"Применить";
        remove_ = root.Add<ui::Button>();
        remove_->rect = Rect{296, 74, 110, 23};
        remove_->text = L"Удалить";
        auto* group = root.Add<ui::GroupBox>();
        group->rect = Rect{8, 224, root.width - 16, 50};
        group->text = L"Сохранить текущее оформление как набор";
        auto* label = root.Add<ui::Label>();
        label->rect = Rect{18, 246, 64, 13};
        label->text = L"Название:";
        name_ = root.Add<ui::TextField>();
        name_->rect = Rect{82, 242, 220, 21};
        auto* store = root.Add<ui::Button>();
        store->rect = Rect{310, 241, 96, 23};
        store->text = L"Сохранить";
        auto* close = root.Add<ui::Button>();
        close->rect = Rect{root.width - 14 - 75, root.height - 3 - 10 - 23, 75, 23};
        close->text = L"Закрыть";
        close->isDefault = true;
        View* self = this;
        PresetsView* me = this;
        apply_->onClick = [me, apply]() {
            if (me->list_->selected >= 0)
            {
                apply(me->list_->selected);
            }
        };
        list_->onActivate = [apply](int i) { apply(i); };
        list_->onSelect = [me](int) {
            me->apply_->enabled = true;
            me->remove_->enabled = true;
        };
        remove_->onClick = [me, remove]() {
            const int i = me->list_->selected;
            if (i >= 0)
            {
                remove(i);
                me->Reload(std::min(i, int(me->items_().size()) - 1));
            }
        };
        store->onClick = [me, save]() {
            const std::wstring name = Trim(me->name_->text);
            if (name.empty())
            {
                me->root.SetFocus(me->name_);
                return;
            }
            if (save(name))
            {
                const auto all = me->items_();
                int at = -1;
                for (int k = 0; k < int(all.size()); ++k)
                {
                    if (all[std::size_t(k)].text == name)
                    {
                        at = k;
                    }
                }
                me->Reload(at);
                me->name_->SetText(L"");
            }
        };
        close->onClick = [self]() { self->Close(0); };
        InstallDialogKeys(
            *this,
            [me, store]() {
                if (me->root.focus == me->name_ && !Trim(me->name_->text).empty())
                {
                    store->onClick();
                    return;
                }
                me->Close(0);
            },
            [self]() { self->Close(0); });
        Reload(-1);
    }

    void PresetsView::Reload(int select)
    {
        list_->items = items_();
        list_->selected = select;
        if (select >= 0)
        {
            list_->EnsureVisible(select);
        }
        apply_->enabled = select >= 0;
        remove_->enabled = select >= 0;
        list_->Invalidate();
    }

    std::vector<std::wstring> PlayerTextsView::Menu() const
    {
        std::vector<std::wstring> items;
        for (const auto& item : SplitList(menu_->text, L','))
        {
            if (!item.empty())
            {
                items.push_back(item);
            }
        }
        return items;
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
