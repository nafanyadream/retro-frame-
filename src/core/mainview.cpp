#include "mainview.h"

#include "desktop.h"
#include "glyphs.h"
#include "icons.h"
#include "icon_editor.h"
#include "library_view.h"
#include "utf.h"

#include <cmath>
#include <cstring>

namespace rf
{
    namespace
    {
        constexpr int WindowW = 900;
        constexpr int WindowH = 640;
        constexpr int PreviewMaxW = 598;
        constexpr int PreviewMaxH = 346;
        constexpr int PreviewLimit = 2400;
        const wchar_t* const AppVersion = L"0.6.0";

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

        ui::CheckBox* AddCheck(ui::Root& root, int x, int y, int w, const std::wstring& text)
        {
            auto* c = Place<ui::CheckBox>(root, x, y, w, 16);
            c->text = text;
            return c;
        }

        std::wstring ScaleText(int k)
        {
            return std::to_wstring(k) + L"x";
        }
    }

    MainView::MainView(Host& host)
        : View(WindowW, WindowH), host_(host)
    {
        s_ = DefaultSettings();
        title = L"Ретро-рамка";
        icon = BuiltinIcon(IconId::Picture);
        Build();
        LoadSettings();
        BuildMenu();
        UpdateFormatItems();
        SyncControls();
        UpdatePreview();
        UpdateStatus();
    }

