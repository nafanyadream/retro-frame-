#include "mainview.h"

#include "icons.h"
#include "utf.h"

#include <cmath>

namespace rf
{
    namespace
    {
        constexpr int FrameMaxW = 640;
        constexpr int FrameMaxH = 360;
        constexpr double PreviewFps = 15.0;

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

        void Fit(int w, int h, int maxW, int maxH, int& ow, int& oh)
        {
            const double k = std::min(double(maxW) / std::max(1, w), double(maxH) / std::max(1, h));
            ow = std::max(2, int(std::lround(w * k)));
            oh = std::max(2, int(std::lround(h * k)));
        }

        int IndexOf(const int* values, int count, int value, int fallback)
        {
            for (int i = 0; i < count; ++i)
            {
                if (values[i] == value)
                {
                    return i;
                }
            }
            return fallback;
        }

        void DrawCentered(Image& target, const Image& picture, const Rect& area)
        {
            if (picture.Empty())
            {
                return;
            }
            Painter p(target);
            p.DrawOpaque(picture, area.x + (area.w - picture.width) / 2, area.y + (area.h - picture.height) / 2);
        }
    }

    void MainView::BuildVideo()
    {
        ui::Root& r = root;
        playButton_ = Place<ui::Button>(r, 16, 414, 92, 24);
        playButton_->text = L"Проиграть";
        playButton_->onClick = [this]() { TogglePlay(); };
        scrub_ = Place<ui::TrackBar>(r, 112, 414, 404, 24);
        scrub_->onChange = [this](double v) {
            if (Media* m = Cur())
            {
                StopPlay();
                SetPosition(v * m->info.duration);
            }
        };
        timeLabel_ = AddLabel(r, 520, 420, 98, L"");
        timeLabel_->align = ui::Align::Right;

        auto* trim = Place<ui::GroupBox>(r, 16, 446, 290, 148);
        trim->text = L"Обрезка";
        wholeCheck_ = AddCheck(r, 26, 463, 260, L"Весь ролик целиком");
        wholeCheck_->onChange = [this](bool v) {
            Media* m = Cur();
            if (!m)
            {
                return;
            }
            m->whole = v;
            if (!v && m->end - m->start < 0.1)
            {
                m->start = 0.0;
                m->end = m->info.duration;
            }
            Changed();
        };
        auto* l1 = AddLabel(r, 26, 490, 52, L"Начало:");
        startField_ = Place<ui::TextField>(r, 80, 486, 80, 21);
        startField_->onChange = [this](const std::wstring& t) { TrimFieldChanged(true, t); };
        startHere_ = Place<ui::Button>(r, 166, 486, 130, 21);
        startHere_->text = L"Взять с ползунка";
        startHere_->onClick = [this]() { SetTrim(true); };
        auto* l2 = AddLabel(r, 26, 516, 52, L"Конец:");
        endField_ = Place<ui::TextField>(r, 80, 512, 80, 21);
        endField_->onChange = [this](const std::wstring& t) { TrimFieldChanged(false, t); };
        endHere_ = Place<ui::Button>(r, 166, 512, 130, 21);
        endHere_->text = L"Взять с ползунка";
        endHere_->onClick = [this]() { SetTrim(false); };
        fragLabel_ = AddLabel(r, 26, 542, 270, L"");
        auto* hint1 = AddLabel(r, 26, 562, 270, L"Время пишется так: минуты:секунды.десятые");
        hint1->color = Rgb(0x40, 0x40, 0x40);

        auto* resultBox = Place<ui::GroupBox>(r, 314, 446, 304, 148);
        resultBox->text = L"Результат";
        const auto radio = [&](int x, int w, const std::wstring& text, int value) {
            auto* rb = Place<ui::RadioButton>(r, x, 463, w, 16);
            rb->text = text;
            rb->value = value;
            rb->group = &resultValue_;
            rb->onChange = [this](int v) {
                (mode_ == MusicMode ? s_.music.square : s_.video.square) = v == 1;
                Changed();
            };
            return rb;
        };
        auto* r1 = radio(324, 104, L"Только окно", 0);
        auto* r2 = radio(430, 80, L"Квадрат", 1);
        squareSize_ = Place<ui::ComboBox>(r, 516, 460, 92, 21);
        squareSize_->items = {L"1080 x 1080", L"720 x 720"};
        squareSize_->onChange = [this](int i) {
            (mode_ == MusicMode ? s_.music.squareSize : s_.video.squareSize) = SquareSizes[std::clamp(i, 0, 1)];
            Changed();
        };
        auto* l3 = AddLabel(r, 324, 490, 82, L"Фон квадрата:");
        bgSwatch_ = Place<ui::Swatch>(r, 408, 486, 24, 19);
        bgSwatch_->onClick = [this]() { PickBackground(); };
        bgPicture_ = AddCheck(r, 438, 488, 76, L"картинка");
        bgPicture_->onChange = [this](bool v) {
            if (v && s_.backgroundImage.Empty())
            {
                LoadBackground();
                SyncControls();
                return;
            }
            s_.backgroundPicture = v;
            Changed();
        };
        bgLoad_ = Place<ui::Button>(r, 516, 486, 92, 21);
        bgLoad_->text = L"Выбрать...";
        bgLoad_->onClick = [this]() { LoadBackground(); };
        auto* l4 = AddLabel(r, 324, 518, 54, L"Для GIF:");
        gifFps_ = Place<ui::ComboBox>(r, 380, 514, 76, 21);
        for (int fps : GifFpsChoices)
        {
            gifFps_->items.push_back(std::to_wstring(fps) + L" к/с");
        }
        gifFps_->onChange = [this](int i) {
            s_.video.gifFps = GifFpsChoices[std::clamp(i, 0, int(std::size(GifFpsChoices)) - 1)];
            Changed();
        };
        gifWidth_ = Place<ui::ComboBox>(r, 462, 514, 146, 21);
        for (int w : GifWidthChoices)
        {
            gifWidth_->items.push_back(w == 0 ? std::wstring(L"ширина как есть") : L"ширина " + std::to_wstring(w));
        }
        gifWidth_->onChange = [this](int i) {
            s_.video.gifWidth = GifWidthChoices[std::clamp(i, 0, int(std::size(GifWidthChoices)) - 1)];
            Changed();
        };
        auto* hint2 = AddLabel(r, 324, 544, 284, L"MP4 сохраняет звук ролика.\nGIF без звука и повторяется по кругу.");
        hint2->rect.h = 26;
        hint2->color = Rgb(0x40, 0x40, 0x40);

        mediaOnly_ = {playButton_, scrub_, timeLabel_, trim, wholeCheck_, l1, startField_, startHere_, l2, endField_, endHere_, fragLabel_, hint1,
                      resultBox, r1, r2, squareSize_, l3, bgSwatch_, bgPicture_, bgLoad_};
        videoOnly_ = {l4, gifFps_, gifWidth_, hint2};
    }

