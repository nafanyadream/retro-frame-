#include "mainview.h"

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

        ui::CheckBox* AddCheck(ui::Root& root, int x, int y, int w, const std::wstring& text)
        {
            auto* c = Place<ui::CheckBox>(root, x, y, w, 16);
            c->text = text;
            return c;
        }

        int FadeIndex(int seconds)
        {
            for (int i = 0; i < int(std::size(FadeChoices)); ++i)
            {
                if (FadeChoices[i] == seconds)
                {
                    return i;
                }
            }
            return 2;
        }
    }

    void MainView::BuildMusic()
    {
        ui::Root& r = root;
        auto* group = Place<ui::GroupBox>(r, 324, 108, 294, 300);
        group->text = L"Трек";
        auto* l1 = AddLabel(r, 334, 132, 82, L"Исполнитель:");
        artistField_ = Place<ui::TextField>(r, 418, 128, 190, 21);
        artistField_->onChange = [this](const std::wstring& t) {
            if (track_)
            {
                track_->artist = t;
                UpdatePreview();
            }
        };
        auto* l2 = AddLabel(r, 334, 158, 82, L"Название:");
        songField_ = Place<ui::TextField>(r, 418, 154, 190, 21);
        songField_->onChange = [this](const std::wstring& t) {
            if (track_)
            {
                track_->title = t;
                UpdatePreview();
            }
        };
        auto* l3 = AddLabel(r, 334, 186, 82, L"Обложка:");
        coverLoad_ = Place<ui::Button>(r, 418, 182, 92, 21);
        coverLoad_->text = L"Выбрать...";
        coverLoad_->onClick = [this]() { LoadCover(); };
        coverReset_ = Place<ui::Button>(r, 514, 182, 94, 21);
        coverReset_->text = L"Из файла";
        coverReset_->onClick = [this]() { ResetCover(); };
        spinCheck_ = AddCheck(r, 334, 214, 270, L"Диск крутится");
        spinCheck_->onChange = [this](bool v) {
            s_.music.spin = v;
            Changed();
        };
        meterCheck_ = AddCheck(r, 334, 234, 270, L"Шкала громкости прыгает под музыку");
        meterCheck_->onChange = [this](bool v) {
            s_.music.meter = v;
            Changed();
        };
        equalizerCheck_ = AddCheck(r, 334, 254, 270, L"Эквалайзер вместо диска");
        equalizerCheck_->onChange = [this](bool v) {
            s_.music.equalizer = v;
            Changed();
        };
        auto* l4 = AddLabel(r, 334, 284, 140, L"Затухание фрагмента:");
        fade_ = Place<ui::ComboBox>(r, 476, 280, 132, 21);
        fade_->items = {L"выключено", L"1 секунда", L"2 секунды", L"3 секунды", L"5 секунд"};
        fade_->onChange = [this](int i) {
            s_.music.fade = FadeChoices[std::clamp(i, 0, int(std::size(FadeChoices)) - 1)];
            Changed();
        };
        auto* hint = AddLabel(r, 334, 352, 274, L"Затухание работает, когда выбран фрагмент.");
        hint->color = Rgb(0x40, 0x40, 0x40);
        levelsLabel_ = AddLabel(r, 334, 374, 274, L"");
        levelsLabel_->color = Rgb(0x40, 0x40, 0x40);
        auto* resultHint = AddLabel(r, 324, 544, 284, L"MP4 — видео с песней.\nPNG — один кадр на выбранной секунде,\nтот же кадр копирует Ctrl+C.");
        resultHint->rect.h = 40;
        resultHint->color = Rgb(0x40, 0x40, 0x40);
        musicOnly_ = {group, l1, artistField_, l2, songField_, l3, coverLoad_, coverReset_, spinCheck_, meterCheck_, equalizerCheck_, l4, fade_, hint, levelsLabel_,
                      resultHint};
    }

    void MainView::SyncMusicControls()
    {
        const bool has = track_ != nullptr;
        for (ui::Widget* w : {static_cast<ui::Widget*>(artistField_), static_cast<ui::Widget*>(songField_), static_cast<ui::Widget*>(coverLoad_),
                              static_cast<ui::Widget*>(coverReset_)})
        {
            w->enabled = has;
        }
        const auto fill = [](ui::TextField* field, const std::wstring& text) {
            if (!field->IsFocused() && field->text != text)
            {
                field->SetText(text);
            }
        };
        fill(artistField_, has ? track_->artist : std::wstring());
        fill(songField_, has ? track_->title : std::wstring());
        spinCheck_->checked = s_.music.spin;
        meterCheck_->checked = s_.music.meter;
        equalizerCheck_->checked = s_.music.equalizer;
        fade_->index = FadeIndex(s_.music.fade);
        levelsLabel_->text = has && !track_->levelsReady ? L"Слушаю песню для шкалы и эквалайзера..." : L"";
    }

    FrameSettings MainView::MusicFrameSettings() const
    {
        FrameSettings f = LookFrame(MusicMode, track_ ? track_->name : std::wstring(L"песня.mp3"));
        f.menuItems = s_.playerMenu;
        f.statusFields = {s_.playerStatus, s_.playerTime ? std::wstring(L"{время}") : std::wstring()};
        f.statusIcon = false;
        return f;
    }

    MusicFrame MainView::TrackFrame() const
    {
        const double start = TrimStart();
        const double length = std::max(0.001, TrimEnd() - start);
        MusicFrame mf = MusicFrameAt(track_->position - start, length, track_->position, s_.music.spin, s_.music.meter, track_->levels);
        if (s_.music.equalizer)
        {
            AddSpectrum(mf, track_->spectrum, track_->position);
        }
        return mf;
    }

    MusicPlan MainView::MusicPlanFor(const FrameSettings& frame) const
    {
        const CanvasSize c = s_.music.square ? CanvasAt(s_.music.canvas) : CanvasSize{SquareSizes[0], SquareSizes[0]};
        return PlanMusic(frame, c.width, c.height, s_.music.square);
    }

    void MainView::OpenTrack(const std::wstring& path)
    {
        SetMode(MusicMode);
        if (host_.FfmpegPath().empty())
        {
            ShowMessage(host_, L"Ретро-рамка", FfmpegMissingText(), MessageKind::Warning);
            return;
        }
        MediaInfo info;
        std::wstring err;
        if (!ProbeAudio(host_, path, info, err))
        {
            Warn(L"Не удалось открыть песню «" + FileNameOf(path) + L"»:\n" + err);
            return;
        }
        if (info.duration <= 0.0)
        {
            Warn(L"Не удалось узнать длительность песни «" + FileNameOf(path) + L"».");
            return;
        }
        StopPlay();
        levels_.reset();
        auto track = std::make_unique<Media>();
        track->path = path;
        track->name = FileNameOf(path);
        track->info = info;
        track->end = info.duration;
        track->artist = info.artist;
        track->title = info.title.empty() ? StemOf(track->name) : info.title;
        Image cover;
        if (ReadCover(host_, path, info, SquareSizes[0], cover))
        {
            track->tagCover = cover;
            track->cover = std::move(cover);
        }
        track_ = std::move(track);
        levels_ = std::make_unique<LevelReader>(host_, path);
        s_.musicFolder = DirectoryOf(path);
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
        SyncControls();
        UpdatePreview();
        UpdateStatus();
    }

    void MainView::EnsureLevels()
    {
        if (!track_ || track_->levelsReady)
        {
            return;
        }
        if (!levels_)
        {
            levels_ = std::make_unique<LevelReader>(host_, track_->path);
        }
        levels_->Wait();
        track_->levels = levels_->Levels();
        track_->spectrum = levels_->Spectrum();
        track_->levelsReady = true;
        levels_.reset();
    }

    void MainView::UpdateMusicPreview()
    {
        preview_->message.clear();
        if (!track_)
        {
            preview_->image = Image();
            preview_->message = host_.FfmpegPath().empty() ? L"Для музыки нужна\nпрограмма ffmpeg.\n\nПоложите ffmpeg.exe\nрядом с RetroFrame.exe."
                                                           : L"Перетащите сюда песню\nили нажмите «Открыть...».\n\nПодходят MP3, FLAC, WAV,\nOGG и M4A.";
            root.dirty = true;
            return;
        }
        const FrameSettings f = MusicFrameSettings();
        const MusicPlan plan = MusicPlanFor(f);
        if (plan.square)
        {
            const int w = plan.outW / plan.scale;
            const int h = plan.outH / plan.scale;
            static const Image none;
            const Image& picture = s_.backgroundPicture ? s_.backgroundImage : none;
            const std::wstring key = std::to_wstring(w) + L"x" + std::to_wstring(h) + L"/" + std::to_wstring(s_.background) + L"/" + std::to_wstring(picture.width) +
                                     L"x" + std::to_wstring(picture.height);
            if (key != bgKey_)
            {
                bgPreview_ = CanvasBackground(picture, s_.background, w, h);
                bgKey_ = key;
            }
        }
        const FrameVars vars{track_->name, 0, 0, L""};
        MusicComposer composer(f, vars, plan, Image(), track_->artist, track_->title, track_->cover);
        const MusicFrame mf = TrackFrame();
        Image img = composer.Small(mf, plan.square ? bgPreview_ : Image());
        const int k = std::max(1, plan.scale);
        ApplyEffects(img, f, plan.square ? Rect{plan.windowX / k, plan.windowY / k, plan.windowW / k, plan.windowH / k} : Rect{0, 0, img.width, img.height}, 1, mf.now,
                     int(mf.now * 15.0));
        const int availW = preview_->rect.w - 4 - 12;
        const int availH = preview_->rect.h - 4 - 12;
        preview_->image = img.width > availW || img.height > availH ? FitInside(img, availW, availH, false) : std::move(img);
        root.dirty = true;
    }

    void MainView::UpdateMusicStatus(std::wstring& first, std::wstring& second, std::wstring& third)
    {
        if (!track_)
        {
            first = host_.FfmpegPath().empty() ? L"Для музыки нужна программа ffmpeg" : L"Перетащите песню в окно или нажмите «Открыть...»";
            second.clear();
            third.clear();
            scale_->items[0] = L"Авто";
            return;
        }
        first = track_->name + L" — " + FormatClock(track_->info.duration) + (track_->artist.empty() ? std::wstring() : L", " + track_->artist);
        const FrameSettings f = MusicFrameSettings();
        const MusicPlan plan = MusicPlanFor(f);
        const std::wstring kind = s_.music.format == 3 ? L"PNG " : plan.square ? L"" : L"Окно ";
        second = plan.square ? kind + std::to_wstring(plan.outW) + L"x" + std::to_wstring(plan.outH) + L", окно " + std::to_wstring(plan.scale) + L"x"
                             : kind + std::to_wstring(plan.outW) + L" x " + std::to_wstring(plan.outH) + L", " + std::to_wstring(plan.scale) + L"x";
        third = L"Фрагмент: " + FormatPrecise(TrimEnd() - TrimStart());
        FrameSettings automatic = f;
        automatic.scale = 0;
        scale_->items[0] = L"Авто (сейчас " + std::to_wstring(MusicPlanFor(automatic).scale) + L"x)";
    }

    bool MainView::MusicStill(Image& out)
    {
        if (!track_)
        {
            return false;
        }
        const FrameSettings f = MusicFrameSettings();
        const MusicPlan plan = MusicPlanFor(f);
        const Image background = plan.square ? CanvasBackground(s_.backgroundPicture ? s_.backgroundImage : Image(), s_.background, plan.outW, plan.outH) : Image();
        MusicComposer composer(f, FrameVars{track_->name, 0, 0, L""}, plan, background, track_->artist, track_->title, track_->cover);
        composer.SetEffects(s_.effects);
        out = composer.Compose(TrackFrame());
        return true;
    }

    void MainView::SaveMusic()
    {
        if (!track_)
        {
            Warn(L"Сначала откройте песню.");
            return;
        }
        StopPlay();
        const std::wstring stem = StemOf(track_->name);
        if (s_.music.format == 3)
        {
            const std::wstring path = host_.AskSavePath(s_.saveFolder, stem + L"_retro.png", ImageFormat::Png);
            if (path.empty())
            {
                return;
            }
            Image img;
            std::wstring err;
            MusicStill(img);
            if (!host_.WriteImage(path, img, ImageFormat::Png, 100, err))
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
            return;
        }
        if (host_.FfmpegPath().empty())
        {
            ShowMessage(host_, L"Ретро-рамка", FfmpegMissingText(), MessageKind::Warning);
            return;
        }
        const std::wstring path = host_.AskSaveVideo(s_.saveFolder, stem + L"_retro.mp4", false);
        if (path.empty())
        {
            return;
        }
        MusicJob job;
        job.source = track_->path;
        job.output = path;
        job.quality = std::clamp(s_.music.format, 0, 2);
        job.start = TrimStart();
        job.duration = TrimEnd() - TrimStart();
        job.fade = track_->whole ? 0.0 : double(s_.music.fade);
        job.fps = double(s_.music.fps);
        job.spin = s_.music.spin;
        job.meter = s_.music.meter;
        job.frame = MusicFrameSettings();
        job.vars = FrameVars{track_->name, 0, 0, L""};
        job.plan = MusicPlanFor(job.frame);
        job.effects = s_.effects;
        job.background =
            job.plan.square ? CanvasBackground(s_.backgroundPicture ? s_.backgroundImage : Image(), s_.background, job.plan.outW, job.plan.outH) : Image();
        job.artist = track_->artist;
        job.title = track_->title;
        job.cover = track_->cover;
        std::wstring err;
        const ExportResult outcome = WithProgress(path, [&](const ExportStep& step) {
            if ((s_.music.meter || s_.music.equalizer) && !track_->levelsReady)
            {
                step(-1, 0);
                EnsureLevels();
            }
            job.levels = track_->levels;
            job.equalizer = s_.music.equalizer;
            job.spectrum = track_->spectrum;
            return ExportMusic(host_, job, step, err);
        });
        FinishExport(outcome, path, err);
    }

    void MainView::CopyMusic()
    {
        Image img;
        if (!MusicStill(img))
        {
            Warn(L"Сначала откройте песню.");
            return;
        }
        if (!host_.SetClipboardImage(img))
        {
            Warn(L"Не удалось положить картинку в буфер обмена.");
            return;
        }
        flash_ = L"Кадр скопирован в буфер обмена";
        flashUntil_ = host_.Ticks() + 4000;
        UpdateStatus();
    }

    void MainView::LoadCover()
    {
        if (!track_)
        {
            return;
        }
        const auto paths = host_.AskOpenImages(s_.openFolder);
        if (paths.empty())
        {
            return;
        }
        Image img;
        std::wstring err;
        if (!host_.ReadImage(paths.front(), img, err))
        {
            Warn(L"Не удалось открыть картинку для обложки:\n" + err);
            return;
        }
        track_->cover = std::max(img.width, img.height) > SquareSizes[0] ? FitInside(img, SquareSizes[0], SquareSizes[0], false) : img;
        UpdatePreview();
    }

    void MainView::ResetCover()
    {
        if (!track_)
        {
            return;
        }
        track_->cover = track_->tagCover;
        UpdatePreview();
    }
}
