#include "mainview.h"

#include "desktop.h"
#include "lyrics.h"
#include "utf.h"

#include <cmath>

namespace rf
{
    namespace
    {
        constexpr int SceneRate = 44100;
        constexpr double SceneFps = 15.0;
        const int HoldChoices[] = {1, 2, 3, 5};
        const int SecondChoices[] = {5, 10, 15, 20, 30, 60};

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

        void Fill(ui::TextField* field, const std::wstring& text)
        {
            if (!field->IsFocused() && field->text != text)
            {
                field->SetText(text);
            }
        }
    }

    void MainView::BuildScenes()
    {
        ui::Root& r = root;
        sceneTextGroup_ = Place<ui::GroupBox>(r, 16, 446, 290, 148);
        sceneTextGroup_->text = L"Текст, который печатается";
        sceneText_ = Place<ui::TextArea>(r, 26, 464, 270, 120);
        sceneText_->onChange = [this](const std::wstring& t) {
            (mode_ == NotepadMode ? s_.notepad.text : s_.setup.files) = t;
            scenePos_ = std::min(scenePos_, SceneLength());
            Changed();
        };
        sceneOnly_ = {sceneTextGroup_, sceneText_};

        const auto row = [&](int y, const std::wstring& text) { return AddLabel(r, 334, y + 4, 90, text); };
        const auto combo = [&](int y, std::vector<std::wstring> items) {
            auto* c = Place<ui::ComboBox>(r, 428, y, 180, 21);
            c->items = std::move(items);
            return c;
        };
        const auto field = [&](int y) { return Place<ui::TextField>(r, 428, y, 180, 21); };

        auto* np = Place<ui::GroupBox>(r, 324, 108, 294, 300);
        np->text = L"Блокнот";
        auto* n1 = row(128, L"Имя файла:");
        npFile_ = field(128);
        npFile_->onChange = [this](const std::wstring& t) {
            s_.notepad.file = t;
            Changed();
        };
        auto* n2 = row(154, L"Язык окна:");
        npLanguage_ = combo(154, {L"Русский: «Блокнот»", L"English: «Notepad»"});
        npLanguage_->onChange = [this](int i) {
            s_.notepad.russian = i == 0;
            Changed();
        };
        auto* n3 = row(180, L"Шрифт:");
        npFont_ = combo(180, {L"как в этой Windows", L"Fixedsys", L"Lucida Console"});
        npFont_->onChange = [this](int i) {
            s_.notepad.font = std::clamp(i, 0, 2);
            Changed();
        };
        auto* n4 = row(206, L"Скорость:");
        npSpeed_ = combo(206, {L"медленно", L"обычно", L"быстро", L"очень быстро"});
        npSpeed_->onChange = [this](int i) {
            s_.notepad.speed = std::clamp(i, 0, NotepadSpeedCount - 1);
            Changed();
        };
        auto* n5 = row(232, L"Окно:");
        npSize_ = combo(232, {L"маленькое", L"среднее", L"большое"});
        npSize_->onChange = [this](int i) {
            s_.notepad.size = std::clamp(i, 0, NotepadSizeCount - 1);
            Changed();
        };
        auto* n6 = row(258, L"Пауза в конце:");
        npHold_ = combo(258, {L"1 секунда", L"2 секунды", L"3 секунды", L"5 секунд"});
        npHold_->onChange = [this](int i) {
            s_.notepad.hold = HoldChoices[std::clamp(i, 0, int(std::size(HoldChoices)) - 1)];
            Changed();
        };
        npTypos_ = AddCheck(r, 334, 288, 270, L"Опечатки и исправления");
        npTypos_->onChange = [this](bool v) {
            s_.notepad.typos = v;
            Changed();
        };
        npCaret_ = AddCheck(r, 334, 308, 270, L"Мигающий курсор");
        npCaret_->onChange = [this](bool v) {
            s_.notepad.caret = v;
            Changed();
        };
        npSound_ = AddCheck(r, 334, 328, 270, L"Звук клавиатуры в MP4");
        npSound_->onChange = [this](bool v) {
            s_.notepad.sound = v;
            Changed();
        };
        auto* nh = AddLabel(r, 334, 354, 274, L"Текст пишите в поле слева внизу или откройте\nфайл .txt кнопкой «Открыть...».");
        nh->rect.h = 28;
        nh->color = Rgb(0x40, 0x40, 0x40);
        notepadOnly_ = {np, n1, npFile_, n2, npLanguage_, n3, npFont_, n4, npSpeed_, n5, npSize_, n6, npHold_, npTypos_, npCaret_, npSound_, nh};

        auto* st = Place<ui::GroupBox>(r, 324, 108, 294, 300);
        st->text = L"Окно с полоской";
        auto* s1 = row(128, L"Что делаем:");
        stKind_ = combo(128, {L"копирование файлов", L"удаление в Корзину", L"установка программы"});
        stKind_->onChange = [this](int i) {
            s_.setup.kind = std::clamp(i, 0, 2);
            Changed();
        };
        auto* s2 = row(154, L"Откуда:");
        stFrom_ = field(154);
        stFrom_->onChange = [this](const std::wstring& t) {
            s_.setup.from = t;
            Changed();
        };
        auto* s3 = row(180, L"Куда:");
        stTo_ = field(180);
        stTo_->onChange = [this](const std::wstring& t) {
            s_.setup.to = t;
            Changed();
        };
        auto* s4 = row(154, L"Программа:");
        stProgram_ = field(154);
        stProgram_->onChange = [this](const std::wstring& t) {
            s_.setup.program = t;
            Changed();
        };
        auto* s5 = row(206, L"Длится:");
        stSeconds_ = combo(206, {L"5 секунд", L"10 секунд", L"15 секунд", L"20 секунд", L"30 секунд", L"1 минуту"});
        stSeconds_->onChange = [this](int i) {
            s_.setup.seconds = SecondChoices[std::clamp(i, 0, int(std::size(SecondChoices)) - 1)];
            scenePos_ = std::min(scenePos_, SceneLength());
            Changed();
        };
        auto* s6 = row(232, L"Полоска:");
        stPace_ = combo(232, {L"ползёт ровно", L"рывками, как обычно", L"зависает на 99%"});
        stPace_->onChange = [this](int i) {
            s_.setup.pace = std::clamp(i, 0, 2);
            Changed();
        };
        auto* s7 = row(258, L"В конце:");
        stEnding_ = combo(258, {L"ничего", L"сообщение «Готово»", L"окно с ошибкой"});
        stEnding_->onChange = [this](int i) {
            s_.setup.ending = std::clamp(i, 0, 2);
            Changed();
        };
        stSound_ = AddCheck(r, 334, 288, 270, L"Звук сообщения в MP4");
        stSound_->onChange = [this](bool v) {
            s_.setup.sound = v;
            Changed();
        };
        auto* sh = AddLabel(r, 334, 314, 274, L"Имена файлов пишите в поле слева внизу,\nпо одному в строке. Они бегут над полоской.");
        sh->rect.h = 28;
        sh->color = Rgb(0x40, 0x40, 0x40);
        setupOnly_ = {st, s1, stKind_, s5, stSeconds_, s6, stPace_, s7, stEnding_, stSound_, sh};
        setupCopy_ = {s2, stFrom_};
        setupTarget_ = {s3, stTo_};
        setupProgram_ = {s4, stProgram_};
    }