    void MainView::UpdateFormatItems()
    {
        if (mode_ == VideoMode)
        {
            format_->items = {L"MP4, высокое качество", L"MP4, среднее качество", L"MP4, низкое качество", L"GIF без звука"};
            format_->index = std::clamp(s_.video.format, 0, 3);
        }
        else if (mode_ == MusicMode)
        {
            format_->items = {L"MP4, высокое качество", L"MP4, среднее качество", L"MP4, низкое качество", L"PNG, один кадр"};
            format_->index = std::clamp(s_.music.format, 0, 3);
        }
        else
        {
            format_->items = {L"PNG", L"JPG, высокое качество", L"JPG, среднее качество"};
            format_->index = std::clamp(s_.format, 0, 2);
        }
    }

    void MainView::SetMode(int mode)
    {
        mode = std::clamp(mode, 0, 2);
        tabs_->index = mode;
        if (mode == mode_)
        {
            return;
        }
        StopPlay();
        mode_ = mode;
        s_.mode = mode;
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
        preview_->rect.w = mode_ == MusicMode ? 300 : 602;
        preview_->rect.h = mode_ == PhotoMode ? 350 : 296;
        UpdateFormatItems();
        SyncControls();
        UpdatePreview();
        UpdateStatus();
    }

    MainView::Media* MainView::Cur()
    {
        return mode_ == VideoMode ? clip_.get() : mode_ == MusicMode ? track_.get() : nullptr;
    }

