#include "icons.h"

#include <array>

namespace rf
{
    namespace
    {
        struct IconInk
        {
            char key;
            Color color;
        };

#include "icons_data.inc"

        constexpr int iconCount = int(sizeof(kIconRows) / sizeof(kIconRows[0]));

        Image Decode(int index)
        {
            Image img(16, 16, 0);
            for (int y = 0; y < 16; ++y)
            {
                const char* row = kIconRows[index][y];
                for (int x = 0; x < 16 && row[x] != '\0'; ++x)
                {
                    const char k = row[x];
                    if (k == '.')
                    {
                        continue;
                    }
                    for (const IconInk& ink : kIconInks)
                    {
                        if (ink.key == k)
                        {
                            img.Set(x, y, ink.color);
                            break;
                        }
                    }
                }
            }
            return img;
        }

        const std::array<IconId, 11> frameIcons = {IconId::Picture, IconId::Folder, IconId::Monitor, IconId::Document, IconId::Floppy, IconId::Globe,
                                                   IconId::Music,   IconId::Mail,   IconId::Film,    IconId::Disc,     IconId::Microphone};

        const wchar_t* const frameIconNames[] = {L"Картинка", L"Папка", L"Монитор", L"Документ", L"Дискета", L"Глобус",
                                                 L"Нота",     L"Конверт", L"Видео", L"Диск",     L"Микрофон"};
    }

    const Image& BuiltinIcon(IconId id)
    {
        static std::array<Image, iconCount> cache;
        static std::array<bool, iconCount> ready{};
        const int i = std::clamp(int(id), 0, iconCount - 1);
        if (!ready[std::size_t(i)])
        {
            cache[std::size_t(i)] = Decode(i);
            ready[std::size_t(i)] = true;
        }
        return cache[std::size_t(i)];
    }

    int FrameIconCount()
    {
        return int(frameIcons.size());
    }

    IconId FrameIconAt(int index)
    {
        return frameIcons[std::size_t(std::clamp(index, 0, FrameIconCount() - 1))];
    }

    std::wstring FrameIconName(int index)
    {
        return frameIconNames[std::clamp(index, 0, FrameIconCount() - 1)];
    }

    Image DisabledIcon(const Image& icon, const Shades& shades)
    {
        Image out(icon.width, icon.height, 0);
        for (int y = 0; y < icon.height; ++y)
        {
            for (int x = 0; x < icon.width; ++x)
            {
                if (Alpha(icon.Get(x, y)) > 0 && x + 1 < icon.width && y + 1 < icon.height)
                {
                    out.Set(x + 1, y + 1, shades.hilite);
                }
            }
        }
        for (int y = 0; y < icon.height; ++y)
        {
            for (int x = 0; x < icon.width; ++x)
            {
                if (Alpha(icon.Get(x, y)) > 0)
                {
                    out.Set(x, y, shades.shadow);
                }
            }
        }
        return out;
    }
}
