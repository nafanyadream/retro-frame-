#pragma once

#include "effects.h"
#include "frame.h"
#include "host.h"
#include "widgets.h"

namespace rf
{
    ui::TitleBar* AddDialogChrome(View& view, const std::wstring& title);
    void InstallDialogKeys(View& view, std::function<void()> onEnter, std::function<void()> onEscape);

    class MessageView : public View
    {
    public:
        MessageView(const std::wstring& title, const std::wstring& text, MessageKind kind);
    };

    class TextsView : public View
    {
    public:
        explicit TextsView(const FrameSettings& settings);
        void Apply(FrameSettings& settings) const;

    private:
        ui::TextField* menu_ = nullptr;
        ui::TextField* fields_[3] = {nullptr, nullptr, nullptr};
        ui::TextField* address_ = nullptr;
        ui::CheckBox* icon_ = nullptr;
    };

    class PlayerTextsView : public View
    {
    public:
        PlayerTextsView(const std::vector<std::wstring>& menu, const std::wstring& status, bool time, const std::wstring& address);
        std::vector<std::wstring> Menu() const;
        std::wstring Status() const { return status_->text; }
        bool Time() const { return time_->checked; }
        std::wstring Address() const { return address_->text; }

    private:
        ui::TextField* menu_ = nullptr;
        ui::TextField* status_ = nullptr;
        ui::CheckBox* time_ = nullptr;
        ui::TextField* address_ = nullptr;
    };

    class EffectsView : public View
    {
    public:
        explicit EffectsView(const EffectSettings& settings);
        EffectSettings Result() const;

    private:
        EffectSettings base_;
        ui::CheckBox* intro_ = nullptr;
        ui::ComboBox* cursor_ = nullptr;
        ui::ComboBox* place_ = nullptr;
        ui::ComboBox* popups_ = nullptr;
        ui::ComboBox* icon_ = nullptr;
        ui::ComboBox* buttons_ = nullptr;
        ui::TextField* title_ = nullptr;
        ui::TextField* line1_ = nullptr;
        ui::TextField* line2_ = nullptr;
        ui::ComboBox* screen_ = nullptr;
    };

    class PresetsView : public View
    {
    public:
        PresetsView(std::function<std::vector<ui::ListItem>()> items, std::function<void(int)> apply, std::function<bool(const std::wstring&)> save,
                    std::function<void(int)> remove);

    private:
        void Reload(int select);

        std::function<std::vector<ui::ListItem>()> items_;
        ui::ListBox* list_ = nullptr;
        ui::TextField* name_ = nullptr;
        ui::Button* apply_ = nullptr;
        ui::Button* remove_ = nullptr;
    };

    class ProgressView : public View
    {
    public:
        explicit ProgressView(const std::wstring& title);
        void Update(const std::wstring& line1, const std::wstring& line2, double value);
        bool cancelled = false;

    private:
        ui::Label* line1_ = nullptr;
        ui::Label* line2_ = nullptr;
        ui::ProgressBar* bar_ = nullptr;
    };

    void ShowMessage(Host& host, const std::wstring& title, const std::wstring& text, MessageKind kind);
}