    const MainView::Media* MainView::Cur() const
    {
        return mode_ == VideoMode ? clip_.get() : mode_ == MusicMode ? track_.get() : nullptr;
    }

    void MainView::SyncVideoControls()
    {
        const bool media = mode_ == VideoMode || mode_ == MusicMode;
        for (ui::Widget* w : mediaOnly_)
        {
            w->visible = media;
        }
        for (ui::Widget* w : videoOnly_)
        {
            w->visible = mode_ == VideoMode;
        }
        for (ui::Widget* w : musicOnly_)
        {
            w->visible = mode_ == MusicMode;
        }
        if (!media)
        {
            return;
        }
        const Media* m = Cur();
        const bool has = m != nullptr;
        const double duration = has ? m->info.duration : 0.0;
        playButton_->enabled = has;
        playButton_->text = Playing() ? L"Стоп" : L"Проиграть";
        scrub_->enabled = has;
        scrub_->value = has && duration > 0.0 ? m->position / duration : 0.0;
        scrub_->range = has && !m->whole;
        if (has && duration > 0.0)
        {
            scrub_->rangeStart = TrimStart() / duration;
            scrub_->rangeEnd = TrimEnd() / duration;
        }
        timeLabel_->text = has ? FormatPrecise(m->position) + L" / " + FormatPrecise(duration) : std::wstring();
        wholeCheck_->text = mode_ == MusicMode ? L"Вся песня целиком" : L"Весь ролик целиком";
        wholeCheck_->enabled = has;
        wholeCheck_->checked = !has || m->whole;
        const bool parts = has && !m->whole;
        for (ui::Widget* w : {static_cast<ui::Widget*>(startField_), static_cast<ui::Widget*>(endField_), static_cast<ui::Widget*>(startHere_),
                              static_cast<ui::Widget*>(endHere_)})
        {
            w->enabled = parts;
        }
        const auto fill = [&](ui::TextField* field, double value) {
            const std::wstring text = has ? FormatPrecise(value) : std::wstring();
            if (!field->IsFocused() && field->text != text)
            {
                field->SetText(text);
            }
        };
        fill(startField_, TrimStart());
        fill(endField_, TrimEnd());
        fragLabel_->text = has ? L"Длина фрагмента: " + FormatPrecise(TrimEnd() - TrimStart()) : std::wstring();
        const bool square = mode_ == MusicMode ? s_.music.square : s_.video.square;
        resultValue_ = square ? 1 : 0;
        squareSize_->index = IndexOf(SquareSizes, int(std::size(SquareSizes)), mode_ == MusicMode ? s_.music.squareSize : s_.video.squareSize, 0);
        squareSize_->enabled = square;
        bgSwatch_->color = s_.background;
        bgSwatch_->enabled = square;
        bgPicture_->checked = s_.backgroundPicture && !s_.backgroundImage.Empty();
        bgPicture_->enabled = square;
        bgLoad_->enabled = square;
        if (mode_ == MusicMode)
        {
            SyncMusicControls();
            return;
        }
        const bool gif = s_.video.format == 3;
        gifFps_->index = IndexOf(GifFpsChoices, int(std::size(GifFpsChoices)), s_.video.gifFps, 2);
        gifFps_->enabled = gif;
        gifWidth_->index = IndexOf(GifWidthChoices, int(std::size(GifWidthChoices)), s_.video.gifWidth, 0);
        gifWidth_->enabled = gif;
    }

    FrameSettings MainView::VideoFrame() const
    {
        FrameSettings f = LookFrame(VideoMode, clip_ ? clip_->name : std::wstring(L"ролик.mp4"));
        f.menuItems = s_.playerMenu;
        f.statusFields = {s_.playerStatus, s_.playerTime ? std::wstring(L"{время}") : std::wstring()};
        f.statusIcon = false;
        f.player = true;
        return f;
    }

