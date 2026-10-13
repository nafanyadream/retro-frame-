#pragma once

#include "dialogs.h"
#include "voice.h"

#include <memory>

namespace rf
{
    class VoiceView : public View
    {
    public:
        VoiceView(Host& host, const VoiceChain& chain, const std::vector<VoicePreset>& presets, std::shared_ptr<const std::vector<std::int16_t>> raw, int rate);

        const VoiceChain& Chain() const { return chain_; }
        const std::vector<VoicePreset>& UserPresets() const { return user_; }
        bool Listening() const { return out_ != nullptr; }
        int Selected() const { return selected_; }

        void Select(int index);
        void Toggle(int index);
        void SetValue(int param, int value);
        void Move(int delta);
        void ApplyPreset(int index);
        bool SavePreset(const std::wstring& name);
        void DeletePreset();
        void Listen();
        void StopListen();
        void OnTick(unsigned long long ms) override;

    private:
        void Reload();
        void SyncParams();
        void ReloadPresets(int select);
        void Edited();

        Host& host_;
        VoiceChain chain_;
        std::vector<VoicePreset> builtin_;
        std::vector<VoicePreset> user_;
        std::shared_ptr<const std::vector<std::int16_t>> raw_;
        int rate_ = 0;
        int selected_ = 0;
        ui::ListBox* list_ = nullptr;
        ui::Label* name_ = nullptr;
        ui::CheckBox* on_ = nullptr;
        ui::Label* about_ = nullptr;
        ui::Label* paramName_[2] = {nullptr, nullptr};
        ui::TrackBar* slider_[2] = {nullptr, nullptr};
        ui::Label* value_[2] = {nullptr, nullptr};
        ui::Button* up_ = nullptr;
        ui::Button* down_ = nullptr;
        ui::ComboBox* preset_ = nullptr;
        ui::Button* remove_ = nullptr;
        ui::TextField* presetName_ = nullptr;
        ui::Button* save_ = nullptr;
        ui::Button* listen_ = nullptr;
        ui::Label* listenInfo_ = nullptr;
        std::unique_ptr<AudioOut> out_;
        std::vector<std::int16_t> pending_;
        std::size_t sent_ = 0;
        unsigned long long started_ = 0;
        bool startPending_ = false;
    };
}
