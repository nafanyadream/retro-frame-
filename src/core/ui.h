#pragma once

#include "paint.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rf::ui
{
    enum class KeyCode
    {
        None,
        Left,
        Right,
        Up,
        Down,
        Home,
        End,
        Backspace,
        Delete,
        Enter,
        Escape,
        Tab,
        PageUp,
        PageDown,
        Letter,
        Other
    };

    struct KeyEvent
    {
        KeyCode key = KeyCode::None;
        wchar_t letter = 0;
        bool shift = false;
        bool ctrl = false;
        bool alt = false;
    };

    struct MouseEvent
    {
        int x = 0;
        int y = 0;
        bool shift = false;
        bool ctrl = false;
        int clicks = 1;
    };

    class Platform
    {
    public:
        virtual ~Platform() = default;
        virtual std::wstring ClipboardText() = 0;
        virtual void SetClipboardText(const std::wstring& text) = 0;
    };

    void SetPlatform(Platform* platform);
    Platform* GetPlatform();

    class Root;

    class Widget
    {
    public:
        virtual ~Widget() = default;

        Rect rect;
        bool visible = true;
        bool enabled = true;
        Root* root = nullptr;

        virtual void Paint(Painter& p) = 0;
        virtual bool Focusable() const { return false; }
        virtual void OnMouseDown(const MouseEvent&) {}
        virtual void OnMouseUp(const MouseEvent&) {}
        virtual void OnMouseMove(const MouseEvent&) {}
        virtual void OnMouseLeave() {}
        virtual void OnWheel(int) {}
        virtual bool OnKey(const KeyEvent&) { return false; }
        virtual bool OnChar(wchar_t) { return false; }
        virtual void OnFocus(bool) {}
        virtual void OnTick(unsigned long long) {}
        virtual bool IBeam() const { return false; }
        virtual bool IsCaption(int, int) const { return false; }

        void Invalidate();
        bool IsHover() const;
        bool IsFocused() const;
        bool IsCaptured() const;
        const Shades& Sh() const;
    };

    class Root
    {
    public:
        Root(int width, int height);
        ~Root();
        Root(const Root&) = delete;
        Root& operator=(const Root&) = delete;

        int width;
        int height;
        Shades shades;
        bool active = true;
        bool dirty = true;
        unsigned long long now = 0;
        std::function<bool(const KeyEvent&)> keyHook;

        template <class T, class... Args>
        T* Add(Args&&... args)
        {
            auto w = std::make_unique<T>(std::forward<Args>(args)...);
            T* raw = w.get();
            raw->root = this;
            widgets_.push_back(std::move(w));
            dirty = true;
            return raw;
        }

        void AddOverlay(std::unique_ptr<Widget> overlay);
        void ReplaceOverlay(std::unique_ptr<Widget> overlay);
        void CloseOverlays();
        bool HasOverlay() const { return !overlays_.empty(); }
        Widget* TopOverlay() const { return overlays_.empty() ? nullptr : overlays_.back().get(); }
        void Post(std::function<void()> action);

        void Paint(Image& target);
        void MouseDown(int x, int y, bool shift, bool ctrl, int clicks);
        void MouseUp(int x, int y);
        void MouseMove(int x, int y);
        void MouseLeave();
        void Wheel(int x, int y, int delta);
        void Key(const KeyEvent& e);
        void Char(wchar_t ch);
        void Tick(unsigned long long ms);
        void SetActive(bool value);
        void SetFocus(Widget* w);
        void FocusNext(bool backwards);
        void Flush();

        bool InCaption(int x, int y) const;
        bool WantsIBeam(int x, int y) const;

        Widget* focus = nullptr;
        Widget* capture = nullptr;
        Widget* hover = nullptr;

    private:
        Widget* HitWidget(int x, int y) const;
        Widget* HitOverlay(int x, int y) const;
        Widget* Hit(int x, int y) const;
        static MouseEvent Local(const Widget* w, int x, int y, bool shift, bool ctrl, int clicks);

        std::vector<std::unique_ptr<Widget>> widgets_;
        std::vector<std::unique_ptr<Widget>> overlays_;
        std::vector<std::unique_ptr<Widget>> graveyard_;
        std::vector<std::function<void()>> posted_;
        bool closeRequested_ = false;
        int lastX_ = -1;
        int lastY_ = -1;
    };
}