    void MainView::Build()
    {
        ui::Root& r = root;
        auto* border = Place<ui::WindowBorder>(r, 0, 0, WindowW, WindowH);
        (void)border;
        auto* bar = Place<ui::TitleBar>(r, 4, 4, WindowW - 8, 18);
        bar->title = L"Ретро-рамка";
        bar->icon = BuiltinIcon(IconId::Picture);
        bar->onClose = [this]() { Exit(); };
        bar->onMinimize = [this]() { host_.Minimize(); };

        menu_ = Place<ui::MenuBar>(r, 4, 23, WindowW - 8, 18);
        BuildMenu();
        Place<ui::Panel>(r, 4, 42, WindowW - 8, 2)->kind = ui::Panel::Kind::EtchedH;

        int bx = 10;
        const auto toolButton = [&](const std::wstring& text, int w, std::function<void()> action, bool isDefault) {
            auto* b = Place<ui::Button>(r, bx, 48, w, 23);
            b->text = text;
            b->onClick = std::move(action);
            b->isDefault = isDefault;
            bx += w + 4;
            return b;
        };
        toolButton(L"Открыть...", 86, [this]() { Open(); }, false);
        toolButton(L"Вставить", 78, [this]() { Paste(); }, false);
        Place<ui::Panel>(r, bx + 2, 50, 2, 19)->kind = ui::Panel::Kind::EtchedV;
        bx += 10;
        toolButton(L"Сохранить...", 96, [this]() { SaveCurrent(); }, true);
        effectsButton_ = toolButton(L"Эффекты...", 86, [this]() { EditEffects(); }, false);
        toolButton(L"Наборы...", 80, [this]() { EditPresets(); }, false);
        saveAllButton_ = toolButton(L"Сохранить все...", 112, [this]() { SaveAll(); }, false);
        const int queueX = bx;
        copyButton_ = toolButton(L"Копировать", 88, [this]() { CopyCurrent(); }, false);
        photoOnly_.push_back(copyButton_);
        const auto arrow = [](const Glyph& g) {
            Image img(int(std::strlen(g.front())), int(g.size()), 0);
            Painter(img).DrawGlyph(0, 0, g, Rgb(0, 0, 0));
            return img;
        };
        prevButton_ = Place<ui::Button>(r, queueX, 48, 22, 23);
        prevButton_->icon = arrow(glyphs::Left);
        prevButton_->onClick = [this]() { StepQueue(-1); };
        queueLabel_ = AddLabel(r, queueX + 24, 53, 44, L"");
        queueLabel_->align = ui::Align::Center;
        nextButton_ = Place<ui::Button>(r, queueX + 70, 48, 22, 23);
        nextButton_->icon = arrow(glyphs::Right);
        nextButton_->onClick = [this]() { StepQueue(1); };

        AddLabel(r, WindowW - 4 - 6 - 150 - 52, 53, 50, L"Формат:");
        format_ = Place<ui::ComboBox>(r, WindowW - 4 - 6 - 150, 49, 150, 21);
        format_->items = {L"PNG", L"JPG, высокое качество", L"JPG, среднее качество"};
        format_->onChange = [this](int i) {
            if (mode_ == VideoMode)
            {
                s_.video.format = i;
            }
            else if (SceneMode())
            {
                SceneOut().format = i;
            }
            else if (AudioMode())
            {
                Audio().format = i;
            }
            else
            {
                s_.format = i;
            }
            Changed();
        };
        Place<ui::Panel>(r, 4, 76, WindowW - 8, 2)->kind = ui::Panel::Kind::EtchedH;

        auto* page = Place<ui::Panel>(r, 8, 102, 618, 500);
        page->kind = ui::Panel::Kind::Raised;
        tabs_ = Place<ui::TabStrip>(r, 8, 82, 618, 21);
        tabs_->tabs = {L"Фото", L"Видео", L"Музыка", L"Звукозапись", L"Блокнот", L"Установка"};
        tabs_->onChange = [this](int i) { SetMode(i); };
        preview_ = Place<ui::PreviewPane>(r, 16, 112, 602, 350);
        filesLabel_ = AddLabel(r, 18, 474, 200, L"Файлы:");
        auto* add = Place<ui::Button>(r, 618 - 84 - 4 - 84 - 4 - 84, 468, 84, 21);
        add->text = L"Добавить...";
        add->onClick = [this]() { Open(); };
        removeButton_ = Place<ui::Button>(r, 618 - 84 - 4 - 84, 468, 84, 21);
        removeButton_->text = L"Убрать";
        removeButton_->onClick = [this]() { RemoveCurrent(); };
        auto* clear = Place<ui::Button>(r, 618 - 84, 468, 84, 21);
        clear->text = L"Убрать все";
        clear->onClick = [this]() { RemoveAll(); };
        thumbs_ = Place<ui::ThumbList>(r, 16, 494, 602, 100);
        thumbs_->onSelect = [this](int i) { Select(i); };
        photoOnly_.push_back(filesLabel_);
        photoOnly_.push_back(add);
        photoOnly_.push_back(removeButton_);
        photoOnly_.push_back(clear);
        photoOnly_.push_back(thumbs_);
        BuildVideo();
        BuildMusic();
        BuildRecorder();
        BuildScenes();

        const int gx = 636;
        const int gw = WindowW - 4 - 4 - gx;

        int y = 80;
        Place<ui::GroupBox>(r, gx, y, gw, 58)->text = L"Стиль";
        const wchar_t* styleNames[] = {L"Windows 95", L"Windows 98", L"Windows 2000", L"Windows XP"};
        const int radioPos[4][2] = {{gx + 10, y + 16}, {gx + 132, y + 16}, {gx + 10, y + 36}, {gx + 132, y + 36}};
        for (int i = 0; i < 4; ++i)
        {
            auto* rb = Place<ui::RadioButton>(r, radioPos[i][0], radioPos[i][1], 110, 16);
            rb->text = styleNames[i];
            rb->value = i;
            rb->group = &styleValue_;
            rb->onChange = [this](int v) { SetStyle(static_cast<Style>(v)); };
        }

        y = 144;
        Place<ui::GroupBox>(r, gx, y, gw, 124)->text = L"Окно";
        AddLabel(r, gx + 10, y + 20, 60, L"Заголовок:");
        titleField_ = Place<ui::TextField>(r, gx + 72, y + 16, gw - 82, 21);
        titleField_->onChange = [this](const std::wstring& t) {
            Look().customTitle = t;
            Changed();
        };
        fromName_ = AddCheck(r, gx + 72, y + 40, gw - 82, L"Имя файла");
        fromName_->onChange = [this](bool v) {
            Look().titleFromName = v;
            SyncControls();
            Changed();
        };
        AddLabel(r, gx + 10, y + 64, 60, L"Тип:");
        kind_ = Place<ui::ComboBox>(r, gx + 72, y + 60, gw - 82, 21);
        kind_->items = {L"Обычное окно", L"Диалог (только ×)", L"Свойства (? ×)", L"Без кнопок"};
        kind_->items[1] = L"Диалог: только закрыть";
        kind_->items[2] = L"Свойства: справка и закрыть";
        kind_->onChange = [this](int i) {
            Look().kind = static_cast<WindowKind>(i);
            Changed();
        };
        active_ = AddCheck(r, gx + 10, y + 86, gw - 20, L"Активное окно");
        active_->onChange = [this](bool v) {
            s_.frame.active = v;
            Changed();
        };
        sunken_ = AddCheck(r, gx + 10, y + 104, gw - 20, L"Фото во вдавленной области");
        sunken_->onChange = [this](bool v) {
            s_.frame.sunken = v;
            Changed();
        };

        y = 274;
        Place<ui::GroupBox>(r, gx, y, gw, 78)->text = L"Иконка";
        for (int i = 0; i < int(iconCells_.size()); ++i)
        {
            const int row = i / 9;
            const int col = i % 9;
            auto* cell = Place<ui::IconCell>(r, gx + 8 + col * 26, y + 16 + row * 26, 24, 24);
            if (i < FrameIconCount())
            {
                cell->icon = BuiltinIcon(FrameIconAt(i));
                cell->onClick = [this, i]() {
                    Look().iconIndex = i;
                    SyncControls();
                    Changed();
                };
            }
            else
            {
                cell->emptyText = L"нет";
                cell->onClick = [this]() {
                    Look().iconIndex = NoIconIndex;
                    SyncControls();
                    Changed();
                };
            }
            iconCells_[std::size_t(i)] = cell;
        }
        const int cx = gx + 8 + 3 * 26 + 10;
        customCell_ = Place<ui::IconCell>(r, cx, y + 42, 24, 24);
        customCell_->emptyText = L"?";
        customCell_->onClick = [this]() {
            if (s_.customIcon.Empty())
            {
                LoadCustomIcon();
                return;
            }
            Look().iconIndex = CustomIconIndex;
            SyncControls();
            Changed();
        };
        Place<ui::Panel>(r, cx - 6, y + 44, 2, 20)->kind = ui::Panel::Kind::EtchedV;
        auto* loadIcon = Place<ui::Button>(r, cx + 28, y + 43, 50, 22);
        loadIcon->text = L"Своя...";
        loadIcon->onClick = [this]() { LoadCustomIcon(); };
        auto* library = Place<ui::Button>(r, cx + 28 + 54, y + 43, gx + gw - 8 - (cx + 28 + 54), 22);
        library->text = L"Библиотека...";
        library->onClick = [this]() { OpenLibrary(); };

        y = 358;
        Place<ui::GroupBox>(r, gx, y, gw, 96)->text = L"Цвета";
        AddLabel(r, gx + 10, y + 20, 60, L"Схема:");
        scheme_ = Place<ui::ComboBox>(r, gx + 72, y + 16, gw - 82, 21);
        for (int i = 0; i < SchemeCount(); ++i)
        {
            scheme_->items.push_back(SchemeName(i));
        }
        scheme_->items.push_back(L"Своя");
        scheme_->onChange = [this](int i) {
            if (i < SchemeCount())
            {
                SetScheme(i);
            }
        };
        AddLabel(r, gx + 10, y + 46, 70, L"Заголовок:");
        active1_ = Place<ui::Swatch>(r, gx + 82, y + 42, 24, 19);
        active1_->onClick = [this]() { PickColor(&FrameColors::active1); };
        active2_ = Place<ui::Swatch>(r, gx + 108, y + 42, 24, 19);
        active2_->onClick = [this]() { PickColor(&FrameColors::active2); };
        AddLabel(r, gx + 146, y + 46, 60, L"Текст:");
        titleText_ = Place<ui::Swatch>(r, gx + gw - 10 - 24, y + 42, 24, 19);
        titleText_->onClick = [this]() { PickColor(&FrameColors::titleText); };
        AddLabel(r, gx + 10, y + 70, 70, L"Неактивный:");
        inactive1_ = Place<ui::Swatch>(r, gx + 82, y + 66, 24, 19);
        inactive1_->onClick = [this]() { PickColor(&FrameColors::inactive1); };
        inactive2_ = Place<ui::Swatch>(r, gx + 108, y + 66, 24, 19);
        inactive2_->onClick = [this]() { PickColor(&FrameColors::inactive2); };
        AddLabel(r, gx + 146, y + 70, 60, L"Рамка:");
        face_ = Place<ui::Swatch>(r, gx + gw - 10 - 24, y + 66, 24, 19);
        face_->onClick = [this]() { PickColor(&FrameColors::face); };

        y = 460;
        Place<ui::GroupBox>(r, gx, y, gw, 104)->text = L"Дополнительно";
        const int c1 = gx + 10;
        const int c2 = gx + 128;
        const int cw = gw - 128 - 4;
        menuCheck_ = AddCheck(r, c1, y + 16, 116, L"Строка меню");
        menuCheck_->onChange = [this](bool v) {
            Look().menu = v;
            SyncControls();
            Changed();
        };
        statusCheck_ = AddCheck(r, c2, y + 16, cw, L"Строка состояния");
        statusCheck_->onChange = [this](bool v) {
            Look().status = v;
            SyncControls();
            Changed();
        };
        addressCheck_ = AddCheck(r, c1, y + 34, 116, L"Адресная строка");
        addressCheck_->onChange = [this](bool v) {
            Look().address = v;
            Changed();
        };
        toolbarCheck_ = AddCheck(r, c2, y + 34, cw, L"Панель кнопок");
        toolbarCheck_->onChange = [this](bool v) {
            Look().toolbar = v;
            Changed();
        };
        cornerCheck_ = AddCheck(r, c1, y + 52, 116, L"Картинка в углу");
        cornerCheck_->onChange = [this](bool v) {
            Look().corner = v;
            if (v && s_.cornerImage.Empty())
            {
                LoadCorner();
            }
            SyncControls();
            Changed();
        };
        gripCheck_ = AddCheck(r, c2, y + 52, cw, L"Уголок размера");
        gripCheck_->onChange = [this](bool v) {
            s_.frame.grip = v;
            Changed();
        };
        auto* texts = Place<ui::Button>(r, c1, y + 74, 112, 22);
        texts->text = L"Тексты окна...";
        texts->onClick = [this]() { EditTexts(); };
        auto* corner = Place<ui::Button>(r, c2, y + 74, cw - 6, 22);
        corner->text = L"Картинка...";
        corner->onClick = [this]() { LoadCorner(); };

        y = 570;
        Place<ui::GroupBox>(r, gx, y, gw, 44)->text = L"Масштаб рамки в файле";
        scale_ = Place<ui::ComboBox>(r, gx + 10, y + 16, gw - 20, 21);
        scale_->items = {L"Авто", L"1x", L"2x", L"3x", L"4x"};
        scale_->onChange = [this](int i) {
            s_.frame.scale = i;
            Changed();
        };

        status_ = Place<ui::StatusBar>(r, 4, WindowH - 4 - 20, WindowW - 8, 20);
        status_->fields = {ui::StatusField{L"", 0}, ui::StatusField{L"", 150}, ui::StatusField{L"", 110}};

        root.keyHook = [this](const ui::KeyEvent& e) { return HandleKey(e); };
    }

