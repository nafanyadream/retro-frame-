#include "library_view.h"

#include "icon_editor.h"

#include "icons.h"
#include "utf.h"

namespace rf
{
    namespace
    {
        template <class T>
        T* Place(ui::Root& root, int x, int y, int w, int h)
        {
            T* t = root.Add<T>();
            t->rect = Rect{x, y, w, h};
            return t;
        }

        ui::Label* AddLabel(ui::Root& root, int x, int y, int w, const std::wstring& text)
        {
            auto* l = Place<ui::Label>(root, x, y, w, 13);
            l->text = text;
            return l;
        }

        ui::Button* AddButton(ui::Root& root, int x, int y, int w, const std::wstring& text, std::function<void()> action)
        {
            auto* b = Place<ui::Button>(root, x, y, w, 23);
            b->text = text;
            b->onClick = std::move(action);
            return b;
        }

        ui::GlyphView* AddZoom(ui::Root& root, int x, int y)
        {
            auto* frame = Place<ui::Panel>(root, x, y, 56, 56);
            frame->kind = ui::Panel::Kind::Sunken;
            frame->useFill = true;
            frame->fillColor = Rgb(0xFF, 0xFF, 0xFF);
            return Place<ui::GlyphView>(root, x + 4, y + 4, 48, 48);
        }

        std::wstring SizeText(int w, int h)
        {
            if (w == 16 && h == 16)
            {
                return L"Размер 16 x 16, без изменений";
            }
            return L"Исходный размер " + std::to_wstring(w) + L" x " + std::to_wstring(h) + L", уменьшена до 16 x 16";
        }

        bool SamePath(const std::wstring& a, const std::wstring& b)
        {
            return ToLower(a) == ToLower(b);
        }
    }

    bool LoadWithProgress(Host& host, LibrarySource& source)
    {
        ProgressView progress(L"Библиотека иконок");
        const std::wstring head = L"Загружается: " + SourceName(source);
        progress.Update(head, L"", 0.0);
        void* handle = host.OpenModeless(progress);
        host.Pump();
        unsigned long long last = host.Ticks();
        const bool done = LoadSource(host, source, [&](const std::wstring& line, double fraction) {
            const unsigned long long now = host.Ticks();
            if (now - last >= 50)
            {
                last = now;
                progress.Update(head, line, fraction < 0.0 ? 0.0 : fraction);
                host.Pump();
            }
            return !progress.cancelled;
        });
        host.CloseModeless(handle);
        return done;
    }

    void ExplainSevenZip(Host& host, const std::wstring& archive)
    {
        ShowMessage(host, L"Библиотека иконок",
                    L"Чтобы открыть архив «" + FileNameOf(archive) + L"», нужна программа 7-Zip.\n\n"
                    L"Установите её с сайта 7-zip.org или положите файл 7za.exe\n"
                    L"рядом с RetroFrame.exe.\n\n"
                    L"Архивы ZIP программа открывает сама: можно перепаковать\n"
                    L"иконки в ZIP и добавить его.",
                    MessageKind::Warning);
    }

