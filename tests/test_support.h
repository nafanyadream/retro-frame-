#pragma once

#include "../src/core/image.h"
#include "../src/core/text.h"

#include <cstdio>
#include <string>
#include <vector>

namespace rftest
{
    struct TestGlyph
    {
        int code;
        int advance;
        const char* rows[13];
    };

#include "testfont.inc"

    class TestTextEngine : public rf::TextEngine
    {
    public:
        int Width(const std::wstring& text, const rf::Font& font) override
        {
            int w = 0;
            for (wchar_t ch : text)
            {
                w += Find(ch, font).advance + (font.bold ? 1 : 0);
            }
            return w;
        }

        int Height(const rf::Font&) override { return 13; }
        int Ascent(const rf::Font&) override { return 11; }

        void Draw(rf::Image& target, const rf::Rect& clip, int x, int y, const std::wstring& text, const rf::Font& font, rf::Color color) override
        {
            int cx = x;
            for (wchar_t ch : text)
            {
                const TestGlyph& g = Find(ch, font);
                for (int pass = 0; pass < (font.bold ? 2 : 1); ++pass)
                {
                    for (int j = 0; j < 13; ++j)
                    {
                        const char* row = g.rows[j];
                        for (int i = 0; row[i] != '\0'; ++i)
                        {
                            if (row[i] == 'x')
                            {
                                const int px = cx + i + pass;
                                const int py = y + j;
                                if (clip.Contains(px, py))
                                {
                                    target.Set(px, py, color);
                                }
                            }
                        }
                    }
                }
                cx += g.advance + (font.bold ? 1 : 0);
            }
        }

    private:
        static const TestGlyph& Find(wchar_t ch, const rf::Font& font)
        {
            const bool second = font.family == rf::FontFamily::Tahoma || font.family == rf::FontFamily::Trebuchet || font.family == rf::FontFamily::Lucida;
            const TestGlyph* table = second ? kFont1 : kFont0;
            const std::size_t n = second ? sizeof(kFont1) / sizeof(kFont1[0]) : sizeof(kFont0) / sizeof(kFont0[0]);
            for (std::size_t i = 0; i < n; ++i)
            {
                if (table[i].code == int(ch))
                {
                    return table[i];
                }
            }
            for (std::size_t i = 0; i < n; ++i)
            {
                if (table[i].code == '?')
                {
                    return table[i];
                }
            }
            return table[0];
        }
    };

    inline bool WritePpm(const std::string& path, const rf::Image& img)
    {
        FILE* f = std::fopen(path.c_str(), "wb");
        if (f == nullptr)
        {
            return false;
        }
        std::fprintf(f, "P6\n%d %d\n255\n", img.width, img.height);
        std::vector<unsigned char> row(std::size_t(img.width) * 3u);
        for (int y = 0; y < img.height; ++y)
        {
            for (int x = 0; x < img.width; ++x)
            {
                const rf::Color c = img.Get(x, y);
                row[std::size_t(x) * 3 + 0] = static_cast<unsigned char>(rf::Red(c));
                row[std::size_t(x) * 3 + 1] = static_cast<unsigned char>(rf::Green(c));
                row[std::size_t(x) * 3 + 2] = static_cast<unsigned char>(rf::Blue(c));
            }
            std::fwrite(row.data(), 1, row.size(), f);
        }
        std::fclose(f);
        return true;
    }

    inline bool ReadPpm(const std::string& path, rf::Image& out)
    {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (f == nullptr)
        {
            return false;
        }
        int w = 0, h = 0, maxv = 0;
        if (std::fscanf(f, "P6 %d %d %d", &w, &h, &maxv) != 3)
        {
            std::fclose(f);
            return false;
        }
        std::fgetc(f);
        out = rf::Image(w, h);
        std::vector<unsigned char> row(std::size_t(w) * 3u);
        for (int y = 0; y < h; ++y)
        {
            if (std::fread(row.data(), 1, row.size(), f) != row.size())
            {
                break;
            }
            for (int x = 0; x < w; ++x)
            {
                out.Set(x, y, rf::Rgb(row[std::size_t(x) * 3], row[std::size_t(x) * 3 + 1], row[std::size_t(x) * 3 + 2]));
            }
        }
        std::fclose(f);
        return true;
    }
}