    void MainView::SyncSceneControls()
    {
        const bool np = mode_ == NotepadMode;
        const bool setup = mode_ == SetupMode;
        const bool install = s_.setup.kind == int(SetupKind::Install);
        for (ui::Widget* w : sceneOnly_)
        {
            w->visible = np || setup;
        }
        for (ui::Widget* w : notepadOnly_)
        {
            w->visible = np;
        }
        for (ui::Widget* w : setupOnly_)
        {
            w->visible = setup;
        }
        for (ui::Widget* w : setupCopy_)
        {
            w->visible = setup && !install;
        }
        for (ui::Widget* w : setupTarget_)
        {
            w->visible = setup && s_.setup.kind == int(SetupKind::Copy);
        }
        for (ui::Widget* w : setupProgram_)
        {
            w->visible = setup && install;
        }
        if (!np && !setup)
        {
            return;
        }
        sceneTextGroup_->text = np ? L"Текст, который печатается" : L"Имена файлов (по одному в строке)";
        const std::wstring& want = np ? s_.notepad.text : s_.setup.files;
        if (!sceneText_->IsFocused() && sceneText_->text != want)
        {
            sceneText_->SetText(want);
        }
        Fill(npFile_, s_.notepad.file);
        npLanguage_->index = s_.notepad.russian ? 0 : 1;
        npFont_->index = std::clamp(s_.notepad.font, 0, 2);
        npSpeed_->index = std::clamp(s_.notepad.speed, 0, NotepadSpeedCount - 1);
        npSize_->index = std::clamp(s_.notepad.size, 0, NotepadSizeCount - 1);
        npHold_->index = IndexOf(HoldChoices, int(std::size(HoldChoices)), s_.notepad.hold, 1);
        npTypos_->checked = s_.notepad.typos;
        npCaret_->checked = s_.notepad.caret;
        npSound_->checked = s_.notepad.sound;
        stKind_->index = std::clamp(s_.setup.kind, 0, 2);
        Fill(stFrom_, s_.setup.from);
        Fill(stTo_, s_.setup.to);
        Fill(stProgram_, s_.setup.program);
        stSeconds_->index = IndexOf(SecondChoices, int(std::size(SecondChoices)), s_.setup.seconds, 1);
        stPace_->index = std::clamp(s_.setup.pace, 0, 2);
        stEnding_->index = std::clamp(s_.setup.ending, 0, 2);
        stSound_->checked = s_.setup.sound;
        stSound_->enabled = s_.setup.ending > 0;
        const double length = SceneLength();
        scenePos_ = std::clamp(scenePos_, 0.0, length);
        playButton_->enabled = true;
        playButton_->text = Playing() ? L"Стоп" : L"Проиграть";
        soundCheck_->checked = s_.previewSound;
        soundCheck_->enabled = true;
        scrub_->enabled = true;
        scrub_->range = false;
        scrub_->value = length > 0.0 ? scenePos_ / length : 0.0;
        timeLabel_->text = FormatPrecise(scenePos_) + L" / " + FormatPrecise(length);
    }

