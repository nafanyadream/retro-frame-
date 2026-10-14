#pragma once

#include "dialogs.h"
#include "library.h"
#include "media.h"
#include "music.h"
#include "scenes.h"
#include "settings.h"
#include "voice.h"

#include <array>
#include <memory>

namespace rf
{
    class MainView : public View
    {
    public:
        explicit MainView(Host& host);

        void LoadSettings();
        void SaveSettings();
        void AddFiles(const std::vector<std::wstring>& paths);
        void AddImage(Image full, const std::wstring& path, const std::wstring& name, bool keepFull);
        bool OpenVideo(const std::wstring& path, std::wstring* failure = nullptr);
        bool OpenTrack(const std::wstring& path, std::wstring* failure = nullptr);
        void OpenMedia(int mode, const std::vector<std::wstring>& paths);
        int QueueSize() const { return int(queue_[std::size_t(mode_)].size()); }
        int QueuePosition() const { return queuePos_[std::size_t(mode_)]; }
        void SetMode(int mode);
        bool HasClip() const { return clip_ != nullptr; }
        bool HasTrack() const { return track_ != nullptr; }
        bool HasRecording() const { return recording_ != nullptr; }
        bool LevelsReady() const
        {
            const Media* m = mode_ == RecorderMode ? recording_.get() : track_.get();
            return m && m->levelsReady;
        }
        int ShownFormat() const;
        int ShownTab() const { return tabs_->index; }
        double SceneLength() const;
        double ScenePosition() const { return scenePos_; }
        bool Recording() const { return mic_ != nullptr; }
        bool RecordingIsVoice() const { return recording_ && recording_->voice; }
        std::wstring RecordingSound() const { return recording_ ? SoundOf(*recording_) : std::wstring(); }
        double RecordingLength() const { return recording_ ? recording_->info.duration : 0.0; }

        void OnTick(unsigned long long ms) override;
        bool OnCloseRequest() override;
        void OnDropFiles(const std::vector<std::wstring>& paths) override;

        const AppSettings& Settings() const { return s_; }
        AppSettings& MutableSettings() { return s_; }
        void Refresh();

    private:
        struct Photo
        {
            std::wstring path;
            std::wstring name;
            int width = 0;
            int height = 0;
            Image preview;
            Image thumb;
            Image full;
        };

        void Build();
        void BuildMenu();
        void SyncControls();
        void Changed();
        void UpdatePreview();
        void UpdateStatus();
        bool AudioMode() const { return mode_ == MusicMode || mode_ == RecorderMode; }
        bool Recorder() const { return mode_ == RecorderMode; }
        MusicSettings& Audio() { return mode_ == RecorderMode ? s_.recorder : s_.music; }
        const MusicSettings& Audio() const { return mode_ == RecorderMode ? s_.recorder : s_.music; }
        void BuildRecorder();
        void SyncRecorderControls();
        void SetupComposer(MusicComposer& composer) const;
        ModeLook& Look() { return s_.looks[std::size_t(mode_)]; }
        const ModeLook& Look() const { return s_.looks[std::size_t(mode_)]; }
        FrameSettings LookFrame(int mode, const std::wstring& name) const;
        FrameSettings FrameFor(const Photo& photo) const;
        FrameVars VarsFor(const Photo& photo) const;
        bool FullImage(const Photo& photo, Image& out, std::wstring& error);
        bool Compose(const Photo& photo, Image& out, std::wstring& error);
        std::wstring Extension() const;
        ImageFormat Format() const;
        int Quality() const;
        std::wstring UniquePath(const std::wstring& dir, const std::wstring& stem) const;

        void Open();
        struct Media
        {
            std::wstring path;
            std::wstring name;
            MediaInfo info;
            double position = 0.0;
            bool whole = true;
            double start = 0.0;
            double end = 0.0;
            Image frame;
            std::wstring artist;
            std::wstring title;
            Image cover;
            Image tagCover;
            std::vector<float> levels;
            std::vector<float> spectrum;
            std::shared_ptr<const std::vector<std::int16_t>> samples;
            std::unique_ptr<LevelReader> reader;
            bool levelsReady = false;
            std::vector<LyricLine> lyrics;
            std::wstring lyricsName;
            bool voice = false;
            std::shared_ptr<const std::vector<std::int16_t>> raw;
            int rawRate = 0;
            std::wstring sound;
        };

        static const std::wstring& SoundOf(const Media& m) { return m.sound.empty() ? m.path : m.sound; }
        void RefreshMicrophones();
        void StartRecording();
        void StopRecording(bool open);
        void PollRecording();
        void ApplyVoice(Media& m);
        void EditVoice();
        bool IsRecordingFile(const std::wstring& path);
        std::wstring NextRecordingPath();
        std::wstring AudioExtension() const;

