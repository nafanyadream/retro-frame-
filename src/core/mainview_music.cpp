#include "mainview.h"

#include "utf.h"
#include "voice_view.h"

#include <cmath>

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
        auto* l5 = AddLabel(r, 334, 212, 82, L"Плеер:");
        playerCombo_ = Place<ui::ComboBox>(r, 418, 208, 190, 21);
        playerCombo_->items = {L"Как CD-плеер Windows", L"Winamp 2", L"Windows Media Player 7"};
        playerCombo_->onChange = [this](int i) {
            s_.music.player = std::clamp(i, 0, PlayerSkinCount - 1);
            Changed();
        };
        for (int i = 0; i < 3; ++i)
        {
            musicOption_[i] = AddCheck(r, 334, 236 + i * 19, 270, L"");
            musicOption_[i]->onChange = [this, i](bool v) {
                MusicOption(i) = v;
                Changed();
            };
        }
        auto* l4 = AddLabel(r, 334, 300, 140, L"Затухание фрагмента:");
        fade_ = Place<ui::ComboBox>(r, 476, 296, 132, 21);
        fade_->items = {L"выключено", L"1 секунда", L"2 секунды", L"3 секунды", L"5 секунд"};
        fade_->onChange = [this](int i) {
            s_.music.fade = FadeChoices[std::clamp(i, 0, int(std::size(FadeChoices)) - 1)];
            Changed();
        };
        lyricsCheck_ = AddCheck(r, 334, 322, 178, L"Текст песни под плеером");
        lyricsCheck_->onChange = [this](bool v) {
            s_.music.lyrics = v;
            Changed();
        };
        lyricsLoad_ = Place<ui::Button>(r, 514, 318, 94, 21);
        lyricsLoad_->text = L"Файл .lrc...";
        lyricsLoad_->onClick = [this]() { PickLyrics(); };
        lyricsLabel_ = AddLabel(r, 334, 344, 274, L"");
        lyricsLabel_->color = Rgb(0x40, 0x40, 0x40);
        auto* hint = AddLabel(r, 334, 364, 274, L"Затухание работает, когда выбран фрагмент.");
        hint->color = Rgb(0x40, 0x40, 0x40);
        musicHint_ = hint;
        levelsLabel_ = AddLabel(r, 334, 393, 274, L"");
        levelsLabel_->color = Rgb(0x40, 0x40, 0x40);
        auto* resultHint = AddLabel(r, 324, 544, 284, L"MP4 — видео с песней.\nPNG — один кадр на выбранной секунде,\nтот же кадр копирует Ctrl+C.");
        resultHint->rect.h = 40;
        resultHint->color = Rgb(0x40, 0x40, 0x40);
        musicOnly_ = {group, l1, artistField_, l2, songField_, l3, coverLoad_, coverReset_, l5, playerCombo_, musicOption_[0], musicOption_[1], musicOption_[2], l4, fade_, lyricsCheck_, lyricsLoad_,
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
        auto* l3 = AddLabel(r, 334, 190, 80, L"Микрофон:");
        recMic_ = Place<ui::ComboBox>(r, 418, 186, 190, 21);
        recMic_->onChange = [this](int i) {
            s_.micName = i > 0 && i <= int(micNames_.size()) ? micNames_[std::size_t(i - 1)] : std::wstring();
            dirty_ = true;
            dirtyAt_ = host_.Ticks();
        };
        auto* l4 = AddLabel(r, 334, 216, 80, L"Качество:");
        recQuality_ = Place<ui::ComboBox>(r, 418, 212, 190, 21);
        recQuality_->items = {L"Windows 98, 8 бит", L"Обычное, 16 бит"};
        recQuality_->onChange = [this](int i) {
            s_.micQuality = std::clamp(i, 0, 1);
            dirty_ = true;
            dirtyAt_ = host_.Ticks();
        };
        recButton_ = Place<ui::Button>(r, 334, 240, 104, 24);
        recButton_->onClick = [this]() {
            if (mic_)
            {
                StopRecording(true);
            }
            else
            {
                StartRecording();
            }
        };
        recState_ = AddLabel(r, 446, 246, 162, L"");
        recEffects_ = Place<ui::Button>(r, 334, 270, 128, 24);
        recEffects_->text = L"Эффекты записи...";
        recEffects_->onClick = [this]() { EditVoice(); };
        recEffectsInfo_ = AddLabel(r, 470, 276, 138, L"");
        recMonitor_ = AddCheck(r, 334, 300, 274, L"Слышать себя с эффектами (в наушниках)");
        recMonitor_->onChange = [this](bool v) {
            s_.micMonitor = v;
            dirty_ = true;
            dirtyAt_ = host_.Ticks();
            SetMonitor(v && mic_ != nullptr);
        };
        recInfo_ = AddLabel(r, 334, 320, 274, L"");
        recInfo_->rect.h = 28;
        auto* hint = AddLabel(r, 334, 350, 274,
                              L"Записи сохраняются в «Документы» →\n«Ретро-рамка» → «Записи». Эффекты\nнакладываются только на эти записи.");
        hint->rect.h = 42;
        hint->color = Rgb(0x40, 0x40, 0x40);
        recLevels_ = AddLabel(r, 334, 393, 274, L"");
        recLevels_->color = Rgb(0x40, 0x40, 0x40);
        recorderOnly_ = {group, l1, recLanguage_, l2, recFade_, l3, recMic_, l4, recQuality_, recButton_, recState_, recEffects_, recEffectsInfo_, recMonitor_, recInfo_,
                         hint, recLevels_};
        RefreshMicrophones();
    }

    void MainView::RefreshMicrophones()
    {
        micNames_ = host_.Microphones();
        recMic_->items = {L"По умолчанию"};
        recMic_->index = 0;
        for (std::size_t i = 0; i < micNames_.size(); ++i)
        {
            recMic_->items.push_back(micNames_[i]);
            if (micNames_[i] == s_.micName)
            {
                recMic_->index = int(i) + 1;
            }
        }
    }

    void MainView::SyncRecorderControls()
    {
        const Media* m = recording_.get();
        const bool live = mic_ != nullptr;
        recLanguage_->index = s_.recorder.russian ? 1 : 0;
        recFade_->index = FadeIndex(s_.recorder.fade);
        recQuality_->index = std::clamp(s_.micQuality, 0, 1);
        recMonitor_->checked = s_.micMonitor;
        recMic_->enabled = !live;
        recQuality_->enabled = !live;
        static const Image dot = [] {
            Image img(9, 9);
            for (int y = 0; y < 9; ++y)
            {
                for (int x = 0; x < 9; ++x)
                {
                    const double r = std::hypot(x - 4.0, y - 4.0);
                    if (r <= 4.3)
                    {
                        img.pixels[std::size_t(y * 9 + x)] = r > 3.4 ? Rgb(0x80, 0x00, 0x00) : Rgb(0xE0, 0x00, 0x00);
                    }
                }
            }
            return img;
        }();
        static const Image square = [] {
            Image img(9, 9);
            for (int y = 1; y < 8; ++y)
            {
                for (int x = 1; x < 8; ++x)
                {
                    img.pixels[std::size_t(y * 9 + x)] = Rgb(0, 0, 0);
                }
            }
            return img;
        }();
        recButton_->icon = live ? square : dot;
        recButton_->text = live ? L"Стоп" : L"Записать";
        recState_->text = live ? L"Идёт запись: " + FormatPrecise(double(micSamples_.size()) / std::max(1, micRate_)) : std::wstring();
        const bool voiceOk = live || !m || m->voice;
        recEffects_->enabled = voiceOk && !live;
        recEffectsInfo_->text = voiceOk ? VoiceSummary(s_.voice) : std::wstring(L"только для записи с микрофона");
        if (live)
        {
            recInfo_->text = L"Файл: " + FileNameOf(micPath_) + L"\nГоворите: звук идёт в окно «Звукозаписи».";
        }
        else
        {
            recInfo_->text = m ? L"Файл: " + m->name + L"\nДлина: " + FormatClock(m->info.duration) + L" (" + RecorderSeconds(m->info.duration, true) + L")"
                               : L"Нажмите «Записать» и говорите в микрофон\nили откройте готовую песню.";
        }
        recLevels_->text = m && !m->levelsReady && !live ? L"Слушаю песню для волны..." : L"";
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
        const int player = std::clamp(s_.music.player, 0, PlayerSkinCount - 1);
        playerCombo_->index = player;
        static const wchar_t* const labels[PlayerSkinCount][4] = {
            {L"Диск крутится", L"Шкала громкости прыгает под музыку", L"Эквалайзер вместо диска", L"Текст песни под плеером"},
            {L"Окно эквалайзера", L"Окно плейлиста", L"Осциллограф вместо спектра", L"Текст бегущей строкой"},
            {L"Панель слева (Now Playing...)", L"Список песен справа", L"Обложка вместо визуализации", L"Текст поверх картинки"}};
        for (int i = 0; i < 3; ++i)
        {
            musicOption_[i]->text = labels[player][i];
            musicOption_[i]->checked = MusicOption(i);
        }
        lyricsCheck_->text = labels[player][3];
        musicHint_->text = player == 0 ? L"Затухание работает, когда выбран фрагмент." : L"Плеер рисуется как в оригинале: заголовок\nи цвета окна справа к нему не относятся.";
        musicHint_->rect.h = player == 0 ? 13 : 28;
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

    bool& MainView::MusicOption(int i)
    {
        MusicSettings& m = s_.music;
        bool* options[PlayerSkinCount][3] = {{&m.spin, &m.meter, &m.equalizer}, {&m.winampEq, &m.winampList, &m.winampScope}, {&m.wmpTasks, &m.wmpList, &m.wmpCover}};
        return *options[std::clamp(m.player, 0, PlayerSkinCount - 1)][std::clamp(i, 0, 2)];
    }

    PlayerSkin MainView::Skin() const
    {
        return mode_ == MusicMode ? PlayerSkin(std::clamp(s_.music.player, 0, PlayerSkinCount - 1)) : PlayerSkin::Classic;
    }

    SkinOptions MainView::Skins() const
    {
        SkinOptions o;
        o.winampEq = s_.music.winampEq;
        o.winampList = s_.music.winampList;
        o.winampScope = s_.music.winampScope;
        o.wmpTasks = s_.music.wmpTasks;
        o.wmpList = s_.music.wmpList;
        o.wmpCover = s_.music.wmpCover;
        return o;
    }

    FrameSettings MainView::MusicFrameSettings() const
    {
        const std::wstring name = Recorder() && mic_ ? FileNameOf(micPath_) : Song() ? Song()->name : std::wstring(L"песня.mp3");
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
        else if (Skin() != PlayerSkin::Classic)
        {
            f.title = SkinTitle(Skin(), Song() ? Song()->artist : std::wstring(), Song() ? Song()->title : StemOf(name));
            f.icon = SkinIcon(Skin());
        }
        return f;
    }

    void MainView::SetupComposer(MusicComposer& composer) const
    {
        if (Recorder() && Song())
        {
            composer.SetRecorder(s_.recorder.russian, Song()->info.duration, Song()->samples);
        }
        else if (Song())
        {
            composer.SetSamples(Song()->samples);
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
        const bool classic = Skin() == PlayerSkin::Classic;
        MusicFrame mf = MusicFrameAt(Song()->position - start, length, Song()->position, player && classic && s_.music.spin, player && classic && s_.music.meter,
                                     Song()->levels);
        if (player && (!classic || s_.music.equalizer))
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
        MusicShape shape;
        shape.lyrics = ShowLyrics();
        shape.recorder = Recorder();
        shape.reserve = Reserve(frame, Audio().square);
        shape.skin = Skin();
        shape.options = Skins();
        return PlanMusic(frame, c.width, c.height, Audio().square, shape);
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
        std::vector<std::int16_t> raw;
        int rawRate = 0;
        if (mode_ == RecorderMode && IsRecordingFile(path))
        {
            int bits = 0;
            auto src = host_.OpenFile(path);
            if (src && DecodeWav(*src, raw, rawRate, bits) && !raw.empty())
            {
                info.duration = double(raw.size()) / rawRate;
                info.hasAudio = true;
            }
            else
            {
                raw.clear();
            }
        }
        if (raw.empty() && !ProbeAudio(host_, path, info, err))
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
        if (raw.empty() && ReadCover(host_, path, info, SquareSizes[0], cover))
        {
            track->tagCover = cover;
            track->cover = std::move(cover);
        }
        track->voice = !raw.empty();
        if (track->voice)
        {
            track->raw = std::make_shared<const std::vector<std::int16_t>>(std::move(raw));
            track->rawRate = rawRate;
        }
        else
        {
            track->reader = std::make_unique<LevelReader>(host_, path);
        }
        if (SongSlot() && !SongSlot()->sound.empty())
        {
            host_.RemoveFile(SongSlot()->sound);
        }
        SongSlot() = std::move(track);
        if (Song()->voice)
        {
            ApplyVoice(*Song());
        }
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
            m.reader = std::make_unique<LevelReader>(host_, SoundOf(m));
        }
        m.reader->Wait();
        TakeLevels(m);
    }

    void MainView::UpdateMusicPreview()
    {
        preview_->message.clear();
        const bool live = Recorder() && mic_ != nullptr;
        if (!Song() && !live)
        {
            preview_->image = Image();
            preview_->message = host_.FfmpegPath().empty() ? L"Для музыки нужна\nпрограмма ffmpeg.\n\nПоложите ffmpeg.exe\nрядом с RetroFrame.exe."
                                : Recorder()                ? L"Нажмите «Записать»\nи говорите в микрофон.\n\nИли перетащите сюда песню:\nMP3, FLAC, WAV, OGG, M4A."
                                                            : L"Перетащите сюда песню\nили нажмите «Открыть...».\n\nПодходят MP3, FLAC, WAV,\nOGG и M4A.";
            root.dirty = true;
            return;
        }
        const FrameSettings f = MusicFrameSettings();
        const MusicPlan plan = MusicPlanFor(f);
        const FrameVars vars{live ? FileNameOf(micPath_) : Song()->name, 0, 0, L""};
        if (plan.square)
        {
            PreviewBackdrop(plan.outW / plan.scale, plan.outH / plan.scale, f, vars);
        }
        MusicFrame mf;
        Image img;
        Rect inside;
        double duration = 0.0;
        double progress = -1.0;
        const int k = std::max(1, plan.scale);
        const Rect window = plan.square ? Rect{plan.windowX / k, plan.windowY / k, plan.windowW / k, plan.windowH / k} : Rect{};
        Spot m;
        if (live)
        {
            const double length = double(micSamples_.size()) / std::max(1, micRate_);
            MusicComposer composer(f, vars, plan, Image(), L"", L"", Image());
            composer.SetRecorder(s_.recorder.russian, length, micAnalysis_);
            mf.now = length;
            mf.length = length;
            mf.absolute = length;
            mf.wave = std::max(0.0, double(micAnalysis_ ? micAnalysis_->size() : 0) / AnalysisRate - RecorderWaveSeconds / 2.0);
            mf.recording = true;
            img = composer.Small(mf, plan.square ? bgPreview_ : Image(), &inside);
        }
        else
        {
            MusicComposer composer(f, vars, plan, Image(), Song()->artist, Song()->title, Song()->cover);
            SetupComposer(composer);
            mf = TrackFrame();
            duration = TrimEnd() - TrimStart();
            progress = Recorder() ? (Song()->info.duration > 0.0 ? std::clamp(mf.absolute / Song()->info.duration, 0.0, 1.0) : 0.0) : mf.position;
            if (plan.square)
            {
                m = PreviewMotion(window, plan.outW / k, plan.outH / k, plan.reserve / k, mf.now, int(mf.now * 15.0));
            }
            img = composer.Small(mf, plan.square ? bgPreview_ : Image(), &inside, m);
        }
        Stage stage{plan.square ? window : Rect{0, 0, img.width, img.height}, 1, inside, Rect{}, Rect{}, duration};
        MusicHotspots(f, plan, Recorder(), &stage.play, &stage.seek);
        stage.play = stage.play.Offset(inside.x, inside.y);
        stage.seek = stage.seek.Offset(inside.x, inside.y);
        ApplyEffects(img, f, stage, mf.now, int(mf.now * 15.0), progress, m);
        const int availW = preview_->rect.w - 4 - 12;
        const int availH = preview_->rect.h - 4 - 12;
        preview_->image = img.width > availW || img.height > availH ? FitInside(img, availW, availH, false) : std::move(img);
        root.dirty = true;
    }

    void MainView::UpdateMusicStatus(std::wstring& first, std::wstring& second, std::wstring& third)
    {
        if (Recorder() && mic_)
        {
            first = L"Идёт запись: " + FileNameOf(micPath_) + L" — " + FormatPrecise(double(micSamples_.size()) / std::max(1, micRate_));
            second = s_.micQuality == 0 ? L"8 бит, 22 кГц, моно" : L"16 бит, 44 кГц, моно";
            third.clear();
            return;
        }
        if (!Song())
        {
            first = host_.FfmpegPath().empty() ? L"Для музыки нужна программа ffmpeg"
                    : Recorder()                ? L"Нажмите «Записать» или перетащите песню в окно"
                                                : L"Перетащите песню в окно или нажмите «Открыть...»";
            second.clear();
            third.clear();
            scale_->items[0] = L"Авто";
            return;
        }
        first = Song()->name + L" — " + FormatClock(Song()->info.duration) + (Song()->artist.empty() ? std::wstring() : L", " + Song()->artist);
        const FrameSettings f = MusicFrameSettings();
        const MusicPlan plan = MusicPlanFor(f);
        if (Recorder() && Audio().format >= 4)
        {
            second = Audio().format == 5 ? L"Только звук, MP3" : Song()->voice && Song()->rawRate == 22050 ? L"Только звук, WAV 8 бит" : L"Только звук, WAV";
            third = L"Фрагмент: " + FormatPrecise(TrimEnd() - TrimStart());
            return;
        }
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
        const FrameVars vars{Song()->name, 0, 0, L""};
        const Image background = plan.square ? Backdrop(plan.outW, plan.outH, plan.scale, f, vars, 0) : Image();
        MusicComposer composer(f, vars, plan, background, Song()->artist, Song()->title, Song()->cover);
        SetupComposer(composer);
        composer.SetEffects(s_.effects, TrimEnd() - TrimStart());
        const MusicFrame mf = TrackFrame();
        out = composer.Compose(mf, int(mf.now * 15.0));
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
        const int format = Audio().format;
        const bool png = format == 3;
        if (!png && host_.FfmpegPath().empty())
        {
            ShowMessage(host_, L"Ретро-рамка", FfmpegMissingText(), MessageKind::Warning);
            return;
        }
        const std::wstring suggested = stem + L"_retro" + AudioExtension();
        const std::wstring path = png ? host_.AskSavePath(s_.saveFolder, suggested, ImageFormat::Png)
                                  : format >= 4 ? host_.AskSaveSound(s_.saveFolder, suggested, format == 5)
                                                : host_.AskSaveVideo(s_.saveFolder, suggested, false);
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
        if (Recorder() && Audio().format >= 4)
        {
            SoundJob sound;
            sound.source = SoundOf(*Song());
            sound.output = path;
            sound.start = TrimStart();
            sound.duration = TrimEnd() - TrimStart();
            sound.fade = Song()->whole ? 0.0 : double(Audio().fade);
            sound.mp3 = Audio().format == 5;
            sound.retro = Song()->voice && Song()->rawRate == 22050;
            return WithProgress(path, [&](const ExportStep& step) { return ExportSound(host_, sound, step, err); }, title);
        }
        MusicJob job;
        job.source = SoundOf(*Song());
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
        if (job.plan.square)
        {
            const int k = job.plan.scale;
            job.background = Backdrop(job.plan.outW, job.plan.outH, k, job.frame, job.vars, 0);
            if (s_.effects.outro > 0)
            {
                job.ending = Backdrop(job.plan.outW, job.plan.outH, k, job.frame, job.vars, s_.effects.outro);
                job.taskTarget = TaskTargetFor(job.frame, job.plan.outW, job.plan.outH, k);
            }
        }
        job.artist = Song()->artist;
        job.title = Song()->title;
        job.cover = Song()->cover;
        const auto work = [&](const ExportStep& step) {
            const bool classic = Skin() == PlayerSkin::Classic;
            if ((Recorder() || !classic || s_.music.meter || s_.music.equalizer) && !Song()->levelsReady)
            {
                step(-1, 0);
                EnsureLevels();
            }
            job.levels = Song()->levels;
            job.spectrum = Song()->spectrum;
            job.equalizer = !Recorder() && (!classic || s_.music.equalizer);
            job.spin = !Recorder() && classic && s_.music.spin;
            job.meter = !Recorder() && classic && s_.music.meter;
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

    std::wstring MainView::AudioExtension() const
    {
        const int format = Audio().format;
        return format == 3 ? L".png" : format == 4 && Recorder() ? L".wav" : format == 5 && Recorder() ? L".mp3" : L".mp4";
    }

    bool MainView::IsRecordingFile(const std::wstring& path)
    {
        const std::wstring folder = host_.RecordingsFolder();
        if (folder.empty() || path.size() < 4)
        {
            return false;
        }
        const auto strip = [](std::wstring d) {
            while (!d.empty() && (d.back() == L'\\' || d.back() == L'/'))
            {
                d.pop_back();
            }
            return ToLower(d);
        };
        return strip(DirectoryOf(path)) == strip(folder) && ToLower(path.substr(path.size() - 4)) == L".wav";
    }

    std::wstring MainView::NextRecordingPath()
    {
        const std::wstring folder = host_.RecordingsFolder();
        std::wstring path;
        for (int n = 1; n < 100000; ++n)
        {
            path = JoinPath(folder, L"Запись " + std::to_wstring(n) + L".wav");
            if (!host_.FileExists(path))
            {
                break;
            }
        }
        return path;
    }

    void MainView::StartRecording()
    {
        if (mic_ || !Recorder())
        {
            return;
        }
        StopPlay();
        if (host_.RecordingsFolder().empty())
        {
            Warn(L"Не удалось создать папку для записей\n«Документы\\Ретро-рамка\\Записи».");
            return;
        }
        RefreshMicrophones();
        if (micNames_.empty())
        {
            Warn(L"Микрофон не найден.\n\nПодключите микрофон и нажмите «Записать» ещё раз.");
            return;
        }
        micRate_ = s_.micQuality == 0 ? 22050 : 44100;
        mic_ = host_.OpenMicrophone(s_.micName, micRate_);
        if (!mic_)
        {
            Warn(L"Не удалось включить микрофон.\n\nПроверьте, что он подключён, и что в Windows разрешён\nдоступ к микрофону: Параметры → Конфиденциальность → Микрофон.");
            return;
        }
        micPath_ = NextRecordingPath();
        micSamples_.clear();
        micAnalysis_ = std::make_shared<std::vector<std::int16_t>>();
        SetMonitor(s_.micMonitor);
        SyncControls();
        UpdatePreview();
        UpdateStatus();
    }

    void MainView::PollRecording()
    {
        if (!mic_)
        {
            return;
        }
        const bool ok = mic_->Read(micSamples_);
        const std::size_t limit = std::size_t(micRate_) * 600;
        const bool full = micSamples_.size() >= limit;
        if (full)
        {
            micSamples_.resize(limit);
        }
        FeedMonitor();
        const double k = double(micRate_) / AnalysisRate;
        for (;;)
        {
            const double pos = double(micAnalysis_->size()) * k;
            const std::size_t i = std::size_t(pos);
            if (i + 1 >= micSamples_.size())
            {
                break;
            }
            const double a = micSamples_[i];
            const double b = micSamples_[i + 1];
            micAnalysis_->push_back(std::int16_t(std::lround(a + (b - a) * (pos - double(i)))));
        }
        if (!ok)
        {
            StopRecording(true);
            Warn(L"Микрофон перестал отвечать. То, что успело записаться, сохранено.");
            return;
        }
        if (full)
        {
            StopRecording(true);
            Warn(L"Запись остановлена: одна запись может длиться до 10 минут.");
            return;
        }
        SyncRecorderControls();
        UpdatePreview();
        UpdateStatus();
    }

    void MainView::SetMonitor(bool on)
    {
        if (!on)
        {
            monitor_.reset();
            monitorOut_.reset();
            return;
        }
        if (monitor_ || !mic_)
        {
            return;
        }
        monitorOut_ = host_.OpenAudio(micRate_, 1);
        if (!monitorOut_)
        {
            return;
        }
        monitor_ = std::make_unique<VoiceMonitor>(s_.voice, micRate_);
        monitorFed_ = micSamples_.size();
    }

    void MainView::FeedMonitor()
    {
        if (!monitor_ || !monitorOut_)
        {
            return;
        }
        if (micSamples_.size() > monitorFed_)
        {
            monitor_->Push(micSamples_.data() + monitorFed_, micSamples_.size() - monitorFed_);
        }
        monitorFed_ = micSamples_.size();
        const std::vector<std::int16_t> ready = monitor_->Take();
        const std::size_t bytes = ready.size() * sizeof(std::int16_t);
        if (bytes > 0 && monitorOut_->Space() >= bytes)
        {
            monitorOut_->Write(ready.data(), bytes);
        }
    }

    void MainView::StopRecording(bool open)
    {
        if (!mic_)
        {
            return;
        }
        mic_->Read(micSamples_);
        mic_.reset();
        SetMonitor(false);
        const std::vector<std::int16_t> samples = std::move(micSamples_);
        micSamples_.clear();
        micAnalysis_.reset();
        const std::wstring path = micPath_;
        const int rate = micRate_;
        SyncControls();
        UpdatePreview();
        UpdateStatus();
        if (samples.size() < std::size_t(rate / 5))
        {
            if (open)
            {
                Warn(L"Запись получилась слишком короткой. Нажмите «Записать» и говорите.");
            }
            return;
        }
        if (!host_.WriteBinary(path, EncodeWav(samples, rate, rate == 22050 ? 8 : 16)))
        {
            Warn(L"Не удалось сохранить запись «" + FileNameOf(path) + L"».");
            return;
        }
        if (!open)
        {
            return;
        }
        int peak = 0;
        for (std::int16_t v : samples)
        {
            peak = std::max(peak, std::abs(int(v)));
        }
        OpenMedia(RecorderMode, {path});
        if (peak < 200)
        {
            Warn(L"Микрофон записал тишину.\n\nПроверьте, что выбран нужный микрофон и что в Windows разрешён\nдоступ к нему: Параметры → Конфиденциальность → Микрофон.");
            return;
        }
        flash_ = L"Запись сохранена: Документы\\Ретро-рамка\\Записи\\" + FileNameOf(path);
        flashUntil_ = host_.Ticks() + 5000;
        UpdateStatus();
    }

    void MainView::ApplyVoice(Media& m)
    {
        if (!m.voice || !m.raw)
        {
            return;
        }
        const double before = m.info.duration;
        std::vector<std::int16_t> out = ProcessVoice(*m.raw, m.rawRate, s_.voice);
        std::wstring sound;
        if (VoiceEffectCount(s_.voice) > 0)
        {
            if (voiceTemp_.empty())
            {
                voiceTemp_ = host_.NewTempFolder();
            }
            if (!voiceTemp_.empty())
            {
                sound = JoinPath(voiceTemp_, L"voice" + std::to_wstring(++voiceCounter_) + L".wav");
                if (!host_.WriteBinary(sound, EncodeWav(out, m.rawRate, 16)))
                {
                    sound.clear();
                }
            }
            if (sound.empty())
            {
                Warn(L"Не удалось подготовить звук с эффектами: временная папка недоступна.");
                out = *m.raw;
            }
        }
        if (!m.sound.empty())
        {
            host_.RemoveFile(m.sound);
        }
        m.sound = sound;
        m.info.duration = std::max(0.05, double(out.size()) / m.rawRate);
        if (std::fabs(m.info.duration - before) > 0.01)
        {
            m.whole = true;
            m.start = 0.0;
            m.end = m.info.duration;
        }
        m.position = std::clamp(m.position, 0.0, std::max(0.0, m.info.duration - 0.05));
        std::vector<std::int16_t> analysis = ResampleVoice(out, m.rawRate, AnalysisRate);
        m.levels = LevelsFromSamples(analysis, AnalysisRate);
        m.spectrum.clear();
        m.samples = std::make_shared<const std::vector<std::int16_t>>(std::move(analysis));
        m.levelsReady = true;
        m.reader.reset();
    }

    void MainView::EditVoice()
    {
        if (mic_)
        {
            return;
        }
        StopPlay();
        Media* m = recording_.get();
        const bool voice = m && m->voice;
        VoiceView view(host_, s_.voice, s_.voicePresets, voice ? m->raw : nullptr, voice ? m->rawRate : 0);
        const bool ok = host_.RunModal(view) == 1;
        s_.voicePresets = view.UserPresets();
        if (ok)
        {
            s_.voice = view.Chain();
            if (voice)
            {
                ApplyVoice(*recording_);
            }
        }
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
        SyncControls();
        UpdatePreview();
        UpdateStatus();
    }
}
