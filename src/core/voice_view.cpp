#include "voice_view.h"

#include "glyphs.h"
#include "utf.h"

#include <cmath>

namespace rf
{
    namespace
    {
        Image CheckIcon(bool on)
        {
            Image img(13, 13);
            Painter p(img);
            const Shades sh;
            const Color white = Rgb(0xFF, 0xFF, 0xFF);
            p.Sunken(Rect{0, 0, 13, 13}, sh, &white);
            if (on)
            {
                p.DrawGlyph(3, 3, glyphs::Check, Rgb(0, 0, 0));
            }
            return img;
        }

        std::wstring Wrap(const std::wstring& text, int width)
        {
            std::wstring out;
            std::wstring line;
            std::wstring word;
            const auto flush = [&]() {
                if (word.empty())
                {
                    return;
                }
                const std::wstring candidate = line.empty() ? word : line + L" " + word;
                if (!line.empty() && Text().Width(candidate, ui::UiFont()) > width)
                {
                    out += line + L"\n";
                    line = word;
                }
                else
                {
                    line = candidate;
                }
                word.clear();
            };
            for (wchar_t c : text)
            {
                if (c == L' ')
                {
                    flush();
                }
                else
                {
                    word.push_back(c);
                }
            }
            flush();
            return out + line;
        }

        int ToValue(const VoiceParam& p, double v)
        {
            return p.min + int(std::lround(std::clamp(v, 0.0, 1.0) * (p.max - p.min)));
        }

        double ToSlider(const VoiceParam& p, int value)
        {
            return p.max > p.min ? double(value - p.min) / double(p.max - p.min) : 0.0;
        }
    }

    VoiceView::VoiceView(Host& host, const VoiceChain& chain, const std::vector<VoicePreset>& presets, std::shared_ptr<const std::vector<std::int16_t>> raw,
                         int rate)
        : View(560, 380), host_(host), chain_(chain), builtin_(BuiltinVoicePresets()), user_(presets), raw_(std::move(raw)), rate_(rate)
    {
        NormalizeVoiceChain(chain_);
        AddDialogChrome(*this, L"Эффекты записи");
        const auto label = [&](int x, int y, int w, const std::wstring& text) {
            auto* l = root.Add<ui::Label>();
            l->rect = Rect{x, y, w, 13};
            l->text = text;
            return l;
        };
        const auto button = [&](int x, int y, int w, int h, const std::wstring& text) {
            auto* b = root.Add<ui::Button>();
            b->rect = Rect{x, y, w, h};
            b->text = text;
            return b;
        };
        const auto group = [&](int x, int y, int w, int h, const std::wstring& text) {
            auto* g = root.Add<ui::GroupBox>();
            g->rect = Rect{x, y, w, h};
            g->text = text;
        };
        label(12, 30, 200, L"Эффекты по порядку, сверху вниз:");
        list_ = root.Add<ui::ListBox>();
        list_->rect = Rect{12, 46, 200, 256};
        up_ = button(12, 308, 98, 23, L"Выше");
        down_ = button(114, 308, 98, 23, L"Ниже");
        group(222, 38, 326, 168, L"Эффект");
        name_ = label(234, 58, 200, L"");
        name_->font = ui::UiBold();
        on_ = root.Add<ui::CheckBox>();
        on_->rect = Rect{448, 56, 92, 16};
        on_->text = L"Включён";
        about_ = label(234, 80, 304, L"");
        about_->rect.h = 40;
        about_->ellipsis = false;
        for (int i = 0; i < 2; ++i)
        {
            const int y = 128 + i * 34;
            paramName_[i] = label(234, y + 6, 86, L"");
            slider_[i] = root.Add<ui::TrackBar>();
            slider_[i]->rect = Rect{322, y, 156, 24};
            value_[i] = label(482, y + 6, 58, L"");
            value_[i]->align = ui::Align::Right;
        }
        group(222, 212, 326, 92, L"Наборы");
        preset_ = root.Add<ui::ComboBox>();
        preset_->rect = Rect{234, 232, 200, 21};
        remove_ = button(440, 232, 98, 21, L"Удалить");
        label(234, 266, 64, L"Название:");
        presetName_ = root.Add<ui::TextField>();
        presetName_->rect = Rect{300, 262, 134, 21};
        save_ = button(440, 262, 98, 21, L"Сохранить");
        listen_ = button(222, 312, 110, 24, L"Прослушать");
        listenInfo_ = label(340, 318, 208, L"");
        auto* ok = button(root.width - 14 - 75 - 6 - 75, root.height - 3 - 10 - 23, 75, 23, L"OK");
        ok->isDefault = true;
        auto* cancel = button(root.width - 14 - 75, root.height - 3 - 10 - 23, 75, 23, L"Отмена");

        VoiceView* me = this;
        list_->onSelect = [me](int i) { me->Select(i); };
        list_->onActivate = [me](int i) { me->Toggle(i); };
        on_->onChange = [me](bool) { me->Toggle(me->selected_); };
        for (int i = 0; i < 2; ++i)
        {
            slider_[i]->onChange = [me, i](double v) {
                const VoiceInfo& info = VoiceInfoOf(me->chain_[std::size_t(me->selected_)].kind);
                me->SetValue(i, ToValue(info.p[i], v));
            };
        }
        up_->onClick = [me]() { me->Move(-1); };
        down_->onClick = [me]() { me->Move(1); };
        preset_->onChange = [me](int i) { me->ApplyPreset(i); };
        remove_->onClick = [me]() { me->DeletePreset(); };
        save_->onClick = [me]() {
            if (!me->SavePreset(me->presetName_->text))
            {
                me->root.SetFocus(me->presetName_);
            }
        };
        presetName_->onEnter = [me]() { me->SavePreset(me->presetName_->text); };
        listen_->onClick = [me]() {
            if (me->Listening())
            {
                me->StopListen();
            }
            else
            {
                me->Listen();
            }
        };
        ok->onClick = [me]() {
            me->StopListen();
            me->Close(1);
        };
        cancel->onClick = [me]() {
            me->StopListen();
            me->Close(0);
        };
        InstallDialogKeys(*this, [me]() {
            me->StopListen();
            me->Close(1);
        }, [me]() {
            me->StopListen();
            me->Close(0);
        });
        ReloadPresets(0);
        Select(0);
        listen_->enabled = raw_ && !raw_->empty() && rate_ > 0;
        listenInfo_->text = listen_->enabled ? L"Запись с этими эффектами" : L"Сначала запишите голос";
    }