        Media* Cur();
        const Media* Cur() const;
        Media* Song() const { return mode_ == RecorderMode ? recording_.get() : track_.get(); }
        std::unique_ptr<Media>& SongSlot() { return mode_ == RecorderMode ? recording_ : track_; }
        void TakeLevels(Media& m);
        bool Playing() const { return play_ != nullptr || musicPlay_ || scenePlaying_; }
        bool SceneMode() const { return mode_ == NotepadMode || mode_ == SetupMode; }
        SceneOutput& SceneOut() { return mode_ == SetupMode ? s_.setup.out : s_.notepad.out; }
        const SceneOutput& SceneOut() const { return mode_ == SetupMode ? s_.setup.out : s_.notepad.out; }
        bool& ResultSquare();
        int& ResultCanvas();
        void SyncResult();
        void BuildScenes();
        void SyncSceneControls();
        void UpdateScenePreview();
        void UpdateSceneStatus(std::wstring& first, std::wstring& second, std::wstring& third);
        FrameSettings SceneFrame() const;
        std::wstring SceneName() const;
        NotepadOptions NotepadOpts() const;
        SetupOptions SetupOpts() const;
        ScenePlan ScenePlanFor(const FrameSettings& frame) const;
        SceneDraw SceneDrawer(const FrameSettings& frame) const;
        std::vector<std::int16_t> SceneSound(double duration) const;
        void StartScenePlay();
        void PollScene(unsigned long long ms);
        bool SceneStill(Image& out);
        void SaveScene();
        void CopyScene();
        void OpenSceneText();
        bool LoadSceneText(const std::wstring& path);
        ExportResult ExportSceneFile(const std::wstring& path, const std::wstring& title, std::wstring& err);
        bool SoundPlaying() const { return sound_ != nullptr && sound_->Active(); }
        void BuildMusic();
        void SyncMusicControls();
        void UpdateMusicPreview();
        void UpdateMusicStatus(std::wstring& first, std::wstring& second, std::wstring& third);
        FrameSettings MusicFrameSettings() const;
        bool& MusicOption(int i);
        void SetMonitor(bool on);
        void FeedMonitor();
        PlayerSkin Skin() const;
        SkinOptions Skins() const;
        MusicPlan MusicPlanFor(const FrameSettings& frame) const;
        bool MusicStill(Image& out);
        void SaveMusic();
        void CopyMusic();
        void LoadCover();
        void ResetCover();
        void EnsureLevels();
        MusicFrame TrackFrame() const;
        bool ShowLyrics() const;
        bool LoadLyrics(const std::wstring& path, bool quiet);
        void PickLyrics();
        ExportResult WithProgress(const std::wstring& path, const std::function<ExportResult(const ExportStep&)>& work, const std::wstring& title = L"");
        ExportResult ExportClip(const std::wstring& path, const std::wstring& title, std::wstring& err);
        ExportResult ExportTrack(const std::wstring& path, const std::wstring& title, std::wstring& err);
        void FinishExport(ExportResult outcome, const std::wstring& path, const std::wstring& error);

        void BuildVideo();
        void SyncVideoControls();
        void UpdateVideoPreview();
        void UpdateVideoStatus(std::wstring& first, std::wstring& second, std::wstring& third);
        FrameSettings VideoFrame() const;
        VideoTarget Target() const;
        double TrimStart() const;
        double TrimEnd() const;
        void SetPosition(double seconds);
        void RequestFrame();
        void PollVideo(unsigned long long ms);
        void TogglePlay();
        void StopPlay();
        void SetTrim(bool start);
        void TrimFieldChanged(bool start, const std::wstring& text);
        void SaveVideo();
        void PickBackground();
        void LoadBackground();
        void VideoChanged();
        void UpdateFormatItems();