    VideoTarget MainView::Target() const
    {
        VideoTarget t;
        t.square = s_.video.square;
        t.squareSize = s_.video.squareSize;
        t.width = s_.video.format == 3 ? s_.video.gifWidth : 0;
        return t;
    }

    double MainView::TrimStart() const
    {
        const Media* m = Cur();
        return m && !m->whole ? m->start : 0.0;
    }

    double MainView::TrimEnd() const
    {
        const Media* m = Cur();
        return m && !m->whole ? m->end : (m ? m->info.duration : 0.0);
    }

    void MainView::OpenVideo(const std::wstring& path)
    {
        SetMode(VideoMode);
        if (host_.FfmpegPath().empty())
        {
            ShowMessage(host_, L"Ретро-рамка", FfmpegMissingText(), MessageKind::Warning);
            return;
        }
        MediaInfo info;
        std::wstring err;
        if (!ProbeMedia(host_, path, info, err))
        {
            Warn(L"Не удалось открыть видео «" + FileNameOf(path) + L"»:\n" + err);
            return;
        }
        if (info.duration <= 0.0)
        {
            Warn(L"Не удалось узнать длительность видео «" + FileNameOf(path) + L"».");
            return;
        }
        StopPlay();
        fetch_.reset();
        auto clip = std::make_unique<Media>();
        clip->path = path;
        clip->name = FileNameOf(path);
        clip->info = info;
        clip->end = info.duration;
        clip->position = std::min(1.0, info.duration * 0.1);
        clip_ = std::move(clip);
        s_.videoFolder = DirectoryOf(path);
        dirty_ = true;
        dirtyAt_ = host_.Ticks();
        RequestFrame();
        SyncControls();
        UpdatePreview();
        UpdateStatus();
    }

    void MainView::SetPosition(double seconds)
    {
        Media* m = Cur();
        if (!m)
        {
            return;
        }
        m->position = std::clamp(seconds, 0.0, std::max(0.0, m->info.duration - 0.05));
        if (mode_ == VideoMode)
        {
            RequestFrame();
        }
        SyncVideoControls();
        UpdatePreview();
    }

    void MainView::RequestFrame()
    {
        if (!clip_)
        {
            return;
        }
        if (fetch_)
        {
            fetchAgain_ = true;
            return;
        }
        int w = 0;
        int h = 0;
        Fit(clip_->info.width, clip_->info.height, FrameMaxW, FrameMaxH, w, h);
        fetch_ = std::make_unique<FrameStream>(host_, clip_->path, clip_->position, 0.0, 0.0, w, h);
        fetchAt_ = clip_->position;
        fetchAgain_ = false;
        if (!fetch_->Started())
        {
            fetch_.reset();
        }
    }

    void MainView::PollVideo(unsigned long long ms)
    {
        if (levels_ && track_ && levels_->Poll())
        {
            track_->levels = levels_->Levels();
            track_->levelsReady = true;
            levels_.reset();
            if (mode_ == MusicMode)
            {
                SyncVideoControls();
                UpdatePreview();
            }
        }
        if (musicPlay_ && track_)
        {
            const double now = musicFrom_ + double(ms - musicStart_) / 1000.0;
            if (now >= TrimEnd() - 0.01)
            {
                track_->position = std::max(0.0, TrimEnd() - 0.05);
                musicPlay_ = false;
            }
            else
            {
                track_->position = now;
            }
            if (mode_ == MusicMode)
            {
                SyncVideoControls();
                UpdatePreview();
            }
        }
        if (fetch_ && clip_)
        {
            Image img;
            if (fetch_->Poll(img))
            {
                clip_->frame = std::move(img);
                fetch_.reset();
                UpdatePreview();
            }
            else if (fetch_->Ended())
            {
                fetch_.reset();
            }
            if (!fetch_ && fetchAgain_)
            {
                fetchAgain_ = false;
                if (clip_->position != fetchAt_)
                {
                    RequestFrame();
                }
            }
        }
        if (play_ && clip_)
        {
            Image img;
            bool shown = false;
            if (playFrames_ == 0)
            {
                if (play_->Poll(img))
                {
                    playStart_ = ms;
                    playFrames_ = 1;
                    shown = true;
                }
            }
            while (playFrames_ > 0 && double(ms - playStart_) / 1000.0 >= playFrames_ / PreviewFps && play_->Poll(img))
            {
                ++playFrames_;
                shown = true;
            }
            if (shown)
            {
                clip_->frame = std::move(img);
                clip_->position = std::min(TrimEnd(), playFrom_ + (playFrames_ - 1) / PreviewFps);
                SyncVideoControls();
                UpdatePreview();
            }
            if (play_->Ended())
            {
                StopPlay();
            }
        }
    }