    void MainView::BuildMenu()
    {
        using ui::Menu;
        using ui::MenuItem;
        MenuItem sep;
        sep.separator = true;
        Menu file{L"Файл", {}};
        file.items.push_back(MenuItem{L"Открыть...", L"Ctrl+O", [this]() { Open(); }, true, false});
        file.items.push_back(MenuItem{L"Вставить из буфера", L"Ctrl+V", [this]() { Paste(); }, true, false});
        file.items.push_back(sep);
        file.items.push_back(MenuItem{L"Сохранить...", L"Ctrl+S", [this]() { SaveCurrent(); }, true, false});
        file.items.push_back(MenuItem{L"Сохранить все в папку...", L"", [this]() { SaveAll(); }, true, false});
        file.items.push_back(sep);
        file.items.push_back(MenuItem{L"Сбросить настройки", L"", [this]() { ResetSettings(); }, true, false});
        file.items.push_back(sep);
        file.items.push_back(MenuItem{L"Выход", L"Alt+F4", [this]() { Exit(); }, true, false});
        Menu edit{L"Правка", {}};
        edit.items.push_back(MenuItem{L"Копировать результат", L"Ctrl+C", [this]() { CopyCurrent(); }, true, false});
        edit.items.push_back(MenuItem{L"Вставить из буфера", L"Ctrl+V", [this]() { Paste(); }, true, false});
        edit.items.push_back(sep);
        edit.items.push_back(MenuItem{L"Убрать из списка", L"Del", [this]() { RemoveCurrent(); }, true, false});
        edit.items.push_back(MenuItem{L"Убрать все", L"", [this]() { RemoveAll(); }, true, false});
        Menu view{L"Вид", {}};
        view.items.push_back(MenuItem{L"Стиль Windows 95", L"", [this]() { SetStyle(Style::Win95); }, true, false});
        view.items.push_back(MenuItem{L"Стиль Windows 98", L"", [this]() { SetStyle(Style::Win98); }, true, false});
        view.items.push_back(MenuItem{L"Стиль Windows 2000", L"", [this]() { SetStyle(Style::Win2000); }, true, false});
        view.items.push_back(MenuItem{L"Стиль Windows XP", L"", [this]() { SetStyle(Style::WinXP); }, true, false});
        view.items.push_back(sep);
        view.items.push_back(MenuItem{L"Предыдущий файл", L"Влево", [this]() { StepQueue(-1); }, true, false});
        view.items.push_back(MenuItem{L"Следующий файл", L"Вправо", [this]() { StepQueue(1); }, true, false});
        view.items.push_back(sep);
        view.items.push_back(MenuItem{L"Эффекты...", L"", [this]() { EditEffects(); }, true, false});
        view.items.push_back(MenuItem{L"Нарисовать значок...", L"", [this]() { DrawIcon(); }, true, false});
        Menu sets{L"Наборы", {}};
        for (int i = 0; i < int(s_.presets.size()); ++i)
        {
            sets.items.push_back(MenuItem{s_.presets[std::size_t(i)].name, L"", [this, i]() { ApplyPreset(i); }, true, false});
        }
        if (!s_.presets.empty())
        {
            sets.items.push_back(sep);
        }
        sets.items.push_back(MenuItem{L"Сохранить или удалить...", L"", [this]() { EditPresets(); }, true, false});
        Menu help{L"Справка", {}};
        help.items.push_back(MenuItem{L"О программе...", L"", [this]() { About(); }, true, false});
        menu_->menus = {file, edit, view, sets, help};
    }

    int MainView::ShownFormat() const
    {
        return format_->index;
    }

    void MainView::LoadSettings()
    {
        const std::wstring dir = host_.SettingsFolder();
        if (dir.empty())
        {
            return;
        }
        std::string text;
        if (host_.ReadText(JoinPath(dir, L"settings.ini"), text))
        {
            ParseSettings(text, s_);
        }
        Image img;
        std::wstring err;
        const std::wstring iconPath = JoinPath(dir, L"icon.png");
        if (host_.FileExists(iconPath) && host_.ReadImage(iconPath, img, err))
        {
            s_.customIcon = MakeIcon16(img);
        }
        const std::wstring cornerPath = JoinPath(dir, L"corner.png");
        if (host_.FileExists(cornerPath) && host_.ReadImage(cornerPath, img, err))
        {
            s_.cornerImage = MakeIcon16(img);
        }
        for (int m = 0; m < ModeCount; ++m)
        {
            ModeLook& look = s_.looks[std::size_t(m)];
            if (look.iconIndex == CustomIconIndex && s_.customIcon.Empty())
            {
                look.iconIndex = DefaultLook(m).iconIndex;
            }
        }
        const std::wstring backgroundPath = JoinPath(dir, L"background.png");
        if (host_.FileExists(backgroundPath) && host_.ReadImage(backgroundPath, img, err))
        {
            s_.backgroundImage = std::max(img.width, img.height) > 1920 ? FitInside(img, 1920, 1920, false) : img;
        }
        library_.clear();
        for (const auto& entry : s_.library)
        {
            LibrarySource source;
            if (DecodeSource(entry, source))
            {
                library_.push_back(std::move(source));
            }
        }
        mode_ = s_.mode;
        tabs_->index = mode_;
        preview_->rect.w = AudioMode() || SceneMode() ? 300 : 602;
        preview_->rect.h = mode_ == PhotoMode ? 350 : 296;
    }

    void MainView::SaveSettings()
    {
        dirty_ = false;
        const std::wstring dir = host_.SettingsFolder();
        if (dir.empty())
        {
            return;
        }
        host_.WriteText(JoinPath(dir, L"settings.ini"), SerializeSettings(s_));
    }

    void MainView::OnTick(unsigned long long ms)
    {
        if (dirty_ && ms - dirtyAt_ > 1500)
        {
            SaveSettings();
        }
        if (!flash_.empty() && ms > flashUntil_)
        {
            flash_.clear();
            UpdateStatus();
        }
        PollRecording();
        PollVideo(ms);
    }

    bool MainView::OnCloseRequest()
    {
        StopPlay();
        StopRecording(false);
        fetch_.reset();
        if (!voiceTemp_.empty())
        {
            host_.RemoveFolder(voiceTemp_);
            voiceTemp_.clear();
        }
        SaveSettings();
        return true;
    }

