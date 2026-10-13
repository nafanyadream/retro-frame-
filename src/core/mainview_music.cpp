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
        lyricsCheck_ = AddCheck(r, 334, 308, 170, L"Текст песни под плеером");
        lyricsCheck_->onChange = [this](bool v) {
            s_.music.lyrics = v;
            Changed();
        };
        lyricsLoad_ = Place<ui::Button>(r, 514, 304, 94, 21);
        lyricsLoad_->text = L"Файл .lrc...";
        lyricsLoad_->onClick = [this]() { PickLyrics(); };
        lyricsLabel_ = AddLabel(r, 334, 330, 274, L"");
        lyricsLabel_->color = Rgb(0x40, 0x40, 0x40);
        auto* hint = AddLabel(r, 334, 352, 274, L"Затухание работает, когда выбран фрагмент.");
        hint->color = Rgb(0x40, 0x40, 0x40);
        levelsLabel_ = AddLabel(r, 334, 374, 274, L"");
        levelsLabel_->color = Rgb(0x40, 0x40, 0x40);
        auto* resultHint = AddLabel(r, 324, 544, 284, L"MP4 — видео с песней.\nPNG — один кадр на выбранной секунде,\nтот же кадр копирует Ctrl+C.");
        resultHint->rect.h = 40;
        resultHint->color = Rgb(0x40, 0x40, 0x40);
        musicOnly_ = {group, l1, artistField_, l2, songField_, l3, coverLoad_, coverReset_, spinCheck_, meterCheck_, equalizerCheck_, l4, fade_, lyricsCheck_, lyricsLoad_,
                      lyricsLabel_, hint, levelsLabel_, resultHint};
    }

    void MainView::BuildRecorder()
    {
        ui::Root& r = root;
        auto* group = Place<ui::GroupBox>(r, 324, 108, 294, 300);
        group->text = L"Звукозапись";
        auto* l1 = AddLabel(r, 334, 132, 104, L"Язык надписей:");
        recLanguage_ = Place<ui::ComboBox>(r, 440, 128, 168, 21);
        recLanguage_->items = {L"English, как в оригинале", L"Русский"};
        recLanguage_->onChange = [this](int i) {
            s_.recorder.russian = i == 1;
            Changed();
        };
        auto* l2 = AddLabel(r, 334, 160, 140, L"Затухание фрагмента:");
        recFade_ = Place<ui::ComboBox>(r, 476, 156, 132, 21);
        recFade_->items = {L"выключено", L"1 секунда", L"2 секунды", L"3 секунды", L"5 секунд"};
        recFade_->onChange = [this](int i) {
            s_.recorder.fade = FadeChoices[std::clamp(i, 0, int(std::size(FadeChoices)) - 1)];
            Changed();
        };
        recInfo_ = AddLabel(r, 334, 192, 274, L"");
        recInfo_->rect.h = 28;
        auto* hint = AddLabel(r, 334, 232, 274,
                              L"Окно как «Звукозапись» из Windows 98 и ME.\nЗелёная волна — настоящий звук песни,\nпозиция и ползунок идут вместе с ней.\nЗатухание работает, когда выбран фрагмент.\n\nС микрофона программа не записывает:\nкнопки в окне нарисованы как в оригинале,\nа звук берётся из открытой песни.");
        hint->rect.h = 120;
        hint->color = Rgb(0x40, 0x40, 0x40);
        recLevels_ = AddLabel(r, 334, 390, 274, L"");
        recLevels_->color = Rgb(0x40, 0x40, 0x40);
        recorderOnly_ = {group, l1, recLanguage_, l2, recFade_, recInfo_, hint, recLevels_};
    }

    void MainView::SyncRecorderControls()
    {
        const Media* m = recording_.get();
        recLanguage_->index = s_.recorder.russian ? 1 : 0;
        recFade_->index = FadeIndex(s_.recorder.fade);
        recInfo_->text = m ? L"Файл: " + m->name + L"\nДлина: " + FormatClock(m->info.duration) + L" (" + RecorderSeconds(m->info.duration, true) + L")"
                           : L"Откройте или перетащите готовую песню:\nона заиграет в окне «Звукозаписи».";
        recLevels_->text = m && !m->levelsReady ? L"Слушаю песню для волны..." : L"";
    }

    void MainView::SyncMusicControls()
    {
        const bool has = Song() != nullptr;
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
        fill(artistField_, has ? Song()->artist : std::wstring());
        fill(songField_, has ? Song()->title : std::wstring());
        spinCheck_->checked = s_.music.spin;
        meterCheck_->checked = s_.music.meter;
        equalizerCheck_->checked = s_.music.equalizer;
        lyricsCheck_->checked = s_.music.lyrics;
        lyricsLoad_->enabled = has;
        if (!has)
        {
            lyricsLabel_->text = L"Текст берётся из файла .lrc рядом с песней.";
        }
        else if (Song()->lyrics.empty())
        {
            lyricsLabel_->text = L"Рядом с песней нет файла .lrc";
        }
        else
        {
            lyricsLabel_->text = L"Текст: " + Song()->lyricsName + L", строк: " + std::to_wstring(Song()->lyrics.size());
        }
        fade_->index = FadeIndex(Audio().fade);
        levelsLabel_->text = has && !Song()->levelsReady ? L"Слушаю песню для шкалы и эквалайзера..." : L"";
    }

    FrameSettings MainView::MusicFrameSettings() const
    {
        const std::wstring name = Song() ? Song()->name : std::wstring(L"песня.mp3");
        FrameSettings f = LookFrame(mode_, name);
        f.menuItems = s_.playerMenu;
        f.statusFields = {s_.playerStatus, s_.playerTime ? std::wstring(L"{время}") : std::wstring()};
        f.statusIcon = false;
        if (Recorder())
        {
            const ModeLook& look = s_.looks[RecorderMode];
            f.title = RecorderTitle(look.titleFromName ? StemOf(name) : look.customTitle, s_.recorder.russian);
            f.menuItems = RecorderMenu(s_.recorder.russian);
        }
        return f;
    }

    void MainView::SetupComposer(MusicComposer& composer) const
    {
        if (Recorder() && Song())
        {
            composer.SetRecorder(s_.recorder.russian, Song()->info.duration, Song()->samples);
        }
    }

    void MainView::TakeLevels(Media& m)
    {
        m.levels = m.reader->Levels();
        m.spectrum = m.reader->Spectrum();
        m.samples = std::make_shared<const std::vector<std::int16_t>>(m.reader->Samples());
        m.levelsReady = true;
        m.reader.reset();
    }

    MusicFrame MainView::TrackFrame() const
    {
        const double start = TrimStart();
        const double length = std::max(0.001, TrimEnd() - start);
        const bool player = !Recorder();
        MusicFrame mf = MusicFrameAt(Song()->position - start, length, Song()->position, player && s_.music.spin, player && s_.music.meter, Song()->levels);
        if (player && s_.music.equalizer)
        {
            AddSpectrum(mf, Song()->spectrum, Song()->position);
        }
        if (ShowLyrics())
        {
            AddLyrics(mf, Song()->lyrics, Song()->position);
        }
        return mf;
    }

    bool MainView::ShowLyrics() const
    {
        return mode_ == MusicMode && s_.music.lyrics && Song() && !Song()->lyrics.empty();
    }

    bool MainView::LoadLyrics(const std::wstring& path, bool quiet)
    {
        if (!Song())
        {
            return false;
        }
        std::string bytes;
        if (!host_.ReadText(path, bytes))
        {
            if (!quiet)
            {
                Warn(L"Не удалось прочитать файл «" + FileNameOf(path) + L"».");
            }
            return false;
        }
        std::vector<LyricLine> lines = ParseLyrics(bytes, Song()->info.duration);
        if (lines.empty())
        {
            if (!quiet)
            {
                Warn(L"В файле «" + FileNameOf(path) + L"» нет строк текста.");
            }
            return false;
        }
        Song()->lyrics = std::move(lines);
        Song()->lyricsName = FileNameOf(path);
        SyncControls();
        UpdatePreview();
        return true;
    }

    void MainView::PickLyrics()
    {
        if (!Song())
        {
            return;
        }
        const std::wstring path = host_.AskOpenLyrics(DirectoryOf(Song()->path));
        if (!path.empty())
        {
            LoadLyrics(path, false);
        }
    }

    MusicPlan MainView::MusicPlanFor(const FrameSettings& frame) const
    {
        const CanvasSize c = Audio().square ? CanvasAt(Audio().canvas) : CanvasSize{SquareSizes[0], SquareSizes[0]};
        return PlanMusic(frame, c.width, c.height, Audio().square, ShowLyrics(), Recorder());
    }

    bool MainView::OpenTrack(const std::wstring& path, std::wstring* failure)
    {
        if (!AudioMode())
        {
            SetMode(MusicMode);
        }
        if (host_.FfmpegPath().empty())
        {
            return Fail(failure, FfmpegMissingText());
        }
        MediaInfo info;
        std::wstring err;
        if (!ProbeAudio(host_, path, info, err))
        {
            return Fail(failure, L"Не удалось открыть песню «" + FileNameOf(path) + L"»:\n" + err);
        }
        if (info.duration <= 0.0)
        {
            return Fail(failure, L"Не удалось узнать длительность песни «" + FileNameOf(path) + L"».");
        }
        StopPlay();
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
        track->reader = std::make_unique<LevelReader>(host_, path);
        SongSlot() = std::move(track);
        for (const wchar_t* ext : {L".lrc", L".LRC", L".Lrc"})
        {
            const std::wstring lrc = JoinPath(DirectoryOf(path), StemOf(FileNameOf(path)) + ext);
            if (mode_ == MusicMode && host_.FileExists(lrc) && LoadLyrics(lrc, true))
            {
                break;
            }
        }
        s_.musicFolder = DirectoryOf(path);
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
        SyncControls();
        UpdatePreview();
        UpdateStatus();
        return true;
    }

    void MainView::EnsureLevels()
    {
        if (!Song() || Song()->levelsReady)
        {
            return;
        }
        Media& m = *Song();
        if (!m.reader)
        {
            m.reader = std::make_unique<LevelReader>(host_, m.path);
        }
        m.reader->Wait();
        TakeLevels(m);
    }

    void MainView::UpdateMusicPreview()
    {
        preview_->message.clear();
        if (!Song())
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
        const FrameVars vars{Song()->name, 0, 0, L""};
        MusicComposer composer(f, vars, plan, Image(), Song()->artist, Song()->title, Song()->cover);
        SetupComposer(composer);
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
        if (!Song())
        {
            first = host_.FfmpegPath().empty() ? L"Для музыки нужна программа ffmpeg" : L"Перетащите песню в окно или нажмите «Открыть...»";
            second.clear();
            third.clear();
            scale_->items[0] = L"Авто";
            return;
        }
        first = Song()->name + L" — " + FormatClock(Song()->info.duration) + (Song()->artist.empty() ? std::wstring() : L", " + Song()->artist);
        const FrameSettings f = MusicFrameSettings();
        const MusicPlan plan = MusicPlanFor(f);
        const std::wstring kind = Audio().format == 3 ? L"PNG " : plan.square ? L"" : L"Окно ";
        second = plan.square ? kind + std::to_wstring(plan.outW) + L"x" + std::to_wstring(plan.outH) + L", окно " + std::to_wstring(plan.scale) + L"x"
                             : kind + std::to_wstring(plan.outW) + L" x " + std::to_wstring(plan.outH) + L", " + std::to_wstring(plan.scale) + L"x";
        third = L"Фрагмент: " + FormatPrecise(TrimEnd() - TrimStart());
        FrameSettings automatic = f;
        automatic.scale = 0;
        scale_->items[0] = L"Авто (сейчас " + std::to_wstring(MusicPlanFor(automatic).scale) + L"x)";
    }

    bool MainView::MusicStill(Image& out)
    {
        if (!Song())
        {
            return false;
        }
        const FrameSettings f = MusicFrameSettings();
        const MusicPlan plan = MusicPlanFor(f);
        const Image background = plan.square ? CanvasBackground(s_.backgroundPicture ? s_.backgroundImage : Image(), s_.background, plan.outW, plan.outH) : Image();
        MusicComposer composer(f, FrameVars{Song()->name, 0, 0, L""}, plan, background, Song()->artist, Song()->title, Song()->cover);
        composer.SetEffects(s_.effects);
        SetupComposer(composer);
        out = composer.Compose(TrackFrame());
        return true;
    }

    void MainView::SaveMusic()
    {
        if (!Song())
        {
            Warn(L"Сначала откройте песню.");
            return;
        }
        StopPlay();
        const std::wstring stem = StemOf(Song()->name);
        const bool png = Audio().format == 3;
        if (!png && host_.FfmpegPath().empty())
        {
            ShowMessage(host_, L"Ретро-рамка", FfmpegMissingText(), MessageKind::Warning);
            return;
        }
        const std::wstring path = png ? host_.AskSavePath(s_.saveFolder, stem + L"_retro.png", ImageFormat::Png) : host_.AskSaveVideo(s_.saveFolder, stem + L"_retro.mp4", false);
        if (path.empty())
        {
            return;
        }
        std::wstring err;
        const ExportResult outcome = ExportTrack(path, L"", err);
        FinishExport(outcome, path, err);
    }

    ExportResult MainView::ExportTrack(const std::wstring& path, const std::wstring& title, std::wstring& err)
    {
        if (Audio().format == 3)
        {
            Image img;
            MusicStill(img);
            return host_.WriteImage(path, img, ImageFormat::Png, 100, err) ? ExportResult::Done : ExportResult::Failed;
        }
        MusicJob job;
        job.source = Song()->path;
        job.output = path;
        job.quality = std::clamp(Audio().format, 0, 2);
        job.start = TrimStart();
        job.duration = TrimEnd() - TrimStart();
        job.fade = Song()->whole ? 0.0 : double(Audio().fade);
        job.fps = double(Audio().fps);
        job.spin = s_.music.spin;
        job.meter = s_.music.meter;
        job.frame = MusicFrameSettings();
        job.vars = FrameVars{Song()->name, 0, 0, L""};
        job.plan = MusicPlanFor(job.frame);
        job.effects = s_.effects;
        job.background =
            job.plan.square ? CanvasBackground(s_.backgroundPicture ? s_.backgroundImage : Image(), s_.background, job.plan.outW, job.plan.outH) : Image();
        job.artist = Song()->artist;
        job.title = Song()->title;
        job.cover = Song()->cover;
        const auto work = [&](const ExportStep& step) {
            if ((Recorder() || s_.music.meter || s_.music.equalizer) && !Song()->levelsReady)
            {
                step(-1, 0);
                EnsureLevels();
            }
            job.levels = Song()->levels;
            job.spectrum = Song()->spectrum;
            job.equalizer = !Recorder() && s_.music.equalizer;
            job.spin = !Recorder() && s_.music.spin;
            job.meter = !Recorder() && s_.music.meter;
            job.recorder = Recorder();
            job.recorderRussian = s_.recorder.russian;
            job.songLength = Song()->info.duration;
            job.samples = Song()->samples;
            if (ShowLyrics())
            {
                job.lyrics = Song()->lyrics;
            }
            return ExportMusic(host_, job, step, err);
        };
        return WithProgress(path, work, title);
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
        if (!Song())
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
        Song()->cover = std::max(img.width, img.height) > SquareSizes[0] ? FitInside(img, SquareSizes[0], SquareSizes[0], false) : img;
        UpdatePreview();
    }

    void MainView::ResetCover()
    {
        if (!Song())
        {
            return;
        }
        Song()->cover = Song()->tagCover;
        UpdatePreview();
    }
}
