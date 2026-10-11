#pragma once

#include "paint.h"

#include <string>

namespace rf
{
    enum class IconId
    {
        Picture,
        Folder,
        Monitor,
        Document,
        Floppy,
        Globe,
        Music,
        Mail,
        Back,
        Forward,
        Up,
        Cut,
        Copy,
        Paste,
        Film,
        Speaker,
        Disc,
        Archive
    };

    const Image& BuiltinIcon(IconId id);
    int FrameIconCount();
    IconId FrameIconAt(int index);
    std::wstring FrameIconName(int index);
    Image DisabledIcon(const Image& icon, const Shades& shades);
}