    NotepadOptions MainView::NotepadOpts() const
    {
        NotepadOptions o;
        o.text = s_.notepad.text;
        o.russian = s_.notepad.russian;
        o.font = s_.notepad.font;
        o.speed = s_.notepad.speed;
        o.typos = s_.notepad.typos;
        o.caret = s_.notepad.caret;
        o.size = s_.notepad.size;
        o.hold = double(s_.notepad.hold);
        return o;
    }

    SetupOptions MainView::SetupOpts() const
    {
        SetupOptions o;
        o.kind = s_.setup.kind;
        o.from = s_.setup.from;
        o.to = s_.setup.to;
        o.program = s_.setup.program;
        o.files = s_.setup.files;
        o.seconds = double(s_.setup.seconds);
        o.pace = s_.setup.pace;
        o.ending = s_.setup.ending;
        return o;
    }

    double MainView::SceneLength() const
    {
        if (mode_ == SetupMode)
        {
            return SetupDuration(SetupOpts());
        }
        const NotepadOptions o = NotepadOpts();
        return NotepadDuration(o, NotepadKeys(o));
    }

    std::wstring MainView::SceneName() const
    {
        if (mode_ == SetupMode)
        {
            return SetupTitle(SetupOpts());
        }
        return NotepadTitle(s_.notepad.file, s_.notepad.russian);
    }

    FrameSettings MainView::SceneFrame() const
    {
        FrameSettings f = LookFrame(mode_, SceneName());
        if (mode_ == NotepadMode)
        {
            f.menuItems = NotepadMenu(s_.notepad.russian, f.style);
        }
        return f;
    }

    ScenePlan MainView::ScenePlanFor(const FrameSettings& frame) const
    {
        int w = 0;
        int h = 0;
        if (mode_ == SetupMode)
        {
            SetupClient(SetupOpts(), &w, &h);
        }
        else
        {
            NotepadClient(NotepadOpts(), &w, &h);
        }
        const SceneOutput& out = SceneOut();
        const CanvasSize c = out.square ? CanvasAt(out.canvas) : CanvasSize{SquareSizes[0], SquareSizes[0]};
        return PlanScene(frame, w, h, c.width, c.height, out.square, Reserve(frame, out.square), out.format == 3 ? s_.video.gifWidth : 0);
    }