    void MainView::TogglePlay()
    {
        if (Playing())
        {
            StopPlay();
            return;
        }
        if (mode_ == MusicMode && track_)
        {
            double from = track_->position;
            if (from < TrimStart() || from >= TrimEnd() - 0.1)
            {
                from = TrimStart();
            }
            track_->position = from;
            musicFrom_ = from;
            musicStart_ = host_.Ticks();
            musicPlay_ = true;
            SyncVideoControls();
            UpdatePreview();
            return;
        }
        if (!clip_ || mode_ != VideoMode)
        {
            return;
        }
        double from = clip_->position;
        if (from < TrimStart() || from >= TrimEnd() - 0.1)
        {
            from = TrimStart();
        }
        fetch_.reset();
        fetchAgain_ = false;
        int w = 0;
        int h = 0;
        Fit(clip_->info.width, clip_->info.height, FrameMaxW, FrameMaxH, w, h);
        play_ = std::make_unique<FrameStream>(host_, clip_->path, from, TrimEnd() - from, PreviewFps, w, h);
        if (!play_->Started())
        {
            play_.reset();
            return;
        }
        playFrom_ = from;
        playFrames_ = 0;
        clip_->position = from;
        SyncVideoControls();
        UpdatePreview();
    }

    void MainView::StopPlay()
    {
        if (!Playing())
        {
            return;
        }
        play_.reset();
        musicPlay_ = false;
        SyncVideoControls();
        root.dirty = true;
    }

    void MainView::SetTrim(bool start)
    {
        Media* m = Cur();
        if (!m)
        {
            return;
        }
        const double d = m->info.duration;
        const double p = m->position;
        if (start)
        {
            m->start = std::clamp(std::min(p, m->end - 0.1), 0.0, d);
        }
        else
        {
            m->end = std::clamp(std::max(p, m->start + 0.1), 0.0, d);
        }
        Changed();
    }

    void MainView::TrimFieldChanged(bool start, const std::wstring& text)
    {
        Media* m = Cur();
        if (!m || m->whole)
        {
            return;
        }
        double v = 0.0;
        if (!ParseClock(text, v))
        {
            return;
        }
        v = std::clamp(v, 0.0, m->info.duration);
        if (start && v < m->end - 0.05)
        {
            m->start = v;
        }
        else if (!start && v > m->start + 0.05)
        {
            m->end = v;
        }
        else
        {
            return;
        }
        SyncVideoControls();
        UpdatePreview();
        UpdateStatus();
    }

    void MainView::PickBackground()
    {
        Color c = s_.background;
        if (!host_.AskColor(c, c))
        {
            return;
        }
        s_.background = c;
        s_.backgroundPicture = false;
        Changed();
    }

    void MainView::LoadBackground()
    {
        const auto paths = host_.AskOpenImages(s_.openFolder);
        if (paths.empty())
        {
            return;
        }
        Image img;
        std::wstring err;
        if (!host_.ReadImage(paths.front(), img, err))
        {
            Warn(L"Не удалось открыть картинку для фона:\n" + err);
            return;
        }
        s_.backgroundImage = SquareBackground(img, s_.background, SquareSizes[0]);
        s_.backgroundPicture = true;
        bgKey_.clear();
        const std::wstring dir = host_.SettingsFolder();
        if (!dir.empty())
        {
            host_.WriteImage(JoinPath(dir, L"background.png"), s_.backgroundImage, ImageFormat::Png, 100, err);
        }
        Changed();
    }