    LibraryView::LibraryView(Host& host, std::vector<LibrarySource>& sources, std::wstring& lastFolder, int startSource)
        : View(620, 440), host_(host), sources_(sources), lastFolder_(lastFolder)
    {
        AddDialogChrome(*this, L"Библиотека иконок");
        builtin_ = BuiltinLibrary();
        ui::Root& r = root;

        AddLabel(r, 12, 31, 180, L"Источники:");
        list_ = Place<ui::ListBox>(r, 10, 47, 190, 260);
        list_->onSelect = [this](int i) { ShowSource(i); };
        AddButton(r, 10, 313, 190, L"Добавить папку...", [this]() { AddFolder(); });
        AddButton(r, 10, 340, 190, L"Добавить архив...", [this]() { AddArchive(); });
        remove_ = AddButton(r, 10, 367, 190, L"Убрать из библиотеки", [this]() { RemoveSource(); });
        AddButton(r, 10, 394, 190, L"Нарисовать свой значок...", [this]() { DrawOwn(); });

        AddLabel(r, 212, 31, 40, L"Найти:");
        search_ = Place<ui::TextField>(r, 252, 27, 358, 21);
        search_->onChange = [this](const std::wstring&) { ApplyFilter(); };
        grid_ = Place<ui::IconGrid>(r, 210, 52, 400, 278);
        grid_->onSelect = [this](int) { UpdateInfo(); };
        grid_->onActivate = [this](int) { Accept(); };

        zoom_ = AddZoom(r, 210, 336);
        name_ = AddLabel(r, 276, 340, 334, L"");
        name_->font = ui::UiBold();
        where_ = AddLabel(r, 276, 358, 334, L"");
        size_ = AddLabel(r, 276, 376, 334, L"");

        count_ = AddLabel(r, 210, 409, 230, L"Двойной щелчок по иконке выбирает её.");
        count_->color = Rgb(0x40, 0x40, 0x40);
        ok_ = AddButton(r, root.width - 14 - 75 - 6 - 75, root.height - 3 - 10 - 23, 75, L"OK", [this]() { Accept(); });
        ok_->isDefault = true;
        AddButton(r, root.width - 14 - 75, root.height - 3 - 10 - 23, 75, L"Отмена", [this]() { Close(0); });
        InstallDialogKeys(*this, [this]() { Accept(); }, [this]() { Close(0); });

        const int count = 2 + int(sources_.size());
        ShowSource(std::clamp(startSource, 0, count - 1));
        list_->EnsureVisible(current_);
        root.SetFocus(grid_);
        root.charHook = [this](wchar_t ch) {
            if (ch < 32 || !search_->enabled)
            {
                return false;
            }
            root.SetFocus(search_);
            return search_->OnChar(ch);
        };
    }

    std::wstring LibraryView::Where(const LibraryIcon& item) const
    {
        for (std::size_t i = 0; i < shown_.size(); ++i)
        {
            if (shown_[i] == &item)
            {
                const int owner = shownOwners_[i];
                if (owner < 0)
                {
                    return L"Встроенный набор";
                }
                return SourceName(sources_[std::size_t(owner)]) + L"\\" + item.where;
            }
        }
        return item.where;
    }

    void LibraryView::FillSources()
    {
        std::size_t total = builtin_.size();
        for (const auto& s : sources_)
        {
            total += s.icons.size();
        }
        list_->items.clear();
        list_->items.push_back(ui::ListItem{BuiltinIcon(IconId::Monitor), L"Все иконки (" + std::to_wstring(total) + L")"});
        list_->items.push_back(ui::ListItem{BuiltinIcon(IconId::Picture), L"Встроенные (" + std::to_wstring(builtin_.size()) + L")"});
        for (const auto& s : sources_)
        {
            std::wstring text = SourceName(s);
            if (s.needsSevenZip)
            {
                text += L" (нужен 7-Zip)";
            }
            else if (s.loaded && !s.problem.empty() && s.icons.empty())
            {
                text += L" (не открывается)";
            }
            else if (s.loaded)
            {
                text += L" (" + std::to_wstring(s.icons.size()) + (s.partial ? L", не все)" : L")");
            }
            list_->items.push_back(ui::ListItem{BuiltinIcon(s.kind == SourceKind::Folder ? IconId::Folder : IconId::Archive), text});
        }
        list_->selected = current_;
        list_->Invalidate();
    }

    void LibraryView::Ensure(int index)
    {
        for (std::size_t i = 0; i < sources_.size(); ++i)
        {
            if ((index == 0 || index == int(i) + 2) && !sources_[i].loaded)
            {
                LoadWithProgress(host_, sources_[i]);
            }
        }
    }

    void LibraryView::ShowSource(int index)
    {
        current_ = std::clamp(index, 0, 1 + int(sources_.size()));
        Ensure(current_);
        all_.clear();
        owners_.clear();
        if (current_ <= 1)
        {
            for (const auto& item : builtin_)
            {
                all_.push_back(&item);
                owners_.push_back(-1);
            }
        }
        for (std::size_t i = 0; i < sources_.size(); ++i)
        {
            if (current_ == 0 || current_ == int(i) + 2)
            {
                for (const auto& item : sources_[i].icons)
                {
                    all_.push_back(&item);
                    owners_.push_back(int(i));
                }
            }
        }
        FillSources();
        remove_->enabled = current_ >= 2;
        grid_->selected = -1;
        grid_->ScrollTop();
        ApplyFilter();
        root.dirty = true;
    }