    SceneDraw MainView::SceneDrawer(const FrameSettings& frame) const
    {
        if (mode_ == SetupMode)
        {
            const SetupOptions o = SetupOpts();
            return [o, frame](Painter& p, int w, int h, double t) { DrawSetup(p, o, w, h, t, frame); };
        }
        const NotepadOptions o = NotepadOpts();
        auto keys = std::make_shared<const std::vector<Keystroke>>(NotepadKeys(o));
        return [o, keys, frame](Painter& p, int w, int h, double t) { DrawNotepad(p, o, w, h, NotepadTextAt(*keys, t), NotepadCaretAt(o, *keys, t), frame); };
    }

    std::vector<std::int16_t> MainView::SceneSound(double duration) const
    {
        if (mode_ == SetupMode)
        {
            const SetupOptions o = SetupOpts();
            if (!s_.setup.sound || o.ending == 0)
            {
                return {};
            }
            std::vector<std::int16_t> out(std::size_t(duration * SceneRate) + 1, 0);
            AddChime(out, o.seconds + 0.2, o.ending == 2, SceneRate);
            return out;
        }
        if (!s_.notepad.sound)
        {
            return {};
        }
        return KeyboardSound(NotepadKeys(NotepadOpts()), duration, SceneRate);
    }

    void MainView::UpdateScenePreview()
    {
        preview_->message.clear();
        const FrameSettings f = SceneFrame();
        const ScenePlan plan = ScenePlanFor(f);
        const double length = SceneLength();
        const double t = std::clamp(scenePos_, 0.0, length);
        const int k = std::max(1, plan.scale);
        const FrameVars vars{SceneName(), plan.clientW, plan.clientH, L""};
        if (plan.square)
        {
            PreviewBackdrop(plan.outW / k, plan.outH / k, f, vars);
        }
        SceneComposer composer(f, vars, plan, Image(), SceneDrawer(f));
        if (mode_ == SetupMode && s_.setup.ending > 0)
        {
            composer.SetPopup(SetupEndingPopup(SetupOpts()), double(s_.setup.seconds) + 0.2);
        }
        const int seed = int(t * 15.0);
        Spot m;
        if (plan.square)
        {
            m = PreviewMotion(Rect{plan.windowX / k, plan.windowY / k, plan.windowW / k, plan.windowH / k}, plan.outW / k, plan.outH / k, plan.reserve / k, t, seed);
        }
        Rect window;
        Rect content;
        Image img = composer.Small(t, plan.square ? bgPreview_ : Image(), &window, &content, m);
        ApplyEffects(img, f, Stage{window, 1, content, Rect{}, Rect{}, length}, t, seed, length > 0.0 ? t / length : 0.0, m);
        const int availW = preview_->rect.w - 4 - 12;
        const int availH = preview_->rect.h - 4 - 12;
        preview_->image = img.width > availW || img.height > availH ? FitInside(img, availW, availH, false) : std::move(img);
        root.dirty = true;
    }

    void MainView::UpdateSceneStatus(std::wstring& first, std::wstring& second, std::wstring& third)
    {
        const FrameSettings f = SceneFrame();
        const ScenePlan plan = ScenePlanFor(f);
        if (mode_ == NotepadMode)
        {
            first = L"Блокнот: " + std::to_wstring(s_.notepad.text.size()) + L" знаков печатается в окне «" + SceneName() + L"»";
        }
        else
        {
            first = SceneName() + L" — полоска идёт " + std::to_wstring(s_.setup.seconds) + L" сек.";
        }
        const int format = SceneOut().format;
        const std::wstring kind = format == 3 ? L"GIF " : format == 4 ? L"PNG " : L"MP4 ";
        second = kind + std::to_wstring(plan.outW) + L" x " + std::to_wstring(plan.outH) + L", окно " + std::to_wstring(plan.scale) + L"x";
        third = L"Длина: " + FormatPrecise(SceneLength());
        FrameSettings automatic = f;
        automatic.scale = 0;
        scale_->items[0] = L"Авто (сейчас " + std::to_wstring(ScenePlanFor(automatic).scale) + L"x)";
    }

