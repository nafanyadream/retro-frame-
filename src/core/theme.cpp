#include "theme.h"

namespace rf
{
    namespace
    {
        struct Preset
        {
            const wchar_t* name;
            Color face;
            Color active1;
            Color active2;
            Color inactive1;
            Color inactive2;
            Color titleText;
            Color inactiveText;
            Color text;
            Color window;
        };

        const Preset presets[] = {
            {L"Сиреневая", Rgb(0xAE, 0xA8, 0xD9), Rgb(0x5A, 0x4E, 0xB1), Rgb(0xB0, 0xA8, 0xE2), Rgb(0x85, 0x80, 0xB0), Rgb(0xC2, 0xBE, 0xE0), Rgb(0xFF, 0xFF, 0xFF), Rgb(0xE2, 0xE0, 0xF2), Rgb(0, 0, 0), Rgb(0xFF, 0xFF, 0xFF)},
            {L"Бордовая", Rgb(0xC8, 0xC0, 0xB8), Rgb(0x80, 0x00, 0x00), Rgb(0xD0, 0x60, 0x60), Rgb(0x9A, 0x8C, 0x88), Rgb(0xC8, 0xBC, 0xB8), Rgb(0xFF, 0xFF, 0xFF), Rgb(0xE0, 0xD8, 0xD4), Rgb(0, 0, 0), Rgb(0xFF, 0xFF, 0xFF)},
            {L"Оливковая", Rgb(0xC4, 0xC4, 0xA8), Rgb(0x4E, 0x5A, 0x1E), Rgb(0xA4, 0xB0, 0x6A), Rgb(0x8E, 0x8E, 0x78), Rgb(0xC0, 0xC0, 0xA8), Rgb(0xFF, 0xFF, 0xFF), Rgb(0xE0, 0xE0, 0xD0), Rgb(0, 0, 0), Rgb(0xFF, 0xFF, 0xFF)},
            {L"Пустыня", Rgb(0xD5, 0xCC, 0xBB), Rgb(0xA2, 0x8D, 0x68), Rgb(0xD8, 0xC8, 0xA0), Rgb(0xA4, 0x9E, 0x90), Rgb(0xCE, 0xC8, 0xB8), Rgb(0xFF, 0xFF, 0xFF), Rgb(0xEA, 0xE4, 0xD8), Rgb(0, 0, 0), Rgb(0xFF, 0xFF, 0xFF)},
            {L"Морская волна", Rgb(0xC0, 0xCC, 0xCC), Rgb(0x00, 0x60, 0x60), Rgb(0x40, 0xB0, 0xB0), Rgb(0x7A, 0x90, 0x90), Rgb(0xB8, 0xC8, 0xC8), Rgb(0xFF, 0xFF, 0xFF), Rgb(0xE0, 0xEA, 0xEA), Rgb(0, 0, 0), Rgb(0xFF, 0xFF, 0xFF)},
            {L"Высокая контрастность", Rgb(0x00, 0x00, 0x00), Rgb(0x80, 0x00, 0x80), Rgb(0x80, 0x00, 0x80), Rgb(0x00, 0x80, 0x00), Rgb(0x00, 0x80, 0x00), Rgb(0xFF, 0xFF, 0xFF), Rgb(0xFF, 0xFF, 0xFF), Rgb(0xFF, 0xFF, 0xFF), Rgb(0x00, 0x00, 0x00)},
        };

        constexpr int presetCount = int(sizeof(presets) / sizeof(presets[0]));
    }

    bool HasGradient(Style style)
    {
        return style != Style::Win95;
    }

    FontFamily FamilyFor(Style style)
    {
        return style == Style::Win2000 ? FontFamily::Tahoma : FontFamily::SansSerif;
    }

    Shades ClassicShades()
    {
        return Shades{};
    }

    Shades ShadesFor(Color face, Style style)
    {
        const Color white = Rgb(0xFF, 0xFF, 0xFF);
        const Color black = Rgb(0, 0, 0);
        face = Opaque(face);
        if (face == Rgb(0xC0, 0xC0, 0xC0))
        {
            return Shades{face, white, Rgb(0xDF, 0xDF, 0xDF), Rgb(0x80, 0x80, 0x80), black};
        }
        if (face == Rgb(0xD4, 0xD0, 0xC8))
        {
            return Shades{face, white, face, Rgb(0x80, 0x80, 0x80), Rgb(0x40, 0x40, 0x40)};
        }
        if (Luminance(face) < 64)
        {
            return Shades{face, white, Rgb(0xC0, 0xC0, 0xC0), Rgb(0x80, 0x80, 0x80), white};
        }
        Shades s;
        s.face = face;
        s.hilite = Mix(face, white, 0.70);
        s.light = style == Style::Win2000 ? face : Mix(face, white, 0.35);
        s.shadow = Mix(face, black, 0.45);
        s.dark = style == Style::Win2000 ? Mix(face, black, 0.75) : black;
        return s;
    }

    int SchemeCount()
    {
        return presetCount + 1;
    }

    std::wstring SchemeName(int index)
    {
        if (index <= 0 || index > presetCount)
        {
            return L"Стандартная";
        }
        return presets[index - 1].name;
    }

    FrameColors SchemeColors(int index, Style style)
    {
        FrameColors c;
        if (index <= 0 || index > presetCount)
        {
            switch (style)
            {
            case Style::Win95:
                c.face = Rgb(0xC0, 0xC0, 0xC0);
                c.active1 = Rgb(0x00, 0x00, 0x80);
                c.active2 = Rgb(0x00, 0x00, 0x80);
                c.inactive1 = Rgb(0x80, 0x80, 0x80);
                c.inactive2 = Rgb(0x80, 0x80, 0x80);
                c.inactiveText = Rgb(0xC0, 0xC0, 0xC0);
                break;
            case Style::Win98:
                c.face = Rgb(0xC0, 0xC0, 0xC0);
                c.active1 = Rgb(0x00, 0x00, 0x80);
                c.active2 = Rgb(0x10, 0x84, 0xD0);
                c.inactive1 = Rgb(0x80, 0x80, 0x80);
                c.inactive2 = Rgb(0xC0, 0xC0, 0xC0);
                c.inactiveText = Rgb(0xC0, 0xC0, 0xC0);
                break;
            case Style::Win2000:
                c.face = Rgb(0xD4, 0xD0, 0xC8);
                c.active1 = Rgb(0x0A, 0x24, 0x6A);
                c.active2 = Rgb(0xA6, 0xCA, 0xF0);
                c.inactive1 = Rgb(0x80, 0x80, 0x80);
                c.inactive2 = Rgb(0xC0, 0xC0, 0xC0);
                c.inactiveText = Rgb(0xD4, 0xD0, 0xC8);
                c.select = Rgb(0x0A, 0x24, 0x6A);
                break;
            }
            return c;
        }
        const Preset& p = presets[index - 1];
        c.face = p.face;
        c.active1 = p.active1;
        c.active2 = style == Style::Win95 ? p.active1 : p.active2;
        c.inactive1 = p.inactive1;
        c.inactive2 = style == Style::Win95 ? p.inactive1 : p.inactive2;
        c.titleText = p.titleText;
        c.inactiveText = p.inactiveText;
        c.text = p.text;
        c.window = p.window;
        c.select = p.active1;
        return c;
    }
}
