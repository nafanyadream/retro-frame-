#pragma once

#include "ui.h"

namespace rf::ui
{
    const Font& UiFont();
    const Font& UiBold();

    class WindowBorder : public Widget
    {
    public:
        void Paint(Painter& p) override;
    };

    class TitleBar : public Widget
    {
    public:
        std::wstring title;
        Image icon;
        bool showMinimize = true;
        bool showMaximize = true;
        bool showClose = true;
        std::function<void()> onMinimize;
        std::function<void()> onClose;

        void Paint(Painter& p) override;
        void OnMouseDown(const MouseEvent& e) override;
        void OnMouseUp(const MouseEvent& e) override;
        void OnMouseMove(const MouseEvent& e) override;
        bool IsCaption(int x, int y) const override;

    private:
        Rect CloseRect() const;
        Rect MaxRect() const;
        Rect MinRect() const;
        int pressed_ = 0;
        bool inside_ = false;
    };

    enum class Align
    {
        Left,
        Center,
        Right
    };

    class Label : public Widget
    {
    public:
        std::wstring text;
        Font font = Font{};
        Color color = Rgb(0, 0, 0);
        Align align = Align::Left;
        bool ellipsis = true;
        int lineHeight = 13;

        void Paint(Painter& p) override;
    };

    class Button : public Widget
    {
    public:
        std::wstring text;
        Image icon;
        bool isDefault = false;
        bool flat = false;
        bool toggled = false;
        int underline = -1;
        std::function<void()> onClick;

        void Paint(Painter& p) override;
        void OnMouseDown(const MouseEvent& e) override;
        void OnMouseUp(const MouseEvent& e) override;
        void OnMouseMove(const MouseEvent& e) override;

    private:
        bool down_ = false;
        bool inside_ = false;
    };

    class CheckBox : public Widget
    {
    public:
        std::wstring text;
        bool checked = false;
        std::function<void(bool)> onChange;

        void Paint(Painter& p) override;
        void OnMouseDown(const MouseEvent& e) override;
        void OnMouseUp(const MouseEvent& e) override;

    private:
        bool down_ = false;
    };

    class RadioButton : public Widget
    {
    public:
        std::wstring text;
        int value = 0;
        int* group = nullptr;
        std::function<void(int)> onChange;

        void Paint(Painter& p) override;
        void OnMouseUp(const MouseEvent& e) override;
    };

    class GroupBox : public Widget
    {
    public:
        std::wstring text;
        void Paint(Painter& p) override;
    };

    class TextField : public Widget
    {
    public:
        std::wstring text;
        int maxLength = 300;
        std::function<void(const std::wstring&)> onChange;
        std::function<void()> onEnter;

        void SetText(const std::wstring& value);
        void SelectAll();

        void Paint(Painter& p) override;
        bool Focusable() const override { return enabled; }
        void OnMouseDown(const MouseEvent& e) override;
        void OnMouseMove(const MouseEvent& e) override;
        void OnMouseUp(const MouseEvent& e) override;
        bool OnKey(const KeyEvent& e) override;
        bool OnChar(wchar_t ch) override;
        void OnFocus(bool focused) override;
        void OnTick(unsigned long long ms) override;
        bool IBeam() const override { return true; }

    private:
        int PrefixWidth(int index) const;
        int IndexAt(int localX) const;
        void EnsureVisible();
        bool HasSelection() const { return caret_ != anchor_; }
        void DeleteSelection();
        void Insert(const std::wstring& s);
        void Changed();
        void Blink();

        int caret_ = 0;
        int anchor_ = 0;
        int scroll_ = 0;
        bool caretOn_ = true;
        unsigned long long lastBlink_ = 0;
        bool dragging_ = false;
    };

    class ComboBox : public Widget
    {
    public:
        std::vector<std::wstring> items;
        int index = 0;
        std::function<void(int)> onChange;

        void Paint(Painter& p) override;
        void OnMouseDown(const MouseEvent& e) override;
        void OnWheel(int delta) override;
        bool open = false;
    };

    class ListPopup : public Widget
    {
    public:
        std::vector<std::wstring> items;
        int hoverIndex = 0;
        int offset = 0;
        int visibleCount = 0;
        std::function<void(int)> onPick;
        std::function<void()> onClosed;
        ~ListPopup() override;

        void Paint(Painter& p) override;
        void OnMouseMove(const MouseEvent& e) override;
        void OnMouseUp(const MouseEvent& e) override;
        void OnWheel(int delta) override;
        bool OnKey(const KeyEvent& e) override;
    };

