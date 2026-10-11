#include "ui.h"

namespace rf::ui
{
    namespace
    {
        Platform* currentPlatform = nullptr;
    }

    void SetPlatform(Platform* platform)
    {
        currentPlatform = platform;
    }

    Platform* GetPlatform()
    {
        return currentPlatform;
    }

    void Widget::Invalidate()
    {
        if (root != nullptr)
        {
            root->dirty = true;
        }
    }

    bool Widget::IsHover() const
    {
        return root != nullptr && root->hover == this;
    }

    bool Widget::IsFocused() const
    {
        return root != nullptr && root->focus == this;
    }

    bool Widget::IsCaptured() const
    {
        return root != nullptr && root->capture == this;
    }

    const Shades& Widget::Sh() const
    {
        static const Shades fallback;
        return root != nullptr ? root->shades : fallback;
    }

    Root::Root(int w, int h)
        : width(w), height(h)
    {
    }

    Root::~Root()
    {
        std::vector<std::unique_ptr<Widget>> doomed = std::move(overlays_);
        overlays_.clear();
        doomed.clear();
        graveyard_.clear();
    }

    void Root::AddOverlay(std::unique_ptr<Widget> overlay)
    {
        overlay->root = this;
        overlays_.push_back(std::move(overlay));
        dirty = true;
    }

    void Root::ReplaceOverlay(std::unique_ptr<Widget> overlay)
    {
        for (auto& o : overlays_)
        {
            if (hover == o.get())
            {
                hover = nullptr;
            }
            if (capture == o.get())
            {
                capture = nullptr;
            }
            graveyard_.push_back(std::move(o));
        }
        overlays_.clear();
        closeRequested_ = false;
        AddOverlay(std::move(overlay));
    }

    void Root::CloseOverlays()
    {
        closeRequested_ = true;
        dirty = true;
    }

    void Root::Post(std::function<void()> action)
    {
        posted_.push_back(std::move(action));
    }

    void Root::Flush()
    {
        if (closeRequested_)
        {
            closeRequested_ = false;
            for (auto& o : overlays_)
            {
                if (hover == o.get())
                {
                    hover = nullptr;
                }
                if (capture == o.get())
                {
                    capture = nullptr;
                }
                if (focus == o.get())
                {
                    focus = nullptr;
                }
                graveyard_.push_back(std::move(o));
            }
            overlays_.clear();
            dirty = true;
        }
        while (!posted_.empty())
        {
            auto actions = std::move(posted_);
            posted_.clear();
            for (auto& a : actions)
            {
                a();
            }
        }
        graveyard_.clear();
    }

    void Root::Paint(Image& target)
    {
        if (target.width != width || target.height != height)
        {
            target = Image(width, height);
        }
        Painter p(target);
        p.Fill(0, 0, width, height, shades.face);
        for (auto& w : widgets_)
        {
            if (!w->visible)
            {
                continue;
            }
            Painter sub = p.Sub(w->rect);
            w->Paint(sub);
        }
        for (auto& o : overlays_)
        {
            if (!o->visible)
            {
                continue;
            }
            Painter sub = p.Sub(o->rect);
            o->Paint(sub);
        }
        dirty = false;
    }

    Widget* Root::HitWidget(int x, int y) const
    {
        for (auto it = widgets_.rbegin(); it != widgets_.rend(); ++it)
        {
            Widget* w = it->get();
            if (w->visible && w->rect.Contains(x, y))
            {
                return w;
            }
        }
        return nullptr;
    }

    Widget* Root::HitOverlay(int x, int y) const
    {
        for (auto it = overlays_.rbegin(); it != overlays_.rend(); ++it)
        {
            Widget* w = it->get();
            if (w->visible && w->rect.Contains(x, y))
            {
                return w;
            }
        }
        return nullptr;
    }

    Widget* Root::Hit(int x, int y) const
    {
        Widget* o = HitOverlay(x, y);
        return o != nullptr ? o : HitWidget(x, y);
    }

    MouseEvent Root::Local(const Widget* w, int x, int y, bool shift, bool ctrl, int clicks)
    {
        MouseEvent e;
        e.x = x - w->rect.x;
        e.y = y - w->rect.y;
        e.shift = shift;
        e.ctrl = ctrl;
        e.clicks = clicks;
        return e;
    }