    void MainView::OnDropFiles(const std::vector<std::wstring>& paths)
    {
        std::vector<std::wstring> images;
        std::vector<std::wstring> videos;
        std::vector<std::wstring> songs;
        std::vector<std::wstring> texts;
        std::wstring lyrics;
        bool withSong = mode_ == MusicMode && track_ != nullptr;
        for (const auto& path : paths)
        {
            withSong = withSong || IsAudioFile(path);
        }
        for (const auto& path : paths)
        {
            const std::wstring lower = ToLower(path);
            const bool txt = lower.size() >= 4 && lower.compare(lower.size() - 4, 4, L".txt") == 0;
            if (txt && !withSong)
            {
                texts.push_back(path);
                continue;
            }
            if (IsLyricsFile(path))
            {
                lyrics = path;
                continue;
            }
            const bool gif = lower.size() >= 4 && lower.compare(lower.size() - 4, 4, L".gif") == 0;
            if (IsVideoFile(path) || (gif && mode_ == VideoMode))
            {
                videos.push_back(path);
            }
            else if (IsAudioFile(path))
            {
                songs.push_back(path);
            }
            else
            {
                images.push_back(path);
            }
        }
        if (!videos.empty())
        {
            OpenMedia(VideoMode, videos);
            return;
        }
        if (!texts.empty() && images.empty() && songs.empty())
        {
            SetMode(NotepadMode);
            LoadSceneText(texts.front());
            return;
        }
        if (!songs.empty())
        {
            OpenMedia(mode_ == RecorderMode ? RecorderMode : MusicMode, songs);
            if (mode_ == MusicMode && !lyrics.empty() && track_ && songs.size() == 1)
            {
                LoadLyrics(lyrics, false);
            }
            return;
        }
        if (!lyrics.empty() && images.empty())
        {
            if (!track_)
            {
                SetMode(MusicMode);
                Warn(L"Сначала откройте песню, потом добавьте к ней текст.");
                return;
            }
            SetMode(MusicMode);
            LoadLyrics(lyrics, false);
            return;
        }
        SetMode(PhotoMode);
        AddFiles(images);
    }

    void MainView::Refresh()
    {
        SyncControls();
        UpdatePreview();
        UpdateStatus();
    }

    void MainView::SyncControls()
    {
        styleValue_ = int(s_.frame.style);
        const ModeLook& look = Look();
        titleField_->enabled = !look.titleFromName;
        if (titleField_->text != look.customTitle)
        {
            titleField_->SetText(look.customTitle);
        }
        fromName_->checked = look.titleFromName;
        kind_->index = int(look.kind);
        active_->checked = s_.frame.active;
        sunken_->checked = s_.frame.sunken;
        for (int i = 0; i < int(iconCells_.size()); ++i)
        {
            const int idx = i < FrameIconCount() ? i : NoIconIndex;
            iconCells_[std::size_t(i)]->selected = look.iconIndex == idx;
        }
        customCell_->icon = s_.customIcon;
        customCell_->selected = look.iconIndex == CustomIconIndex;
        scheme_->index = s_.scheme >= 0 && s_.scheme < SchemeCount() ? s_.scheme : SchemeCount();
        const FrameColors& c = s_.frame.colors;
        active1_->color = c.active1;
        active2_->color = c.active2;
        titleText_->color = c.titleText;
        inactive1_->color = c.inactive1;
        inactive2_->color = c.inactive2;
        face_->color = c.face;
        active2_->enabled = HasGradient(s_.frame.style);
        inactive2_->enabled = HasGradient(s_.frame.style);
        menuCheck_->checked = look.menu;
        statusCheck_->checked = look.status;
        addressCheck_->checked = look.address;
        toolbarCheck_->checked = look.toolbar;
        cornerCheck_->checked = look.corner;
        cornerCheck_->enabled = look.menu;
        gripCheck_->checked = s_.frame.grip;
        gripCheck_->enabled = look.status;
        scale_->index = std::clamp(s_.frame.scale, 0, 4);
        UpdateFormatItems();
        for (ui::Widget* w : photoOnly_)
        {
            w->visible = mode_ == PhotoMode;
        }
        const bool queue = mode_ != PhotoMode && QueueSize() > 1;
        saveAllButton_->visible = mode_ == PhotoMode || queue;
        prevButton_->visible = queue;
        nextButton_->visible = queue;
        queueLabel_->visible = queue;
        prevButton_->enabled = QueuePosition() > 0;
        nextButton_->enabled = QueuePosition() + 1 < QueueSize();
        queueLabel_->text = std::to_wstring(QueuePosition() + 1) + L" / " + std::to_wstring(QueueSize());
        removeButton_->enabled = current_ >= 0;
        sunken_->text = mode_ == VideoMode ? L"Видео во вдавленной области" : L"Фото во вдавленной области";
        sunken_->enabled = !AudioMode() && !SceneMode();
        SyncVideoControls();
        root.dirty = true;
    }

    void MainView::Changed()
    {
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
        SyncControls();
        UpdatePreview();
        UpdateStatus();
    }

    FrameSettings MainView::LookFrame(int mode, const std::wstring& name) const
    {
        const ModeLook& look = s_.looks[std::size_t(mode)];
        FrameSettings f = s_.frame;
        f.title = look.titleFromName ? name : look.customTitle;
        f.kind = look.kind;
        f.menu = look.menu;
        f.status = look.status;
        f.toolbar = look.toolbar;
        f.address = look.address;
        f.corner = look.corner;
        if (look.iconIndex == CustomIconIndex)
        {
            f.icon = s_.customIcon;
        }
        else if (look.iconIndex >= 0 && look.iconIndex < FrameIconCount())
        {
            f.icon = BuiltinIcon(FrameIconAt(look.iconIndex));
        }
        else
        {
            f.icon = Image();
        }
        f.cornerImage = s_.cornerImage;
        if (s_.effects.popups > 0)
        {
            f.active = false;
        }
        return f;
    }

    void MainView::ApplyEffects(Image& img, const FrameSettings& frame, const Stage& stage, double t, int seed, double progress, Spot shift) const
    {
        if (!HasOverlays(s_.effects))
        {
            return;
        }
        EffectsLayer layer(s_.effects, frame, stage);
        layer.Shift(shift.x, shift.y);
        layer.Apply(img, t, seed, false, progress);
    }

    Spot MainView::PreviewMotion(const Rect& window, int outW, int outH, int reserve, double t, int seed) const
    {
        if (s_.effects.motion == 0)
        {
            return Spot{};
        }
        return WindowMotion(s_.effects, t, seed, 1, window, Rect{0, 0, outW, outH - reserve});
    }

    Image MainView::Backdrop(int w, int h, int k, const FrameSettings& frame, const FrameVars& vars, int ending) const
    {
        Image bg = CanvasBackground(s_.backgroundPicture ? s_.backgroundImage : Image(), s_.background, w, h);
        if (!s_.effects.desktop && !s_.effects.desktopIcons)
        {
            return bg;
        }
        DesktopOptions d;
        d.taskbar = s_.effects.desktop;
        d.icons = s_.effects.desktopIcons;
        d.clock = s_.effects.clock;
        const TaskButton button = ending == 1 ? TaskButton::None : ending == 2 ? TaskButton::Inactive : TaskButton::Active;
        return DesktopBackground(bg, frame, d, w, h, k, ExpandVars(frame.title, vars), frame.icon, button);
    }