    void VoiceView::Reload()
    {
        static const Image off = CheckIcon(false);
        static const Image on = CheckIcon(true);
        list_->items.clear();
        for (const VoiceStep& s : chain_)
        {
            list_->items.push_back(ui::ListItem{s.on ? on : off, VoiceInfoOf(s.kind).name});
        }
        list_->selected = selected_;
        list_->EnsureVisible(selected_);
        up_->enabled = selected_ > 0;
        down_->enabled = selected_ + 1 < int(chain_.size());
        root.dirty = true;
    }

    void VoiceView::SyncParams()
    {
        const VoiceStep& s = chain_[std::size_t(selected_)];
        const VoiceInfo& info = VoiceInfoOf(s.kind);
        name_->text = info.name;
        on_->checked = s.on;
        about_->text = Wrap(info.about, about_->rect.w);
        const int values[2] = {s.a, s.b};
        for (int i = 0; i < 2; ++i)
        {
            const bool shown = i < info.params;
            paramName_[i]->visible = shown;
            slider_[i]->visible = shown;
            value_[i]->visible = shown;
            if (shown)
            {
                paramName_[i]->text = std::wstring(info.p[i].name) + L":";
                slider_[i]->value = ToSlider(info.p[i], values[i]);
                value_[i]->text = VoiceValueText(s.kind, i, values[i]);
            }
        }
        root.dirty = true;
    }

    void VoiceView::ReloadPresets(int select)
    {
        preset_->items = {L"Свой набор"};
        for (const auto& p : builtin_)
        {
            preset_->items.push_back(p.name);
        }
        for (const auto& p : user_)
        {
            preset_->items.push_back(p.name);
        }
        preset_->index = std::clamp(select, 0, int(preset_->items.size()) - 1);
        remove_->enabled = preset_->index > int(builtin_.size());
        root.dirty = true;
    }

    void VoiceView::Edited()
    {
        StopListen();
        preset_->index = 0;
        remove_->enabled = false;
        Reload();
        SyncParams();
    }

    void VoiceView::Select(int index)
    {
        selected_ = std::clamp(index, 0, int(chain_.size()) - 1);
        Reload();
        SyncParams();
    }