    void MainView::UpdateVideoPreview()
    {
        preview_->message.clear();
        if (!clip_)
        {
            preview_->image = Image();
            preview_->message = host_.FfmpegPath().empty()
                                    ? FfmpegMissingText()
                                    : L"Перетащите сюда видео или нажмите «Открыть...».\n\nПодходят MP4, MOV, AVI, MKV, WebM и анимированный GIF.";
            root.dirty = true;
            return;
        }
        FrameSettings f = VideoFrame();
        const double start = TrimStart();
        const double length = std::max(0.001, TrimEnd() - start);
        const double rel = std::clamp(clip_->position - start, 0.0, length);
        f.playerPosition = rel / length;
        const FrameVars vars{clip_->name, clip_->info.width, clip_->info.height, FormatClock(rel) + L" / " + FormatClock(length)};
        const int availW = preview_->rect.w - 4 - 12;
        const int availH = preview_->rect.h - 4 - 12;
        const MediaInfo& info = clip_->info;
        if (s_.video.square)
        {
            const VideoPlan plan = PlanVideo(f, info.width, info.height, Target());
            const int k = plan.scale;
            const int side = plan.outW / k;
            static const Image none;
            const Image& picture = s_.backgroundPicture ? s_.backgroundImage : none;
            const std::wstring key = std::to_wstring(side) + L"/" + std::to_wstring(s_.background) + L"/" + std::to_wstring(picture.width);
            if (key != bgKey_)
            {
                bgPreview_ = SquareBackground(picture, s_.background, side);
                bgKey_ = key;
            }
            Image square = bgPreview_;
            Rect client;
            Image window = RenderFrame(f, plan.clientW, plan.clientH, vars, &client);
            if (!clip_->frame.Empty())
            {
                DrawCentered(window, ResizeSmooth(clip_->frame, std::max(2, plan.videoW / k), std::max(2, plan.videoH / k)), client);
            }
            Painter(square).DrawOpaque(window, plan.windowX / k, plan.windowY / k);
            preview_->image = side > availW || side > availH ? FitInside(square, availW, availH, false) : square;
        }
        else
        {
            const FrameGeometry g = MeasureFrame(f, 100, 100);
            const int maxW = std::max(PlayerMinWidth, availW - (g.width - 100));
            const int maxH = std::max(40, availH - (g.height - 100));
            int vw = 0;
            int vh = 0;
            Fit(info.width, info.height, maxW, maxH, vw, vh);
            Rect client;
            Image window = RenderFrame(f, std::max(vw, PlayerMinWidth), vh, vars, &client);
            if (!clip_->frame.Empty())
            {
                DrawCentered(window, ResizeSmooth(clip_->frame, vw, vh), client);
            }
            preview_->image = std::move(window);
        }
        root.dirty = true;
    }

    void MainView::UpdateVideoStatus(std::wstring& first, std::wstring& second, std::wstring& third)
    {
        if (!clip_)
        {
            first = host_.FfmpegPath().empty() ? L"Для видео нужна программа ffmpeg" : L"Перетащите видео в окно или нажмите «Открыть...»";
            second.clear();
            third.clear();
            scale_->items[0] = L"Авто";
            return;
        }
        const MediaInfo& info = clip_->info;
        first = clip_->name + L" — " + std::to_wstring(info.width) + L" x " + std::to_wstring(info.height) + L", " + FormatClock(info.duration) +
                (info.hasAudio ? L"" : L", без звука");
        const VideoPlan plan = PlanVideo(VideoFrame(), info.width, info.height, Target());
        second = s_.video.square ? L"Квадрат " + std::to_wstring(plan.outW) + L", окно " + std::to_wstring(plan.scale) + L"x"
                                 : L"Итог " + std::to_wstring(plan.outW) + L" x " + std::to_wstring(plan.outH);
        third = L"Фрагмент: " + FormatPrecise(TrimEnd() - TrimStart());
        FrameSettings automatic = VideoFrame();
        automatic.scale = 0;
        scale_->items[0] = L"Авто (сейчас " + std::to_wstring(PlanVideo(automatic, info.width, info.height, Target()).scale) + L"x)";
    }

