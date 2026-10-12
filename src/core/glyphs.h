#pragma once

#include "paint.h"

namespace rf::glyphs
{
    inline const Glyph Close = {"xx....xx", ".xx..xx.", "..xxxx..", "...xx...", "..xxxx..", ".xx..xx.", "xx....xx"};
    inline const Glyph Minimize = {"xxxxxx", "xxxxxx"};
    inline const Glyph Maximize = {"xxxxxxxxx", "xxxxxxxxx", "x.......x", "x.......x", "x.......x", "x.......x", "x.......x", "x.......x", "xxxxxxxxx"};
    inline const Glyph Help = {".xxxx.", "xx..xx", "....xx", "...xx.", "..xx..", "..xx..", "......", "..xx..", "..xx.."};
    inline const Glyph Down = {"xxxxxxx", ".xxxxx.", "..xxx..", "...x..."};
    inline const Glyph Up = {"...x...", "..xxx..", ".xxxxx.", "xxxxxxx"};
    inline const Glyph Left = {"...x", "..xx", ".xxx", "xxxx", ".xxx", "..xx", "...x"};
    inline const Glyph Right = {"x...", "xx..", "xxx.", "xxxx", "xxx.", "xx..", "x..."};
    inline const Glyph Check = {"......x", ".....xx", "x...xxx", "xx.xxx.", "xxxxx..", ".xxx...", "..x...."};
    inline const Glyph Dot = {".xx.", "xxxx", "xxxx", ".xx."};
    inline const Glyph MenuArrow = {"x...", "xx..", "xxx.", "xxxx", "xxx.", "xx..", "x..."};
    inline const Glyph Play = {"x....", "xx...", "xxx..", "xxxx.", "xxxxx", "xxxx.", "xxx..", "xx...", "x...."};
    inline const Glyph Pause = {"xx.xx", "xx.xx", "xx.xx", "xx.xx", "xx.xx", "xx.xx", "xx.xx", "xx.xx"};
    inline const Glyph Stop = {"xxxxxxx", "xxxxxxx", "xxxxxxx", "xxxxxxx", "xxxxxxx", "xxxxxxx", "xxxxxxx"};
    inline const Glyph Previous = {"xx...x", "xx..xx", "xx.xxx", "xxxxxx", "xx.xxx", "xx..xx", "xx...x"};
    inline const Glyph Rewind = {"...x...x", "..xx..xx", ".xxx.xxx", "xxxxxxxx", ".xxx.xxx", "..xx..xx", "...x...x"};
    inline const Glyph FastForward = {"x...x...", "xx..xx..", "xxx.xxx.", "xxxxxxxx", "xxx.xxx.", "xx..xx..", "x...x..."};
    inline const Glyph Next = {"x...xx", "xx..xx", "xxx.xx", "xxxxxx", "xxx.xx", "xx..xx", "x...xx"};
    inline const Glyph Plus = {"...x...", "...x...", "...x...", "xxxxxxx", "...x...", "...x...", "...x..."};
    inline const Glyph Minus = {"xxxxxxx"};
    inline const Glyph Shuffle = {"......x..", "xx....xxx", "..x..x.x.", "...xx....", "..x..x.x.", "xx....xxx", "......x.."};
    inline const Glyph Repeat = {".xxxxxx..", "x......x.", "x.....xxx", "x......x.", "x........", "x.......x", ".xxxxxxx."};
    inline const Glyph Heart = {".xx.xx.", "xxxxxxx", "xxxxxxx", ".xxxxx.", "..xxx..", "...x..."};
    inline const Glyph Info = {
        "...........xxxxxxxxxx...........",
        "........xxxxxxxxxxxxxxxx........",
        "......xxxxxxxxxxxxxxxxxxxx......",
        ".....xxxxxxxxxxxxxxxxxxxxxx.....",
        "....xxxxxxxxxxxxxxxxxxxxxxxx....",
        "...xxxxxxxxxxxxxxxxxxxxxxxxxx...",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxxxx..",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxxxx..",
        ".xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
        ".xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        ".xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
        ".xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxxxx..",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxxxx..",
        "...xxxxxxxxxxxxxxxxxxxxxxxxxx...",
        "....xxxxxxxxxxxxxxxxxxxxxxxx....",
        ".....xxxxxxxxxxxxxxxxxxxxxx.....",
        "......xxxxxxxxxxxxxxxxxxxx......",
        "........xxxxxxxxxxxxxxxx........",
        "...........xxxxxxxxxx..........."};
    inline const Glyph InfoMark = {".xx.", ".xx.", "....", "....", "xxx.", ".xx.", ".xx.", ".xx.", ".xx.", ".xx.", ".xx.", ".xx.", "xxxx"};
    inline const Glyph Warning = {
        "...............xx...............",
        "..............xxxx..............",
        "..............xxxx..............",
        ".............xxxxxx.............",
        ".............xxxxxx.............",
        "............xxxxxxxx............",
        "............xxxxxxxx............",
        "...........xxxxxxxxxx...........",
        "...........xxxxxxxxxx...........",
        "..........xxxxxxxxxxxx..........",
        "..........xxxxxxxxxxxx..........",
        ".........xxxxxxxxxxxxxx.........",
        ".........xxxxxxxxxxxxxx.........",
        "........xxxxxxxxxxxxxxxx........",
        "........xxxxxxxxxxxxxxxx........",
        ".......xxxxxxxxxxxxxxxxxx.......",
        ".......xxxxxxxxxxxxxxxxxx.......",
        "......xxxxxxxxxxxxxxxxxxxx......",
        "......xxxxxxxxxxxxxxxxxxxx......",
        ".....xxxxxxxxxxxxxxxxxxxxxx.....",
        ".....xxxxxxxxxxxxxxxxxxxxxx.....",
        "....xxxxxxxxxxxxxxxxxxxxxxxx....",
        "....xxxxxxxxxxxxxxxxxxxxxxxx....",
        "...xxxxxxxxxxxxxxxxxxxxxxxxxx...",
        "...xxxxxxxxxxxxxxxxxxxxxxxxxx...",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxxxx..",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxxxx..",
        ".xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"};
    inline const Glyph WarningMark = {"xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "xx", "..", "xx", "xx"};
}