    const Image& MainView::PreviewBackdrop(int w, int h, const FrameSettings& frame, const FrameVars& vars)
    {
        const Image& picture = s_.backgroundImage;
        std::wstring key = std::to_wstring(w) + L"x" + std::to_wstring(h) + L"/" + std::to_wstring(s_.background) + L"/" +
                           (s_.backgroundPicture ? std::to_wstring(picture.width) + L"x" + std::to_wstring(picture.height) : std::wstring(L"-"));
        if (s_.effects.desktop || s_.effects.desktopIcons)
        {
            key += L"/" + std::to_wstring(int(s_.effects.desktop)) + std::to_wstring(int(s_.effects.desktopIcons)) + L"/" + std::to_wstring(int(frame.style)) + L"/" +
                   std::to_wstring(frame.colors.face) + L"/" + ClockText(s_.effects.clock) + L"/" + ExpandVars(frame.title, vars) + L"/" +
                   std::to_wstring(frame.icon.width);
            std::uint32_t hash = 2166136261u;
            for (const Color c : frame.icon.pixels)
            {
                hash = (hash ^ c) * 16777619u;
            }
            key += L"/" + std::to_wstring(hash);
        }
        if (key != bgKey_)
        {
            bgPreview_ = Backdrop(w, h, 1, frame, vars, 0);
            bgKey_ = key;
        }
        return bgPreview_;
    }

    int MainView::Reserve(const FrameSettings& frame, bool square) const
    {
        return square && s_.effects.desktop ? TaskbarHeight(frame.style) : 0;
    }

    Rect MainView::TaskTargetFor(const FrameSettings& frame, int outW, int outH, int scale) const
    {
        return s_.effects.desktop ? TaskTarget(frame, outW, outH, scale) : Rect{};
    }

    std::vector<ui::ListItem> MainView::PresetItems() const
    {
        std::vector<ui::ListItem> items;
        for (const auto& pr : s_.presets)
        {
            ui::ListItem item;
            item.text = pr.name;
            if (pr.look.iconIndex == CustomIconIndex)
            {
                item.icon = pr.icon;
            }
            else if (pr.look.iconIndex >= 0 && pr.look.iconIndex < FrameIconCount())
            {
                item.icon = BuiltinIcon(FrameIconAt(pr.look.iconIndex));
            }
            items.push_back(item);
        }
        return items;
    }

    void MainView::ApplyPreset(int index)
    {
        if (index < 0 || index >= int(s_.presets.size()))
        {
            return;
        }
        const LookPreset pr = s_.presets[std::size_t(index)];
        s_.frame.style = pr.style;
        s_.frame.colors = pr.colors;
        s_.frame.active = pr.active;
        s_.frame.sunken = pr.sunken;
        s_.frame.grip = pr.grip;
        s_.frame.scale = pr.scale;
        s_.scheme = pr.scheme;
        Look() = pr.look;
        if (pr.look.iconIndex == CustomIconIndex)
        {
            if (!pr.icon.Empty())
            {
                SetCustomIcon(pr.icon);
            }
            else if (s_.customIcon.Empty())
            {
                Look().iconIndex = DefaultLook(mode_).iconIndex;
            }
        }
        flash_ = L"Набор: " + pr.name;
        flashUntil_ = host_.Ticks() + 3000;
        Changed();
    }

    bool MainView::SavePreset(const std::wstring& name)
    {
        LookPreset pr;
        pr.name = name;
        pr.style = s_.frame.style;
        pr.scheme = s_.scheme;
        pr.colors = s_.frame.colors;
        pr.active = s_.frame.active;
        pr.sunken = s_.frame.sunken;
        pr.grip = s_.frame.grip;
        pr.scale = s_.frame.scale;
        pr.look = Look();
        if (pr.look.iconIndex == CustomIconIndex)
        {
            pr.icon = s_.customIcon;
        }
        bool replaced = false;
        for (auto& existing : s_.presets)
        {
            if (existing.name == name)
            {
                existing = pr;
                replaced = true;
            }
        }
        if (!replaced)
        {
            s_.presets.push_back(pr);
        }
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
        return true;
    }

    void MainView::DeletePreset(int index)
    {
        if (index < 0 || index >= int(s_.presets.size()))
        {
            return;
        }
        s_.presets.erase(s_.presets.begin() + index);
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
    }

    void MainView::EditPresets()
    {
        PresetsView view([this]() { return PresetItems(); }, [this](int i) { ApplyPreset(i); }, [this](const std::wstring& name) { return SavePreset(name); },
                         [this](int i) { DeletePreset(i); });
        host_.RunModal(view);
        root.Post([this]() { BuildMenu(); });
    }

    void MainView::EditEffects()
    {
        EffectsView view(s_.effects);
        if (host_.RunModal(view) != 1)
        {
            return;
        }
        s_.effects = view.Result();
        bgKey_.clear();
        Changed();
    }

    FrameSettings MainView::FrameFor(const Photo& photo) const
    {
        return LookFrame(PhotoMode, photo.name);
    }

    FrameVars MainView::VarsFor(const Photo& photo) const
    {
        return FrameVars{photo.name, photo.width, photo.height, L""};
    }

    void MainView::UpdatePreview()
    {
        if (mode_ == VideoMode)
        {
            UpdateVideoPreview();
            return;
        }
        if (AudioMode())
        {
            UpdateMusicPreview();
            return;
        }
        if (SceneMode())
        {
            UpdateScenePreview();
            return;
        }
        if (current_ < 0 || current_ >= int(photos_.size()))
        {
            preview_->image = Image();
            preview_->message = L"Перетащите сюда фото или нажмите «Открыть...».\n\nКартинку из буфера обмена можно вставить по Ctrl+V.";
            root.dirty = true;
            return;
        }
        const Photo& ph = photos_[std::size_t(current_)];
        const FrameSettings fs = FrameFor(ph);
        const int k = EffectiveScale(fs, ph.width, ph.height);
        int cw = std::max(1, (ph.width + k - 1) / k);
        int ch = std::max(1, (ph.height + k - 1) / k);
        const int longest = std::max(cw, ch);
        if (longest > PreviewLimit)
        {
            cw = std::max(1, int(std::lround(double(cw) * PreviewLimit / longest)));
            ch = std::max(1, int(std::lround(double(ch) * PreviewLimit / longest)));
        }
        Rect client;
        Image frame = RenderFrame(fs, cw, ch, VarsFor(ph), &client);
        Painter p(frame);
        p.Draw(ph.preview.width == cw && ph.preview.height == ch ? ph.preview : ResizeSmooth(ph.preview, cw, ch), client.x, client.y);
        ApplyEffects(frame, fs, Stage{Rect{0, 0, frame.width, frame.height}, 1, Rect{client.x, client.y, cw, ch}, Rect{}, Rect{}, 0.0}, 0.0, 0);
        const int availW = preview_->rect.w - 4 - 16;
        const int availH = preview_->rect.h - 4 - 16;
        preview_->image = frame.width > availW || frame.height > availH ? FitInside(frame, availW, availH, false) : std::move(frame);
        preview_->message.clear();
        root.dirty = true;
    }

    void MainView::UpdateStatus()
    {
        std::wstring first;
        std::wstring second;
        std::wstring third = L"Файлов: " + std::to_wstring(photos_.size());
        int k = 1;
        if (current_ >= 0 && current_ < int(photos_.size()))
        {
            const Photo& ph = photos_[std::size_t(current_)];
            first = ph.name + L" — " + std::to_wstring(ph.width) + L" x " + std::to_wstring(ph.height);
            k = EffectiveScale(s_.frame, ph.width, ph.height);
            second = L"Рамка в файле: " + ScaleText(k);
            scale_->items[0] = L"Авто (сейчас " + ScaleText(AutoScale(ph.width, ph.height)) + L")";
        }
        else
        {
            first = L"Перетащите фото в окно или нажмите Ctrl+V";
            second = L"Рамка в файле: авто";
            scale_->items[0] = L"Авто";
        }
        if (mode_ == VideoMode)
        {
            UpdateVideoStatus(first, second, third);
        }
        else if (AudioMode())
        {
            UpdateMusicStatus(first, second, third);
        }
        else if (SceneMode())
        {
            UpdateSceneStatus(first, second, third);
        }
        if (!flash_.empty())
        {
            first = flash_;
        }
        status_->fields[0].text = first;
        status_->fields[1].text = second;
        status_->fields[2].text = third;
        filesLabel_->text = L"Файлы (" + std::to_wstring(photos_.size()) + L"):";
        root.dirty = true;
    }