    void MainView::StartScenePlay()
    {
        const double length = SceneLength();
        if (scenePos_ >= length - 0.05)
        {
            scenePos_ = 0.0;
        }
        sceneFrom_ = scenePos_;
        sceneStart_ = host_.Ticks();
        sceneShown_ = 0;
        scenePlaying_ = true;
        sceneAudio_.reset();
        sceneSamples_.clear();
        if (s_.previewSound)
        {
            sceneSamples_ = SceneSound(length);
            if (!sceneSamples_.empty())
            {
                sceneAudio_ = host_.OpenAudio(SceneRate, 1);
                sceneSamplePos_ = std::min(sceneSamples_.size(), std::size_t(sceneFrom_ * SceneRate));
            }
        }
        PollScene(sceneStart_);
        SyncVideoControls();
        UpdatePreview();
    }

    void MainView::PollScene(unsigned long long ms)
    {
        if (!scenePlaying_)
        {
            return;
        }
        const double length = SceneLength();
        const double now = sceneFrom_ + double(ms - sceneStart_) / 1000.0;
        if (sceneAudio_)
        {
            const std::size_t ahead = std::min(sceneSamples_.size(), std::size_t((now + 0.3) * SceneRate));
            while (sceneSamplePos_ < ahead)
            {
                const std::size_t n = std::min<std::size_t>(ahead - sceneSamplePos_, 2048);
                const std::size_t bytes = n * sizeof(std::int16_t);
                if (sceneAudio_->Space() < bytes || !sceneAudio_->Write(sceneSamples_.data() + sceneSamplePos_, bytes))
                {
                    break;
                }
                sceneSamplePos_ += n;
            }
        }
        if (now >= length)
        {
            scenePos_ = length;
            scenePlaying_ = false;
            sceneAudio_.reset();
            SyncVideoControls();
            UpdatePreview();
            UpdateStatus();
            return;
        }
        scenePos_ = now;
        if (sceneShown_ == 0 || ms - sceneShown_ >= (unsigned long long)(1000.0 / SceneFps))
        {
            sceneShown_ = ms;
            SyncVideoControls();
            UpdatePreview();
        }
    }

    bool MainView::SceneStill(Image& out)
    {
        const FrameSettings f = SceneFrame();
        const ScenePlan plan = ScenePlanFor(f);
        const FrameVars vars{SceneName(), plan.clientW, plan.clientH, L""};
        const Image background = plan.square ? Backdrop(plan.outW, plan.outH, plan.scale, f, vars, 0) : Image();
        SceneComposer composer(f, vars, plan, background, SceneDrawer(f));
        const double length = SceneLength();
        composer.SetEffects(s_.effects, length);
        if (mode_ == SetupMode && s_.setup.ending > 0)
        {
            composer.SetPopup(SetupEndingPopup(SetupOpts()), double(s_.setup.seconds) + 0.2);
        }
        const double t = std::clamp(scenePos_, 0.0, length);
        out = composer.Compose(t, int(t * 15.0), false);
        return true;
    }