    void Root::MouseDown(int x, int y, bool shift, bool ctrl, int clicks)
    {
        if (!overlays_.empty())
        {
            Widget* o = HitOverlay(x, y);
            if (o == nullptr)
            {
                CloseOverlays();
                Flush();
                return;
            }
            capture = o;
            o->OnMouseDown(Local(o, x, y, shift, ctrl, clicks));
            Flush();
            return;
        }
        Widget* w = HitWidget(x, y);
        if (w == nullptr || !w->enabled)
        {
            Flush();
            return;
        }
        if (w->Focusable())
        {
            SetFocus(w);
        }
        else if (focus != nullptr && focus != w)
        {
            SetFocus(nullptr);
        }
        capture = w;
        w->OnMouseDown(Local(w, x, y, shift, ctrl, clicks));
        Flush();
    }

    void Root::MouseUp(int x, int y)
    {
        Widget* target = capture != nullptr ? capture : Hit(x, y);
        capture = nullptr;
        if (target != nullptr && target->enabled)
        {
            target->OnMouseUp(Local(target, x, y, false, false, 1));
        }
        dirty = true;
        Flush();
    }

    void Root::MouseMove(int x, int y)
    {
        lastX_ = x;
        lastY_ = y;
        if (capture != nullptr)
        {
            capture->OnMouseMove(Local(capture, x, y, false, false, 1));
            Flush();
            return;
        }
        Widget* w = Hit(x, y);
        if (w != nullptr && !w->enabled)
        {
            w = nullptr;
        }
        if (w != hover)
        {
            if (hover != nullptr)
            {
                hover->OnMouseLeave();
            }
            hover = w;
            dirty = true;
        }
        if (w != nullptr)
        {
            w->OnMouseMove(Local(w, x, y, false, false, 1));
        }
        Flush();
    }

    void Root::MouseLeave()
    {
        if (hover != nullptr && capture == nullptr)
        {
            hover->OnMouseLeave();
            hover = nullptr;
            dirty = true;
        }
        Flush();
    }

    void Root::Wheel(int x, int y, int delta)
    {
        Widget* w = Hit(x, y);
        if (w != nullptr && w->enabled)
        {
            w->OnWheel(delta);
        }
        Flush();
    }

    void Root::Key(const KeyEvent& e)
    {
        if (!overlays_.empty())
        {
            Widget* top = overlays_.back().get();
            if (e.key == KeyCode::Escape)
            {
                CloseOverlays();
            }
            else
            {
                top->OnKey(e);
            }
            Flush();
            return;
        }
        if (keyHook && keyHook(e))
        {
            Flush();
            return;
        }
        if (focus != nullptr && focus->OnKey(e))
        {
            Flush();
            return;
        }
        if (e.key == KeyCode::Tab)
        {
            FocusNext(e.shift);
        }
        Flush();
    }

    void Root::Char(wchar_t ch)
    {
        if (overlays_.empty() && focus != nullptr)
        {
            focus->OnChar(ch);
        }
        Flush();
    }

    void Root::Tick(unsigned long long ms)
    {
        now = ms;
        if (focus != nullptr)
        {
            focus->OnTick(ms);
        }
        Flush();
    }

    void Root::SetActive(bool value)
    {
        if (active != value)
        {
            active = value;
            dirty = true;
        }
        if (!value)
        {
            CloseOverlays();
            Flush();
        }
    }

    void Root::SetFocus(Widget* w)
    {
        if (focus == w)
        {
            return;
        }
        Widget* old = focus;
        focus = w;
        if (old != nullptr)
        {
            old->OnFocus(false);
        }
        if (w != nullptr)
        {
            w->OnFocus(true);
        }
        dirty = true;
    }

    void Root::FocusNext(bool backwards)
    {
        std::vector<Widget*> list;
        for (auto& w : widgets_)
        {
            if (w->visible && w->enabled && w->Focusable())
            {
                list.push_back(w.get());
            }
        }
        if (list.empty())
        {
            return;
        }
        int index = -1;
        for (std::size_t i = 0; i < list.size(); ++i)
        {
            if (list[i] == focus)
            {
                index = int(i);
            }
        }
        const int n = int(list.size());
        index = backwards ? (index <= 0 ? n - 1 : index - 1) : (index + 1) % n;
        SetFocus(list[std::size_t(index)]);
    }

    bool Root::InCaption(int x, int y) const
    {
        if (!overlays_.empty())
        {
            return false;
        }
        Widget* w = HitWidget(x, y);
        return w != nullptr && w->IsCaption(x - w->rect.x, y - w->rect.y);
    }

    bool Root::WantsIBeam(int x, int y) const
    {
        Widget* w = Hit(x, y);
        return w != nullptr && w->enabled && w->IBeam();
    }
}