    void LibraryView::ApplyFilter()
    {
        const LibraryIcon* keep = grid_->selected >= 0 && grid_->selected < int(shown_.size()) ? shown_[std::size_t(grid_->selected)] : nullptr;
        const std::wstring query = ToLower(Trim(search_->text));
        shown_.clear();
        shownOwners_.clear();
        grid_->icons.clear();
        grid_->selected = -1;
        for (std::size_t i = 0; i < all_.size(); ++i)
        {
            const LibraryIcon* item = all_[i];
            if (!query.empty() && ToLower(item->name).find(query) == std::wstring::npos && ToLower(item->where).find(query) == std::wstring::npos)
            {
                continue;
            }
            if (item == keep)
            {
                grid_->selected = int(shown_.size());
            }
            shown_.push_back(item);
            shownOwners_.push_back(owners_[i]);
            grid_->icons.push_back(&item->icon);
        }
        if (grid_->selected >= 0)
        {
            grid_->EnsureVisible(grid_->selected);
        }
        else
        {
            grid_->ScrollTop();
        }
        if (!all_.empty())
        {
            grid_->emptyText = L"Ничего не найдено.";
        }
        else if (current_ == 0)
        {
            grid_->emptyText = L"Добавьте папку или архив с иконками\nкнопками слева.";
        }
        else
        {
            const LibrarySource& s = sources_[std::size_t(current_ - 2)];
            if (s.needsSevenZip)
            {
                grid_->emptyText = L"Для этого архива нужна программа 7-Zip.\nУстановите её с сайта 7-zip.org.";
            }
            else if (!s.problem.empty())
            {
                grid_->emptyText = L"Не удалось открыть: " + s.problem + L".";
            }
            else
            {
                grid_->emptyText = L"Здесь нет файлов .ico и .png.";
            }
        }
        UpdateInfo();
    }

    void LibraryView::UpdateInfo()
    {
        const int sel = grid_->selected;
        const bool has = sel >= 0 && sel < int(shown_.size());
        ok_->enabled = has;
        if (has)
        {
            const LibraryIcon& item = *shown_[std::size_t(sel)];
            zoom_->image = ScaleNearest(item.icon, 3);
            name_->text = item.name;
            where_->text = Where(item);
            size_->text = shownOwners_[std::size_t(sel)] < 0 ? L"Размер 16 x 16" : SizeText(item.width, item.height);
        }
        else
        {
            zoom_->image = Image();
            name_->text.clear();
            where_->text.clear();
            size_->text.clear();
            if (current_ >= 2)
            {
                const LibrarySource& s = sources_[std::size_t(current_ - 2)];
                name_->text = SourceName(s);
                where_->text = s.path;
                std::wstring stats = L"Иконок: " + std::to_wstring(s.icons.size());
                if (s.failed > 0)
                {
                    stats += L", не прочитано файлов: " + std::to_wstring(s.failed);
                }
                if (s.partial)
                {
                    stats += L", загрузка была прервана";
                }
                size_->text = stats;
            }
            else
            {
                where_->text = L"Иконок: " + std::to_wstring(shown_.size());
                size_->text = sources_.empty() ? L"Свои иконки: подключите папку или архив кнопками слева." : L"Выберите иконку, чтобы увидеть её крупно.";
            }
        }
        root.dirty = true;
    }

    void LibraryView::AddFolder()
    {
        const std::wstring dir = host_.AskFolder(lastFolder_, L"Папка с иконками");
        if (dir.empty())
        {
            return;
        }
        lastFolder_ = dir;
        LibrarySource s;
        s.kind = SourceKind::Folder;
        s.path = dir;
        AddSource(std::move(s));
    }

    void LibraryView::AddArchive()
    {
        const std::wstring path = host_.AskOpenArchive(lastFolder_);
        if (path.empty())
        {
            return;
        }
        lastFolder_ = DirectoryOf(path);
        if (IsSevenZipArchive(path) && host_.SevenZipPath().empty())
        {
            ExplainSevenZip(host_, path);
            return;
        }
        LibrarySource s;
        s.kind = SourceKind::Archive;
        s.path = path;
        AddSource(std::move(s));
    }

