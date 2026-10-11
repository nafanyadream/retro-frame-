#pragma once

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
