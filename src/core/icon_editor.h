#pragma once

#include "dialogs.h"

namespace rf
{
    enum class IconTool
    {
        Pencil = 0,
        Fill = 1,
        Eraser = 2,
        Picker = 3
    };

    class IconCanvas;

    class IconEditorView : public View
    {
    public:
        IconEditorView(Host& host, const Image& start);

        Image Result() const { return icon_; }
        IconTool Tool() const { return tool_; }
        Color Current() const { return color_; }

        void SetTool(IconTool tool);
        void SetColor(Color color);
        void Touch(int x, int y, bool startStroke);
        void Undo();
        void Clear();

    private:
        void Sync();
        void Snapshot();

        Host& host_;
        Image icon_;
        std::vector<Image> history_;
        IconTool tool_ = IconTool::Pencil;
        Color color_ = Rgb(0, 0, 0);
        IconCanvas* canvas_ = nullptr;
        ui::Button* tools_[4] = {nullptr, nullptr, nullptr, nullptr};
        ui::GlyphView* current_ = nullptr;
        ui::GlyphView* preview_ = nullptr;
        ui::Button* undo_ = nullptr;
    };
}