    void MainView::AddImage(Image full, const std::wstring& path, const std::wstring& name, bool keepFull)
    {
        Photo ph;
        ph.path = path;
        ph.name = name;
        ph.width = full.width;
        ph.height = full.height;
        ph.preview = FitInside(full, PreviewMaxW, PreviewMaxH, false);
        ph.thumb = FitInside(ph.preview, 76, 46, false);
        if (keepFull)
        {
            ph.full = std::move(full);
        }
        thumbs_->items.push_back(ui::ThumbItem{ph.thumb, ph.name});
        photos_.push_back(std::move(ph));
        Select(int(photos_.size()) - 1);
    }

    void MainView::AddFiles(const std::vector<std::wstring>& paths)
    {
        std::wstring errors;
        int failed = 0;
        for (const auto& path : paths)
        {
            Image full;
            std::wstring err;
            if (!host_.ReadImage(path, full, err))
            {
                ++failed;
                if (failed <= 8)
                {
                    errors += FileNameOf(path) + L": " + err + L"\n";
                }
                continue;
            }
            s_.openFolder = DirectoryOf(path);
            AddImage(std::move(full), path, FileNameOf(path), false);
        }
        if (failed > 0)
        {
            if (failed > 8)
            {
                errors += L"…и ещё " + std::to_wstring(failed - 8) + L"\n";
            }
            Warn(L"Не удалось открыть некоторые файлы:\n\n" + errors);
        }
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
    }

    void MainView::Select(int index)
    {
        if (photos_.empty())
        {
            current_ = -1;
        }
        else
        {
            current_ = std::clamp(index, 0, int(photos_.size()) - 1);
        }
        thumbs_->selected = current_;
        thumbs_->EnsureVisible(current_);
        SyncControls();
        UpdatePreview();
        UpdateStatus();
    }

    bool MainView::FullImage(const Photo& photo, Image& out, std::wstring& error)
    {
        if (!photo.full.Empty())
        {
            out = photo.full;
            return true;
        }
        return host_.ReadImage(photo.path, out, error);
    }

    bool MainView::Compose(const Photo& photo, Image& out, std::wstring& error)
    {
        Image full;
        if (!FullImage(photo, full, error))
        {
            return false;
        }
        const FrameSettings fs = FrameFor(photo);
        const int k = EffectiveScale(fs, full.width, full.height);
        Rect area;
        out = ComposeFramed(full, fs, VarsFor(photo), k, &area);
        ApplyEffects(out, fs, Stage{Rect{0, 0, out.width, out.height}, k, Rect{area.x, area.y, full.width, full.height}, Rect{}, Rect{}, 0.0}, 0.0, 0);
        return true;
    }

    ImageFormat MainView::Format() const
    {
        return s_.format == 0 ? ImageFormat::Png : ImageFormat::Jpeg;
    }

    int MainView::Quality() const
    {
        return s_.format == 2 ? 80 : 95;
    }

    std::wstring MainView::Extension() const
    {
        return s_.format == 0 ? L".png" : L".jpg";
    }

    std::wstring MainView::UniquePath(const std::wstring& dir, const std::wstring& stem) const
    {
        const std::wstring base = JoinPath(dir, stem + L"_retro");
        std::wstring path = base + Extension();
        int n = 2;
        while (host_.FileExists(path))
        {
            path = base + L" (" + std::to_wstring(n++) + L")" + Extension();
        }
        return path;
    }

    bool MainView::Fail(std::wstring* failure, const std::wstring& text)
    {
        if (failure != nullptr)
        {
            *failure = text;
        }
        else
        {
            Warn(text);
        }
        return false;
    }

    void MainView::Warn(const std::wstring& text)
    {
        ShowMessage(host_, L"Ретро-рамка", text, MessageKind::Warning);
    }

    void MainView::Open()
    {
        if (SceneMode())
        {
            OpenSceneText();
            return;
        }
        if (mode_ == VideoMode)
        {
            const auto paths = host_.AskOpenVideo(s_.videoFolder.empty() ? s_.openFolder : s_.videoFolder);
            if (!paths.empty())
            {
                OpenMedia(VideoMode, paths);
            }
            return;
        }
        if (AudioMode())
        {
            const auto paths = host_.AskOpenAudio(s_.musicFolder.empty() ? s_.openFolder : s_.musicFolder);
            if (!paths.empty())
            {
                OpenMedia(mode_, paths);
            }
            return;
        }
        const auto paths = host_.AskOpenImages(s_.openFolder);
        if (!paths.empty())
        {
            SetMode(PhotoMode);
            AddFiles(paths);
        }
    }

    void MainView::Paste()
    {
        const auto files = host_.ClipboardFiles();
        if (!files.empty())
        {
            OnDropFiles(files);
            return;
        }
        if (mode_ == VideoMode)
        {
            Warn(L"В буфере обмена нет видеофайла.\nСкопируйте ролик в Проводнике и вставьте снова.");
            return;
        }
        if (AudioMode())
        {
            Warn(L"В буфере обмена нет песни.\nСкопируйте файл в Проводнике и вставьте снова.");
            return;
        }
        if (SceneMode())
        {
            root.SetFocus(sceneText_);
            sceneText_->OnKey(ui::KeyEvent{ui::KeyCode::Letter, L'V', false, true, false});
            return;
        }
        Image img;
        if (host_.ClipboardImage(img))
        {
            ++pasteCounter_;
            AddImage(std::move(img), L"", L"Вставка " + std::to_wstring(pasteCounter_) + L".png", true);
            return;
        }
        Warn(L"В буфере обмена нет картинки или файлов с фото.");
    }

    void MainView::SaveCurrent()
    {
        if (mode_ == VideoMode)
        {
            SaveVideo();
            return;
        }
        if (SceneMode())
        {
            SaveScene();
            return;
        }
        if (AudioMode())
        {
            SaveMusic();
            return;
        }
        if (current_ < 0)
        {
            Warn(L"Сначала добавьте фото.");
            return;
        }
        const Photo& ph = photos_[std::size_t(current_)];
        const std::wstring suggested = StemOf(ph.name) + L"_retro" + Extension();
        const std::wstring path = host_.AskSavePath(s_.saveFolder, suggested, Format());
        if (path.empty())
        {
            return;
        }
        Image out;
        std::wstring err;
        if (!Compose(ph, out, err))
        {
            Warn(L"Не удалось прочитать фото:\n" + err);
            return;
        }
        if (!host_.WriteImage(path, out, Format(), Quality(), err))
        {
            Warn(L"Не удалось сохранить файл:\n" + err);
            return;
        }
        s_.saveFolder = DirectoryOf(path);
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
        flash_ = L"Сохранено: " + FileNameOf(path);
        flashUntil_ = host_.Ticks() + 4000;
        UpdateStatus();
    }

    void MainView::OpenMedia(int mode, const std::vector<std::wstring>& paths)
    {
        if (paths.empty())
        {
            return;
        }
        StopPlay();
        queue_[std::size_t(mode)] = paths;
        queuePos_[std::size_t(mode)] = 0;
        if (mode == VideoMode)
        {
            OpenVideo(paths.front());
        }
        else
        {
            SetMode(mode);
            OpenTrack(paths.front());
        }
        SyncControls();
        UpdateStatus();
    }

