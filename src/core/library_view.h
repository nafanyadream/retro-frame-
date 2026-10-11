#pragma once

#include "dialogs.h"
#include "library.h"

namespace rf
{
    bool LoadWithProgress(Host& host, LibrarySource& source);
    void ExplainSevenZip(Host& host, const std::wstring& archive);

    class LibraryView : public View
    {
    public:
        LibraryView(Host& host, std::vector<LibrarySource>& sources, std::wstring& lastFolder, int startSource);

        std::function<void()> onSourcesChanged;

        int CurrentSource() const { return current_; }
        bool HasPick() const { return picked_ >= 0 && picked_ < int(shown_.size()); }
        const LibraryIcon& Picked() const { return *shown_[std::size_t(picked_)]; }

    private:
        void FillSources();
        void ShowSource(int index);
        void ApplyFilter();
        void UpdateInfo();
        void Ensure(int index);
        void AddFolder();
        void AddArchive();
        void AddSource(LibrarySource source);
        void RemoveSource();
        void Accept();
        std::wstring Where(const LibraryIcon& icon) const;

        Host& host_;
        std::vector<LibrarySource>& sources_;
        std::wstring& lastFolder_;
        std::vector<LibraryIcon> builtin_;
        std::vector<const LibraryIcon*> all_;
        std::vector<const LibraryIcon*> shown_;
        std::vector<int> owners_;
        std::vector<int> shownOwners_;
        int current_ = 0;
        int picked_ = -1;

        ui::ListBox* list_ = nullptr;
        ui::TextField* search_ = nullptr;
        ui::IconGrid* grid_ = nullptr;
        ui::GlyphView* zoom_ = nullptr;
        ui::Label* name_ = nullptr;
        ui::Label* where_ = nullptr;
        ui::Label* size_ = nullptr;
        ui::Label* count_ = nullptr;
        ui::Button* remove_ = nullptr;
        ui::Button* ok_ = nullptr;
    };

    class IconPickerView : public View
    {
    public:
        IconPickerView(const std::wstring& fileName, const std::vector<Image>& icons);
        int Picked() const { return grid_->selected; }

    private:
        void UpdateInfo();

        std::vector<Image> small_;
        std::vector<int> widths_;
        std::vector<int> heights_;
        ui::IconGrid* grid_ = nullptr;
        ui::GlyphView* zoom_ = nullptr;
        ui::Label* info_ = nullptr;
        ui::Button* ok_ = nullptr;
    };
}
