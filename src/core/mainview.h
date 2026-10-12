#pragma once

#include "dialogs.h"
#include "library.h"
#include "media.h"
#include "music.h"
#include "settings.h"

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
        void OpenVideo(const std::wstring& path);
        void OpenTrack(const std::wstring& path);
        void SetMode(int mode);
        bool HasClip() const { return clip_ != nullptr; }
        bool HasTrack() const { return track_ != nullptr; }
        bool LevelsReady() const { return track_ && track_->levelsReady; }
        int ShownFormat() const;

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
            bool levelsReady = false;
        };

        Media* Cur();
        const Media* Cur() const;
        bool Playing() const { return play_ != nullptr || musicPlay_; }
        void BuildMusic();
        void SyncMusicControls();
        void UpdateMusicPreview();
        void UpdateMusicStatus(std::wstring& first, std::wstring& second, std::wstring& third);
        FrameSettings MusicFrameSettings() const;
        MusicPlan MusicPlanFor(const FrameSettings& frame) const;
        bool MusicStill(Image& out);
        void SaveMusic();
        void CopyMusic();
        void LoadCover();
        void ResetCover();
        void EnsureLevels();
        ExportResult WithProgress(const std::wstring& path, const std::function<ExportResult(const ExportStep&)>& work);
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
        void SyncLibrary();
        void LoadCorner();
        void EditTexts();
        void EditEffects();
        void ApplyEffects(Image& img, const FrameSettings& frame, const Rect& window, int scale, double t, int seed) const;
        void ResetSettings();
        void About();
        void Exit();
        void SetStyle(Style style);
        void SetScheme(int index);
        bool HandleKey(const ui::KeyEvent& e);
        void Warn(const std::wstring& text);

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
        std::array<ui::IconCell*, 11> iconCells_{};
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

        std::unique_ptr<Media> clip_;
        std::unique_ptr<Media> track_;
        std::unique_ptr<LevelReader> levels_;
        bool musicPlay_ = false;
        unsigned long long musicStart_ = 0;
        double musicFrom_ = 0.0;
        std::vector<ui::Widget*> mediaOnly_;
        std::vector<ui::Widget*> musicOnly_;
        ui::TextField* artistField_ = nullptr;
        ui::TextField* songField_ = nullptr;
        ui::Button* coverLoad_ = nullptr;
        ui::Button* coverReset_ = nullptr;
        ui::CheckBox* spinCheck_ = nullptr;
        ui::CheckBox* meterCheck_ = nullptr;
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