    void LibraryView::AddSource(LibrarySource source)
    {
        for (std::size_t i = 0; i < sources_.size(); ++i)
        {
            if (sources_[i].kind == source.kind && SamePath(sources_[i].path, source.path))
            {
                ShowSource(int(i) + 2);
                list_->EnsureVisible(current_);
                return;
            }
        }
        const bool done = LoadWithProgress(host_, source);
        if (!done)
        {
            return;
        }
        const std::wstring name = SourceName(source);
        if (source.needsSevenZip)
        {
            ExplainSevenZip(host_, source.path);
            return;
        }
        if (source.icons.empty())
        {
            const std::wstring why = source.problem.empty() ? L"В «" + name + L"» не нашлось файлов .ico и .png." : L"Не удалось открыть «" + name + L"»: " + source.problem + L".";
            ShowMessage(host_, L"Библиотека иконок", why, MessageKind::Warning);
            return;
        }
        sources_.push_back(std::move(source));
        if (onSourcesChanged)
        {
            onSourcesChanged();
        }
        ShowSource(int(sources_.size()) + 1);
        list_->EnsureVisible(current_);
    }

    void LibraryView::RemoveSource()
    {
        if (current_ < 2 || current_ - 2 >= int(sources_.size()))
        {
            return;
        }
        sources_.erase(sources_.begin() + (current_ - 2));
        if (onSourcesChanged)
        {
            onSourcesChanged();
        }
        ShowSource(std::min(current_, 1 + int(sources_.size())));
        list_->EnsureVisible(current_);
    }

    void LibraryView::DrawOwn()
    {
        const int sel = grid_->selected;
        const Image start = sel >= 0 && sel < int(shown_.size()) ? shown_[std::size_t(sel)]->icon : Image();
        IconEditorView editor(host_, start);
        if (host_.RunModal(editor) != 1)
        {
            return;
        }
        drawn_ = editor.Result();
        Close(2);
    }

    void LibraryView::Accept()
    {
        if (grid_->selected >= 0 && grid_->selected < int(shown_.size()))
        {
            picked_ = grid_->selected;
            Close(1);
        }
    }

    IconPickerView::IconPickerView(const std::wstring& fileName, const std::vector<Image>& icons)
        : View(440, 362)
    {
        AddDialogChrome(*this, L"Выбор иконки");
        ui::Root& r = root;
        for (const auto& img : icons)
        {
            small_.push_back(MakeIcon16(img));
            widths_.push_back(img.width);
            heights_.push_back(img.height);
        }
        AddLabel(r, 12, 31, root.width - 24, L"В файле «" + fileName + L"» иконок: " + std::to_wstring(icons.size()) + L". Выберите одну:");
        grid_ = Place<ui::IconGrid>(r, 10, 48, root.width - 20, 230);
        for (const auto& img : small_)
        {
            grid_->icons.push_back(&img);
        }
        grid_->selected = small_.empty() ? -1 : 0;
        grid_->onSelect = [this](int) { UpdateInfo(); };
        IconPickerView* self = this;
        grid_->onActivate = [self](int) { self->Close(1); };
        zoom_ = AddZoom(r, 10, 286);
        info_ = AddLabel(r, 76, 292, root.width - 90, L"");
        info_->rect.h = 30;
        ok_ = AddButton(r, root.width - 14 - 75 - 6 - 75, root.height - 3 - 10 - 23, 75, L"OK", [self]() {
            if (self->Picked() >= 0)
            {
                self->Close(1);
            }
        });
        ok_->isDefault = true;
        AddButton(r, root.width - 14 - 75, root.height - 3 - 10 - 23, 75, L"Отмена", [self]() { self->Close(0); });
        InstallDialogKeys(*this, [self]() {
            if (self->Picked() >= 0)
            {
                self->Close(1);
            }
        }, [self]() { self->Close(0); });
        root.SetFocus(grid_);
        UpdateInfo();
    }

    void IconPickerView::UpdateInfo()
    {
        const int sel = grid_->selected;
        if (sel < 0 || sel >= int(small_.size()))
        {
            zoom_->image = Image();
            info_->text.clear();
            ok_->enabled = false;
            return;
        }
        ok_->enabled = true;
        zoom_->image = ScaleNearest(small_[std::size_t(sel)], 3);
        info_->text = L"Иконка № " + std::to_wstring(sel + 1) + L"\n" + SizeText(widths_[std::size_t(sel)], heights_[std::size_t(sel)]);
        root.dirty = true;
    }
}