    void VoiceView::Toggle(int index)
    {
        if (index < 0 || index >= int(chain_.size()))
        {
            return;
        }
        selected_ = index;
        chain_[std::size_t(index)].on = !chain_[std::size_t(index)].on;
        Edited();
    }

    void VoiceView::SetValue(int param, int value)
    {
        VoiceStep& s = chain_[std::size_t(selected_)];
        const VoiceInfo& info = VoiceInfoOf(s.kind);
        if (param < 0 || param >= info.params)
        {
            return;
        }
        const int v = std::clamp(value, info.p[param].min, info.p[param].max);
        (param == 0 ? s.a : s.b) = v;
        s.on = true;
        Edited();
    }

    void VoiceView::Move(int delta)
    {
        const int to = selected_ + delta;
        if (to < 0 || to >= int(chain_.size()))
        {
            return;
        }
        std::swap(chain_[std::size_t(selected_)], chain_[std::size_t(to)]);
        selected_ = to;
        Edited();
    }

    void VoiceView::ApplyPreset(int index)
    {
        StopListen();
        if (index <= 0)
        {
            ReloadPresets(0);
            return;
        }
        const int b = int(builtin_.size());
        const VoicePreset* p = index <= b ? &builtin_[std::size_t(index - 1)] : index - 1 - b < int(user_.size()) ? &user_[std::size_t(index - 1 - b)] : nullptr;
        if (p == nullptr)
        {
            return;
        }
        chain_ = p->chain;
        NormalizeVoiceChain(chain_);
        selected_ = 0;
        for (std::size_t i = 0; i < chain_.size(); ++i)
        {
            if (chain_[i].on)
            {
                selected_ = int(i);
                break;
            }
        }
        if (index > b)
        {
            presetName_->SetText(p->name);
        }
        ReloadPresets(index);
        Reload();
        SyncParams();
    }

    bool VoiceView::SavePreset(const std::wstring& name)
    {
        const std::wstring clean = Trim(name);
        if (clean.empty())
        {
            return false;
        }
        int at = -1;
        for (std::size_t i = 0; i < user_.size(); ++i)
        {
            if (user_[i].name == clean)
            {
                at = int(i);
            }
        }
        if (at < 0)
        {
            user_.push_back(VoicePreset{clean, chain_});
            at = int(user_.size()) - 1;
        }
        else
        {
            user_[std::size_t(at)].chain = chain_;
        }
        ReloadPresets(1 + int(builtin_.size()) + at);
        return true;
    }

    void VoiceView::DeletePreset()
    {
        const int i = preset_->index - 1 - int(builtin_.size());
        if (i < 0 || i >= int(user_.size()))
        {
            return;
        }
        user_.erase(user_.begin() + i);
        ReloadPresets(0);
    }

    void VoiceView::Listen()
    {
        StopListen();
        if (!raw_ || raw_->empty() || rate_ <= 0)
        {
            return;
        }
        pending_ = ProcessVoice(*raw_, rate_, chain_);
        out_ = host_.OpenAudio(rate_, 1);
        if (!out_)
        {
            listenInfo_->text = L"Не удалось включить звук";
            root.dirty = true;
            return;
        }
        sent_ = 0;
        startPending_ = true;
        listen_->text = L"Стоп";
        listenInfo_->text = L"Звучит запись с эффектами";
        root.dirty = true;
    }

    void VoiceView::StopListen()
    {
        if (!out_)
        {
            return;
        }
        out_.reset();
        pending_.clear();
        sent_ = 0;
        listen_->text = L"Прослушать";
        listenInfo_->text = L"Запись с этими эффектами";
        root.dirty = true;
    }

    void VoiceView::OnTick(unsigned long long ms)
    {
        if (!out_)
        {
            return;
        }
        if (startPending_)
        {
            started_ = ms;
            startPending_ = false;
        }
        while (sent_ < pending_.size())
        {
            const std::size_t space = out_->Space() / 2;
            if (space < 1024)
            {
                break;
            }
            const std::size_t n = std::min(space, pending_.size() - sent_);
            if (!out_->Write(pending_.data() + sent_, n * 2))
            {
                break;
            }
            sent_ += n;
        }
        const unsigned long long length = (unsigned long long)(double(pending_.size()) * 1000.0 / rate_);
        if (sent_ >= pending_.size() && ms - started_ > length + 500)
        {
            StopListen();
        }
    }
}