    void MainView::StepQueue(int delta)
    {
        if (mode_ == PhotoMode)
        {
            Select(current_ + delta);
            return;
        }
        auto& q = queue_[std::size_t(mode_)];
        int& pos = queuePos_[std::size_t(mode_)];
        const int next = std::clamp(pos + delta, 0, int(q.size()) - 1);
        if (q.empty() || next == pos)
        {
            return;
        }
        StopPlay();
        pos = next;
        if (mode_ == VideoMode)
        {
            OpenVideo(q[std::size_t(pos)]);
        }
        else
        {
            OpenTrack(q[std::size_t(pos)]);
        }
        SyncControls();
        UpdateStatus();
    }

    void MainView::SaveQueue()
    {
        const auto q = queue_[std::size_t(mode_)];
        if (q.size() < 2)
        {
            SaveCurrent();
            return;
        }
        if (host_.FfmpegPath().empty())
        {
            ShowMessage(host_, L"Ретро-рамка", FfmpegMissingText(), MessageKind::Warning);
            return;
        }
        const std::wstring dir = host_.AskFolder(s_.saveFolder, L"Папка для сохранения всех файлов");
        if (dir.empty())
        {
            return;
        }
        StopPlay();
        s_.saveFolder = dir;
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
        const bool video = mode_ == VideoMode;
        const std::wstring ext = video ? (s_.video.format == 3 ? L".gif" : L".mp4") : AudioExtension();
        const int n = int(q.size());
        int saved = 0;
        bool cancelled = false;
        std::wstring problems;
        for (int i = 0; i < n; ++i)
        {
            queuePos_[std::size_t(mode_)] = i;
            const std::wstring& source = q[std::size_t(i)];
            std::wstring err;
            const bool opened = video ? OpenVideo(source, &err) : OpenTrack(source, &err);
            if (!opened)
            {
                problems += L"\n" + FileNameOf(source);
                continue;
            }
            std::wstring path = JoinPath(dir, StemOf(FileNameOf(source)) + L"_retro" + ext);
            for (int k = 2; host_.FileExists(path); ++k)
            {
                path = JoinPath(dir, StemOf(FileNameOf(source)) + L"_retro (" + std::to_wstring(k) + L")" + ext);
            }
            const std::wstring title = L"Сохранение: файл " + std::to_wstring(i + 1) + L" из " + std::to_wstring(n);
            const ExportResult outcome = video ? ExportClip(path, title, err) : ExportTrack(path, title, err);
            if (outcome == ExportResult::Cancelled)
            {
                cancelled = true;
                break;
            }
            if (outcome == ExportResult::Failed)
            {
                problems += L"\n" + FileNameOf(source);
                continue;
            }
            ++saved;
        }
        SyncControls();
        UpdatePreview();
        UpdateStatus();
        std::wstring text = L"Сохранено файлов: " + std::to_wstring(saved) + L" из " + std::to_wstring(n) + L".";
        if (cancelled)
        {
            text += L"\nСохранение остановлено.";
        }
        if (!problems.empty())
        {
            text += L"\n\nНе получилось:" + problems;
        }
        ShowMessage(host_, L"Сохранить все", text, problems.empty() ? MessageKind::Info : MessageKind::Warning);
    }

    void MainView::SaveAll()
    {
        if (mode_ != PhotoMode)
        {
            SaveQueue();
            return;
        }
        if (photos_.empty())
        {
            Warn(L"Сначала добавьте фото.");
            return;
        }
        const std::wstring dir = host_.AskFolder(s_.saveFolder, L"Папка для сохранения");
        if (dir.empty())
        {
            return;
        }
        s_.saveFolder = dir;
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
        ProgressView progress(L"Сохранение");
        void* handle = host_.OpenModeless(progress);
        int saved = 0;
        std::wstring errors;
        const int n = int(photos_.size());
        for (int i = 0; i < n; ++i)
        {
            if (progress.cancelled)
            {
                break;
            }
            const Photo& ph = photos_[std::size_t(i)];
            progress.Update(L"Сохраняется: " + ph.name, std::to_wstring(i + 1) + L" из " + std::to_wstring(n), double(i) / n);
            host_.Pump();
            if (progress.cancelled)
            {
                break;
            }
            Image out;
            std::wstring err;
            const std::wstring path = UniquePath(dir, StemOf(ph.name));
            if (!Compose(ph, out, err) || !host_.WriteImage(path, out, Format(), Quality(), err))
            {
                errors += ph.name + L": " + err + L"\n";
                continue;
            }
            ++saved;
            progress.Update(L"Сохраняется: " + ph.name, std::to_wstring(i + 1) + L" из " + std::to_wstring(n), double(i + 1) / n);
            host_.Pump();
        }
        host_.CloseModeless(handle);
        std::wstring text = L"Сохранено файлов: " + std::to_wstring(saved) + L" из " + std::to_wstring(n) + L".\nПапка: " + dir;
        if (progress.cancelled)
        {
            text += L"\n\nСохранение остановлено.";
        }
        if (!errors.empty())
        {
            text += L"\n\nОшибки:\n" + errors;
        }
        ShowMessage(host_, L"Сохранение", text, errors.empty() ? MessageKind::Info : MessageKind::Warning);
    }

    void MainView::CopyCurrent()
    {
        if (mode_ == VideoMode)
        {
            Warn(L"Видео нельзя положить в буфер обмена: нажмите «Сохранить...».");
            return;
        }
        if (SceneMode())
        {
            CopyScene();
            return;
        }
        if (AudioMode())
        {
            CopyMusic();
            return;
        }
        if (current_ < 0)
        {
            Warn(L"Сначала добавьте фото.");
            return;
        }
        Image out;
        std::wstring err;
        if (!Compose(photos_[std::size_t(current_)], out, err))
        {
            Warn(L"Не удалось прочитать фото:\n" + err);
            return;
        }
        if (!host_.SetClipboardImage(out))
        {
            Warn(L"Не удалось положить картинку в буфер обмена.");
            return;
        }
        flash_ = L"Результат скопирован в буфер обмена";
        flashUntil_ = host_.Ticks() + 4000;
        UpdateStatus();
    }

    void MainView::RemoveCurrent()
    {
        if (current_ < 0 || current_ >= int(photos_.size()))
        {
            return;
        }
        photos_.erase(photos_.begin() + current_);
        thumbs_->items.erase(thumbs_->items.begin() + current_);
        Select(current_ >= int(photos_.size()) ? current_ - 1 : current_);
    }

    void MainView::RemoveAll()
    {
        photos_.clear();
        thumbs_->items.clear();
        Select(-1);
    }

    void MainView::PickColor(Color FrameColors::*field)
    {
        Color result = s_.frame.colors.*field;
        if (!host_.AskColor(result, result))
        {
            return;
        }
        s_.frame.colors.*field = result;
        if (field == &FrameColors::active1 && !HasGradient(s_.frame.style))
        {
            s_.frame.colors.active2 = result;
        }
        if (field == &FrameColors::inactive1 && !HasGradient(s_.frame.style))
        {
            s_.frame.colors.inactive2 = result;
        }
        s_.scheme = -1;
        Changed();
    }

