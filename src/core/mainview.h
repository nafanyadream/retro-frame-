#pragma once

#include "dialogs.h"
#include "settings.h"

#include <array>

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
        FrameSettings FrameFor(const Photo& photo) const;
        FrameVars VarsFor(const Photo& photo) const;
        bool FullImage(const Photo& photo, Image& out, std::wstring& error);
        bool Compose(const Photo& photo, Image& out, std::wstring& error);
        std::wstring Extension() const;
        ImageFormat Format() const;
        int Quality() const;
        std::wstring UniquePath(const std::wstring& dir, const std::wstring& stem) const;

        void Open();
        void Paste();
        void SaveCurrent();
        void SaveAll();
        void CopyCurrent();
        void RemoveCurrent();
        void RemoveAll();
        void Select(int index);
        void PickColor(Color FrameColors::*field);
        void LoadCustomIcon();
        void LoadCorner();
        void EditTexts();
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
        std::array<ui::IconCell*, 9> iconCells_{};
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
    };
}
