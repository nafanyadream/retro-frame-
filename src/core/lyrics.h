#pragma once

#include <string>
#include <vector>

namespace rf
{
    struct LyricLine
    {
        double time = 0.0;
        std::wstring text;
    };

    std::wstring DecodeText(const std::string& bytes);
    std::vector<LyricLine> ParseLyrics(const std::string& bytes, double duration);
    int LyricIndex(const std::vector<LyricLine>& lines, double seconds);
    bool IsLyricsFile(const std::wstring& path);
}