    class Swatch : public Widget
    {
    public:
        Color color = Rgb(0, 0, 0);
        std::function<void()> onClick;

        void Paint(Painter& p) override;
        void OnMouseDown(const MouseEvent& e) override;
        void OnMouseUp(const MouseEvent& e) override;

    private:
        bool down_ = false;
    };

    class IconCell : public Widget
    {
    public:
        Image icon;
        bool selected = false;
        std::wstring emptyText;
        std::function<void()> onClick;

        void Paint(Painter& p) override;
        void OnMouseUp(const MouseEvent& e) override;
    };

    class TabStrip : public Widget
    {
    public:
        std::vector<std::wstring> tabs;
        int index = 0;
        std::function<void(int)> onChange;

        void Paint(Painter& p) override;
        void OnMouseDown(const MouseEvent& e) override;

    private:
        std::vector<Rect> Cells() const;
    };

    class Panel : public Widget
    {
    public:
        enum class Kind
        {
            Raised,
            Sunken,
            ThinSunken,
            EtchedH,
            EtchedV
        };
        Kind kind = Kind::Raised;
        bool fill = true;
        Color fillColor = Rgb(0xC0, 0xC0, 0xC0);
        bool useFill = false;

        void Paint(Painter& p) override;
    };

    class PreviewPane : public Widget
    {
    public:
        Image image;
        std::wstring message;
        Color background = Rgb(0x00, 0x80, 0x80);

        void Paint(Painter& p) override;
    };

    struct ThumbItem
    {
        Image thumb;
        std::wstring name;
    };

    class ThumbList : public Widget
    {
    public:
        std::vector<ThumbItem> items;
        int selected = -1;
        std::function<void(int)> onSelect;

        void Paint(Painter& p) override;
        void OnMouseDown(const MouseEvent& e) override;
        void OnMouseMove(const MouseEvent& e) override;
        void OnMouseUp(const MouseEvent& e) override;
        void OnWheel(int delta) override;
        void EnsureVisible(int index);

    private:
        int ContentWidth() const;
        int ViewWidth() const;
        bool NeedsScroll() const;
        Rect TrackRect() const;
        Rect ThumbRect() const;
        void ScrollTo(int value);

        int scroll_ = 0;
        bool draggingThumb_ = false;
        int dragStartX_ = 0;
        int dragStartScroll_ = 0;
    };

    struct StatusField
    {
        std::wstring text;
        int width = 0;
    };

    class StatusBar : public Widget
    {
    public:
        std::vector<StatusField> fields;
        bool grip = true;
        void Paint(Painter& p) override;
    };

    struct MenuItem
    {
        std::wstring text;
        std::wstring shortcut;
        std::function<void()> action;
        bool enabled = true;
        bool separator = false;
    };

    struct Menu
    {
        std::wstring title;
        std::vector<MenuItem> items;
    };

    class MenuBar : public Widget
    {
    public:
        std::vector<Menu> menus;
        int openIndex = -1;

        void Paint(Painter& p) override;
        void OnMouseDown(const MouseEvent& e) override;
        void OnMouseMove(const MouseEvent& e) override;
        void OnMouseLeave() override;
        void OpenMenu(int index);

    private:
        std::vector<Rect> Cells() const;
        int hot_ = -1;
    };

    class MenuPopup : public Widget
    {
    public:
        std::vector<MenuItem> items;
        MenuBar* owner = nullptr;
        int hoverIndex = -1;
        ~MenuPopup() override;

        static Rect Measure(const std::vector<MenuItem>& items, int x, int y);
        void Paint(Painter& p) override;
        void OnMouseMove(const MouseEvent& e) override;
        void OnMouseUp(const MouseEvent& e) override;
        void OnMouseLeave() override;
        bool OnKey(const KeyEvent& e) override;

    private:
        int ItemAt(int y) const;
        int ItemTop(int index) const;
    };

    class ProgressBar : public Widget
    {
    public:
        double value = 0.0;
        void Paint(Painter& p) override;
    };

    class GlyphView : public Widget
    {
    public:
        const Glyph* glyph = nullptr;
        Color color = Rgb(0, 0, 0);
        const Glyph* mark = nullptr;
        Color markColor = Rgb(255, 255, 255);
        int markX = 0;
        int markY = 0;
        Image image;

        void Paint(Painter& p) override;
    };
}