        void Paste();
        void SaveCurrent();
        void SaveAll();
        void CopyCurrent();
        void RemoveCurrent();
        void RemoveAll();
        void Select(int index);
        void PickColor(Color FrameColors::*field);
        void LoadCustomIcon();
        void SetCustomIcon(const Image& picked);
        void OpenLibrary();
        void DrawIcon();
        void SyncLibrary();
        void LoadCorner();
        void EditTexts();
        void EditEffects();
        void EditPresets();
        void ApplyPreset(int index);
        bool SavePreset(const std::wstring& name);
        void DeletePreset(int index);
        std::vector<ui::ListItem> PresetItems() const;
        void ApplyEffects(Image& img, const FrameSettings& frame, const Stage& stage, double t, int seed, double progress = -1.0, Spot shift = Spot{}) const;
        Spot PreviewMotion(const Rect& window, int outW, int outH, int reserve, double t, int seed) const;
        Image Backdrop(int w, int h, int k, const FrameSettings& frame, const FrameVars& vars, int ending) const;
        const Image& PreviewBackdrop(int w, int h, const FrameSettings& frame, const FrameVars& vars);
        int Reserve(const FrameSettings& frame, bool square) const;
        Rect TaskTargetFor(const FrameSettings& frame, int outW, int outH, int scale) const;
        void ResetSettings();
        void About();
        void Exit();
        void SetStyle(Style style);
        void SetScheme(int index);
        bool HandleKey(const ui::KeyEvent& e);
        void Warn(const std::wstring& text);
        bool Fail(std::wstring* failure, const std::wstring& text);
        void StepQueue(int delta);
        void SaveQueue();

        Host& host_;
        AppSettings s_;
        std::vector<Photo> photos_;
        std::vector<LibrarySource> library_;
        int librarySource_ = 0;
        int current_ = -1;
        int mode_ = 0;
        int styleValue_ = 1;
        int pasteCounter_ = 0;
        bool dirty_ = false;
        unsigned long long dirtyAt_ = 0;
        std::wstring flash_;
        unsigned long long flashUntil_ = 0;

        ui::MenuBar* menu_ = nullptr;
        ui::TabStrip* tabs_ = nullptr;
        ui::PreviewPane* preview_ = nullptr;
        ui::ThumbList* thumbs_ = nullptr;
        ui::Label* filesLabel_ = nullptr;
        ui::StatusBar* status_ = nullptr;
        ui::TextField* titleField_ = nullptr;
        ui::CheckBox* fromName_ = nullptr;
        ui::ComboBox* kind_ = nullptr;
        ui::CheckBox* active_ = nullptr;
        ui::CheckBox* sunken_ = nullptr;
        std::array<ui::IconCell*, 12> iconCells_{};
        ui::IconCell* customCell_ = nullptr;
        ui::ComboBox* scheme_ = nullptr;
        ui::Swatch* active1_ = nullptr;
        ui::Swatch* active2_ = nullptr;
        ui::Swatch* titleText_ = nullptr;
        ui::Swatch* inactive1_ = nullptr;
        ui::Swatch* inactive2_ = nullptr;
        ui::Swatch* face_ = nullptr;
        ui::CheckBox* menuCheck_ = nullptr;
        ui::CheckBox* statusCheck_ = nullptr;
        ui::CheckBox* addressCheck_ = nullptr;
        ui::CheckBox* toolbarCheck_ = nullptr;
        ui::CheckBox* cornerCheck_ = nullptr;
        ui::CheckBox* gripCheck_ = nullptr;
        ui::ComboBox* scale_ = nullptr;
        ui::ComboBox* format_ = nullptr;
        ui::Button* removeButton_ = nullptr;
        std::vector<ui::Widget*> photoOnly_;
        std::vector<ui::Widget*> videoOnly_;

