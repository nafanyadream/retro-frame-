#include "test_support.h"

#include "../src/core/frame.h"
#include "../src/core/icons.h"

#include <cstdio>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf("usage: frames_test photo.ppm outdir\n");
        return 1;
    }
    rftest::TestTextEngine engine;
    rf::SetTextEngine(&engine);
    rf::Image photo;
    if (!rftest::ReadPpm(argv[1], photo))
    {
        std::printf("cannot read photo\n");
        return 1;
    }
    const std::string out = argv[2];
    const rf::FrameVars vars{L"закат.jpg", photo.width, photo.height};

    struct Case
    {
        const char* name;
        rf::FrameSettings s;
    };
    std::vector<Case> cases;

    for (int st = 0; st < 3; ++st)
    {
        rf::FrameSettings s;
        s.style = static_cast<rf::Style>(st);
        s.colors = rf::SchemeColors(0, s.style);
        s.title = L"закат.jpg";
        s.icon = rf::BuiltinIcon(rf::IconId::Picture);
        cases.push_back({st == 0 ? "style95" : st == 1 ? "style98" : "style2000", s});
    }
    {
        rf::FrameSettings s;
        s.colors = rf::SchemeColors(0, s.style);
        s.title = L"Фото";
        s.icon = rf::BuiltinIcon(rf::IconId::Folder);
        s.menu = true;
        s.toolbar = true;
        s.address = true;
        s.status = true;
        s.corner = true;
        s.cornerImage = rf::BuiltinIcon(rf::IconId::Globe);
        cases.push_back({"all98", s});
    }
    {
        rf::FrameSettings s;
        s.style = rf::Style::Win2000;
        s.colors = rf::SchemeColors(1, s.style);
        s.title = L"Свойства: закат.jpg";
        s.icon = rf::BuiltinIcon(rf::IconId::Picture);
        s.kind = rf::WindowKind::Help;
        s.sunken = true;
        s.menu = true;
        s.status = true;
        cases.push_back({"lilac2000", s});
    }
    {
        rf::FrameSettings s;
        s.style = rf::Style::Win95;
        s.colors = rf::SchemeColors(0, s.style);
        s.title = L"поле.jpg";
        s.icon = rf::BuiltinIcon(rf::IconId::Picture);
        s.active = false;
        s.menu = true;
        cases.push_back({"inactive95", s});
    }
    {
        rf::FrameSettings s;
        s.style = rf::Style::Win98;
        s.colors = rf::SchemeColors(6, s.style);
        s.title = L"Высокий контраст";
        s.kind = rf::WindowKind::Dialog;
        s.menu = true;
        s.status = true;
        cases.push_back({"contrast", s});
    }

    for (const Case& c : cases)
    {
        const int k = c.s.scale > 0 ? c.s.scale : 2;
        const rf::Image img = rf::ComposeFramed(photo, c.s, vars, k);
        rftest::WritePpm(out + "/" + c.name + ".ppm", img);
        std::printf("%s %dx%d\n", c.name, img.width, img.height);
    }
    return 0;
}