    void MainView::LoadCustomIcon()
    {
        const std::wstring path = host_.AskOpenIcon(s_.openFolder);
        if (path.empty())
        {
            return;
        }
        std::wstring err;
        if (IsProgramFile(path))
        {
            std::vector<Image> icons;
            if (!host_.ReadFileIcons(path, icons, err) || icons.empty())
            {
                Warn(L"Не удалось достать иконки из файла «" + FileNameOf(path) + L"»:\n" + (err.empty() ? std::wstring(L"в файле нет иконок") : err));
                return;
            }
            int index = 0;
            if (icons.size() > 1)
            {
                IconPickerView picker(FileNameOf(path), icons);
                if (host_.RunModal(picker) != 1 || picker.Picked() < 0)
                {
                    return;
                }
                index = picker.Picked();
            }
            SetCustomIcon(MakeIcon16(icons[std::size_t(index)]));
            return;
        }
        Image img;
        if (!host_.ReadIcon(path, img, err))
        {
            Warn(L"Не удалось открыть картинку для иконки:\n" + err);
            return;
        }
        SetCustomIcon(MakeIcon16(img));
    }

    void MainView::SetCustomIcon(const Image& picked)
    {
        s_.customIcon = picked;
        Look().iconIndex = CustomIconIndex;
        const std::wstring dir = host_.SettingsFolder();
        if (!dir.empty())
        {
            std::wstring err;
            host_.WriteImage(JoinPath(dir, L"icon.png"), s_.customIcon, ImageFormat::Png, 100, err);
        }
        Changed();
    }

    void MainView::SyncLibrary()
    {
        s_.library.clear();
        for (const auto& source : library_)
        {
            s_.library.push_back(EncodeSource(source));
        }
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
    }

    void MainView::OpenLibrary()
    {
        std::wstring folder = s_.libraryFolder.empty() ? s_.openFolder : s_.libraryFolder;
        LibraryView view(host_, library_, folder, librarySource_);
        view.onSourcesChanged = [this]() { SyncLibrary(); };
        const int answer = host_.RunModal(view);
        librarySource_ = view.CurrentSource();
        if (folder != s_.libraryFolder)
        {
            s_.libraryFolder = folder;
            SyncLibrary();
        }
        if (answer == 1 && view.HasPick())
        {
            SetCustomIcon(view.Picked().icon);
        }
        else if (answer == 2 && view.HasDrawn())
        {
            SetCustomIcon(view.Drawn());
        }
    }

    void MainView::DrawIcon()
    {
        const ModeLook& look = Look();
        Image start;
        if (look.iconIndex == CustomIconIndex)
        {
            start = s_.customIcon;
        }
        else if (look.iconIndex >= 0 && look.iconIndex < FrameIconCount())
        {
            start = BuiltinIcon(FrameIconAt(look.iconIndex));
        }
        IconEditorView editor(host_, start);
        if (host_.RunModal(editor) == 1)
        {
            SetCustomIcon(editor.Result());
        }
    }

    void MainView::LoadCorner()
    {
        const std::wstring path = host_.AskOpenIcon(s_.openFolder);
        if (path.empty())
        {
            if (s_.cornerImage.Empty())
            {
                Look().corner = false;
                SyncControls();
            }
            return;
        }
        Image img;
        std::wstring err;
        if (!host_.ReadIcon(path, img, err))
        {
            Warn(L"Не удалось открыть картинку:\n" + err);
            return;
        }
        s_.cornerImage = MakeIcon16(img);
        Look().corner = true;
        Look().menu = true;
        const std::wstring dir = host_.SettingsFolder();
        if (!dir.empty())
        {
            host_.WriteImage(JoinPath(dir, L"corner.png"), s_.cornerImage, ImageFormat::Png, 100, err);
        }
        Changed();
    }

    void MainView::EditTexts()
    {
        if (mode_ != PhotoMode)
        {
            PlayerTextsView dialog(s_.playerMenu, s_.playerStatus, s_.playerTime, s_.frame.addressText);
            if (host_.RunModal(dialog) == 1)
            {
                s_.playerMenu = dialog.Menu();
                s_.playerStatus = dialog.Status();
                s_.playerTime = dialog.Time();
                s_.frame.addressText = dialog.Address();
                Changed();
            }
            return;
        }
        TextsView dialog(s_.frame);
        if (host_.RunModal(dialog) == 1)
        {
            dialog.Apply(s_.frame);
            Changed();
        }
    }

    void MainView::ResetSettings()
    {
        const Image icon = s_.customIcon;
        const Image corner = s_.cornerImage;
        const std::wstring openFolder = s_.openFolder;
        const std::wstring saveFolder = s_.saveFolder;
        const std::vector<LookPreset> presets = s_.presets;
        s_ = DefaultSettings();
        s_.presets = presets;
        s_.customIcon = icon;
        s_.cornerImage = corner;
        s_.openFolder = openFolder;
        s_.saveFolder = saveFolder;
        Changed();
    }

    void MainView::About()
    {
        ShowMessage(host_, L"О программе",
                    std::wstring(L"Ретро-рамка, версия ") + AppVersion +
                        L"\n\nОформляет фото, видео и музыку в окна в стиле Windows 95, 98 и 2000.",
                    MessageKind::Info);
    }

    void MainView::Exit()
    {
        StopPlay();
        fetch_.reset();
        SaveSettings();
        host_.Quit();
    }

    void MainView::SetStyle(Style style)
    {
        s_.frame.style = style;
        if (s_.scheme >= 0)
        {
            s_.frame.colors = SchemeColors(s_.scheme, style);
        }
        else if (!HasGradient(style))
        {
            s_.frame.colors.active2 = s_.frame.colors.active1;
            s_.frame.colors.inactive2 = s_.frame.colors.inactive1;
        }
        Changed();
    }

    void MainView::SetScheme(int index)
    {
        s_.scheme = index;
        s_.frame.colors = SchemeColors(index, s_.frame.style);
        Changed();
    }

    bool MainView::HandleKey(const ui::KeyEvent& e)
    {
        const bool textFocus = dynamic_cast<ui::TextField*>(root.focus) != nullptr || dynamic_cast<ui::TextArea*>(root.focus) != nullptr;
        if (e.key == ui::KeyCode::Letter && e.ctrl)
        {
            switch (e.letter)
            {
            case L'O':
                Open();
                return true;
            case L'S':
                SaveCurrent();
                return true;
            case L'V':
                if (textFocus)
                {
                    return false;
                }
                Paste();
                return true;
            case L'C':
                if (textFocus)
                {
                    return false;
                }
                CopyCurrent();
                return true;
            default:
                return false;
            }
        }
        if (textFocus)
        {
            return false;
        }
        if (mode_ != PhotoMode)
        {
            if (e.key == ui::KeyCode::PageUp || e.key == ui::KeyCode::PageDown)
            {
                StepQueue(e.key == ui::KeyCode::PageUp ? -1 : 1);
                return true;
            }
            if (Cur() && (e.key == ui::KeyCode::Left || e.key == ui::KeyCode::Right))
            {
                StopPlay();
                SetPosition(Cur()->position + (e.key == ui::KeyCode::Left ? -1.0 : 1.0));
                return true;
            }
            return false;
        }
        if (e.key == ui::KeyCode::Delete)
        {
            RemoveCurrent();
            return true;
        }
        if (e.key == ui::KeyCode::Left && current_ > 0)
        {
            Select(current_ - 1);
            return true;
        }
        if (e.key == ui::KeyCode::Right && current_ + 1 < int(photos_.size()))
        {
            Select(current_ + 1);
            return true;
        }
        return false;
    }
}