    ExportResult MainView::ExportSceneFile(const std::wstring& path, const std::wstring& title, std::wstring& err)
    {
        const int format = SceneOut().format;
        if (format == 4)
        {
            Image img;
            SceneStill(img);
            return host_.WriteImage(path, img, ImageFormat::Png, 100, err) ? ExportResult::Done : ExportResult::Failed;
        }
        SceneJob job;
        job.output = path;
        job.gif = format == 3;
        job.quality = std::clamp(format, 0, 2);
        job.fps = job.gif ? double(s_.video.gifFps) : double(SceneOut().fps);
        job.duration = SceneLength();
        job.frame = SceneFrame();
        job.plan = ScenePlanFor(job.frame);
        job.vars = FrameVars{SceneName(), job.plan.clientW, job.plan.clientH, L""};
        job.effects = s_.effects;
        if (job.plan.square)
        {
            const int k = job.plan.scale;
            job.background = Backdrop(job.plan.outW, job.plan.outH, k, job.frame, job.vars, 0);
            if (s_.effects.outro > 0 && !job.gif)
            {
                job.ending = Backdrop(job.plan.outW, job.plan.outH, k, job.frame, job.vars, s_.effects.outro);
                job.taskTarget = TaskTargetFor(job.frame, job.plan.outW, job.plan.outH, k);
            }
        }
        if (mode_ == SetupMode && s_.setup.ending > 0)
        {
            job.popup = SetupEndingPopup(SetupOpts());
            job.popupFrom = double(s_.setup.seconds) + 0.2;
        }
        if (!job.gif)
        {
            const std::vector<std::int16_t> sound = SceneSound(job.duration);
            if (!sound.empty())
            {
                if (voiceTemp_.empty())
                {
                    voiceTemp_ = host_.NewTempFolder();
                }
                const std::wstring wav = voiceTemp_.empty() ? std::wstring() : JoinPath(voiceTemp_, L"scene" + std::to_wstring(++voiceCounter_) + L".wav");
                if (!wav.empty() && host_.WriteBinary(wav, EncodeWav(sound, SceneRate, 16)))
                {
                    job.audio = wav;
                }
            }
        }
        job.draw = SceneDrawer(job.frame);
        const ExportResult result = WithProgress(path, [&](const ExportStep& step) { return ExportScene(host_, job, step, err); }, title);
        if (!job.audio.empty())
        {
            host_.RemoveFile(job.audio);
        }
        return result;
    }

    void MainView::SaveScene()
    {
        StopPlay();
        const int format = SceneOut().format;
        const bool png = format == 4;
        if (!png && host_.FfmpegPath().empty())
        {
            ShowMessage(host_, L"Ретро-рамка", FfmpegMissingText(), MessageKind::Warning);
            return;
        }
        std::wstring stem = mode_ == NotepadMode ? (s_.notepad.file.empty() ? std::wstring(L"блокнот") : StemOf(s_.notepad.file))
                                                 : std::wstring(s_.setup.kind == 2 ? L"установка" : s_.setup.kind == 1 ? L"удаление" : L"копирование");
        const std::wstring suggested = stem + L"_retro" + (png ? L".png" : format == 3 ? L".gif" : L".mp4");
        const std::wstring path = png ? host_.AskSavePath(s_.saveFolder, suggested, ImageFormat::Png) : host_.AskSaveVideo(s_.saveFolder, suggested, format == 3);
        if (path.empty())
        {
            return;
        }
        std::wstring err;
        const ExportResult outcome = ExportSceneFile(path, L"", err);
        FinishExport(outcome, path, err);
    }

    void MainView::CopyScene()
    {
        Image out;
        SceneStill(out);
        if (!host_.SetClipboardImage(out))
        {
            Warn(L"Не удалось положить картинку в буфер обмена.");
            return;
        }
        flash_ = L"Кадр скопирован в буфер обмена";
        flashUntil_ = host_.Ticks() + 3000;
        UpdateStatus();
    }

    bool MainView::LoadSceneText(const std::wstring& path)
    {
        std::string bytes;
        if (!host_.ReadText(path, bytes))
        {
            Warn(L"Не удалось прочитать файл «" + FileNameOf(path) + L"».");
            return false;
        }
        std::wstring text = DecodeText(bytes);
        std::wstring clean;
        for (std::size_t i = 0; i < text.size() && clean.size() < 4000; ++i)
        {
            if (text[i] == L'\r')
            {
                if (i + 1 >= text.size() || text[i + 1] != L'\n')
                {
                    clean.push_back(L'\n');
                }
                continue;
            }
            clean.push_back(text[i]);
        }
        s_.notepad.text = clean;
        s_.notepad.file = StemOf(FileNameOf(path));
        scenePos_ = 0.0;
        if (mode_ != NotepadMode)
        {
            SetMode(NotepadMode);
        }
        Changed();
        return true;
    }

    void MainView::OpenSceneText()
    {
        if (mode_ != NotepadMode)
        {
            root.SetFocus(sceneText_);
            return;
        }
        const std::wstring path = host_.AskOpenText(s_.openFolder);
        if (!path.empty())
        {
            LoadSceneText(path);
        }
    }
}