        std::array<std::vector<std::wstring>, ModeCount> queue_;
        std::array<int, ModeCount> queuePos_{};
        ui::Button* prevButton_ = nullptr;
        ui::Button* nextButton_ = nullptr;
        ui::Label* queueLabel_ = nullptr;
        std::unique_ptr<Media> clip_;
        std::unique_ptr<Media> track_;
        std::unique_ptr<Media> recording_;
        bool musicPlay_ = false;
        unsigned long long musicStart_ = 0;
        double musicFrom_ = 0.0;
        std::vector<ui::Widget*> playRow_;
        std::vector<ui::Widget*> trimOnly_;
        std::vector<ui::Widget*> resultOnly_;
        std::vector<ui::Widget*> sceneOnly_;
        std::vector<ui::Widget*> notepadOnly_;
        std::vector<ui::Widget*> setupOnly_;
        std::vector<ui::Widget*> setupCopy_;
        std::vector<ui::Widget*> setupTarget_;
        std::vector<ui::Widget*> setupProgram_;
        ui::GroupBox* sceneTextGroup_ = nullptr;
        ui::TextArea* sceneText_ = nullptr;
        ui::TextField* npFile_ = nullptr;
        ui::ComboBox* npLanguage_ = nullptr;
        ui::ComboBox* npFont_ = nullptr;
        ui::ComboBox* npSpeed_ = nullptr;
        ui::ComboBox* npSize_ = nullptr;
        ui::ComboBox* npHold_ = nullptr;
        ui::CheckBox* npTypos_ = nullptr;
        ui::CheckBox* npCaret_ = nullptr;
        ui::CheckBox* npSound_ = nullptr;
        ui::ComboBox* stKind_ = nullptr;
        ui::TextField* stFrom_ = nullptr;
        ui::TextField* stTo_ = nullptr;
        ui::TextField* stProgram_ = nullptr;
        ui::ComboBox* stSeconds_ = nullptr;
        ui::ComboBox* stPace_ = nullptr;
        ui::ComboBox* stEnding_ = nullptr;
        ui::CheckBox* stSound_ = nullptr;
        double scenePos_ = 0.0;
        bool scenePlaying_ = false;
        unsigned long long sceneStart_ = 0;
        unsigned long long sceneShown_ = 0;
        double sceneFrom_ = 0.0;
        std::unique_ptr<AudioOut> sceneAudio_;
        std::vector<std::int16_t> sceneSamples_;
        std::size_t sceneSamplePos_ = 0;
        std::vector<ui::Widget*> musicOnly_;
        std::vector<ui::Widget*> recorderOnly_;
        ui::ComboBox* recLanguage_ = nullptr;
        ui::ComboBox* recFade_ = nullptr;
        ui::Label* recInfo_ = nullptr;
        ui::Label* recLevels_ = nullptr;
        ui::ComboBox* recMic_ = nullptr;
        ui::ComboBox* recQuality_ = nullptr;
        ui::Button* recButton_ = nullptr;
        ui::Label* recState_ = nullptr;
        ui::Button* recEffects_ = nullptr;
        ui::Label* recEffectsInfo_ = nullptr;
        std::vector<std::wstring> micNames_;
        std::unique_ptr<AudioIn> mic_;
        std::vector<std::int16_t> micSamples_;
        std::shared_ptr<std::vector<std::int16_t>> micAnalysis_;
        int micRate_ = 0;
        std::wstring micPath_;
        std::wstring voiceTemp_;
        int voiceCounter_ = 0;
        ui::TextField* artistField_ = nullptr;
        ui::TextField* songField_ = nullptr;
        ui::Button* coverLoad_ = nullptr;
        ui::Button* coverReset_ = nullptr;
        ui::ComboBox* playerCombo_ = nullptr;
        ui::CheckBox* recMonitor_ = nullptr;
        std::unique_ptr<VoiceMonitor> monitor_;
        std::unique_ptr<AudioOut> monitorOut_;
        std::size_t monitorFed_ = 0;
        ui::Label* musicHint_ = nullptr;
        ui::CheckBox* musicOption_[3] = {nullptr, nullptr, nullptr};
        ui::CheckBox* lyricsCheck_ = nullptr;
        ui::Button* lyricsLoad_ = nullptr;
        ui::Label* lyricsLabel_ = nullptr;
        ui::ComboBox* fade_ = nullptr;
        ui::Label* levelsLabel_ = nullptr;
        std::unique_ptr<FrameStream> fetch_;
        double fetchAt_ = -1.0;
        bool fetchAgain_ = false;
        std::unique_ptr<FrameStream> play_;
        double playFrom_ = 0.0;
        int playFrames_ = 0;
        unsigned long long playStart_ = 0;
        int resultValue_ = 0;
        ui::Button* playButton_ = nullptr;
        ui::TrackBar* scrub_ = nullptr;
        ui::CheckBox* soundCheck_ = nullptr;
        std::unique_ptr<SoundPreview> sound_;
        ui::Label* timeLabel_ = nullptr;
        ui::CheckBox* wholeCheck_ = nullptr;
        ui::TextField* startField_ = nullptr;
        ui::TextField* endField_ = nullptr;
        ui::Button* startHere_ = nullptr;
        ui::Button* endHere_ = nullptr;
        ui::Label* fragLabel_ = nullptr;
        ui::ComboBox* canvas_ = nullptr;
        ui::Swatch* bgSwatch_ = nullptr;
        ui::CheckBox* bgPicture_ = nullptr;
        ui::Button* bgLoad_ = nullptr;
        ui::ComboBox* fps_ = nullptr;
        ui::ComboBox* gifFps_ = nullptr;
        ui::ComboBox* gifWidth_ = nullptr;
        ui::Button* saveAllButton_ = nullptr;
        ui::Button* effectsButton_ = nullptr;
        ui::Button* copyButton_ = nullptr;
        Image bgPreview_;
        std::wstring bgKey_;
    };
}