    void MainView::SaveVideo()
    {
        if (!clip_)
        {
            Warn(L"Сначала откройте видео.");
            return;
        }
        if (host_.FfmpegPath().empty())
        {
            ShowMessage(host_, L"Ретро-рамка", FfmpegMissingText(), MessageKind::Warning);
            return;
        }
        StopPlay();
        const bool gif = s_.video.format == 3;
        const std::wstring suggested = StemOf(clip_->name) + L"_retro" + (gif ? L".gif" : L".mp4");
        const std::wstring path = host_.AskSaveVideo(s_.saveFolder, suggested, gif);
        if (path.empty())
        {
            return;
        }
        const MediaInfo& info = clip_->info;
        ExportJob job;
        job.source = clip_->path;
        job.output = path;
        job.gif = gif;
        job.quality = std::clamp(s_.video.format, 0, 2);
        job.start = TrimStart();
        job.duration = TrimEnd() - TrimStart();
        job.fps = gif ? double(s_.video.gifFps) : info.fps;
        job.audio = !gif && info.hasAudio;
        job.frame = VideoFrame();
        job.vars = FrameVars{clip_->name, info.width, info.height, L""};
        job.plan = PlanVideo(job.frame, info.width, info.height, Target());
        if (job.plan.square)
        {
            job.background = SquareBackground(s_.backgroundPicture ? s_.backgroundImage : Image(), s_.background, job.plan.outW);
        }
        std::wstring err;
        const ExportResult outcome = WithProgress(path, [&](const ExportStep& step) { return ExportVideo(host_, job, step, err); });
        FinishExport(outcome, path, err);
    }

    ExportResult MainView::WithProgress(const std::wstring& path, const std::function<ExportResult(const ExportStep&)>& work)
    {
        ProgressView progress(mode_ == MusicMode ? L"Сохранение песни" : L"Сохранение видео");
        const std::wstring line1 = L"Сохраняется: " + FileNameOf(path);
        progress.Update(line1, L"Подготовка...", 0.0);
        void* handle = host_.OpenModeless(progress);
        host_.Pump();
        const unsigned long long began = host_.Ticks();
        unsigned long long last = 0;
        const ExportResult outcome = work([&](int i, int total) {
            const unsigned long long now = host_.Ticks();
            if (now - last >= 100 || i <= 0)
            {
                last = now;
                std::wstring line2;
                if (i < 0)
                {
                    line2 = L"Подготовка: считаю громкость песни...";
                }
                else if (i >= total)
                {
                    line2 = L"Завершение записи...";
                }
                else
                {
                    line2 = L"Кадр " + std::to_wstring(i + 1) + L" из " + std::to_wstring(total);
                    if (i >= 10)
                    {
                        const double left = double(now - began) / 1000.0 * double(total - i) / double(i);
                        line2 += L", осталось примерно " + FormatClock(left + 0.999);
                    }
                }
                progress.Update(line1, line2, i < 0 ? 0.0 : double(i) / double(std::max(1, total)));
                host_.Pump();
            }
            return !progress.cancelled;
        });
        host_.CloseModeless(handle);
        return outcome;
    }

    void MainView::FinishExport(ExportResult outcome, const std::wstring& path, const std::wstring& error)
    {
        if (outcome == ExportResult::Done)
        {
            s_.saveFolder = DirectoryOf(path);
            dirty_ = true;
            dirtyAt_ = host_.Ticks();
            flash_ = L"Сохранено: " + FileNameOf(path);
        }
        else if (outcome == ExportResult::Cancelled)
        {
            flash_ = L"Сохранение отменено";
        }
        else
        {
            Warn(L"Не удалось сохранить:\n" + error);
        }
        flashUntil_ = host_.Ticks() + 4000;
        UpdateStatus();
    }
}
