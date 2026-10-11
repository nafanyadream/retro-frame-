#include "test_support.h"

#include "../src/core/library_view.h"
#include "../src/core/mainview.h"
#include "../src/core/utf.h"

#include "test_host.h"

#include <cstdio>

namespace
{
    using rftest::TestHost;

    void Shot(rf::View& v, const std::string& path)
    {
        rf::Image img;
        v.root.Paint(img);
        rftest::WritePpm(path, img);
    }

    int failures = 0;

    void Check(bool ok, const char* what)
    {
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
        if (!ok)
        {
            ++failures;
        }
    }

    void Click(rf::View& v, int x, int y, int clicks = 1)
    {
        v.root.MouseDown(x, y, false, false, clicks);
        v.root.MouseUp(x, y);
    }
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        return 1;
    }
    rftest::TestTextEngine engine;
    rf::SetTextEngine(&engine);
    const std::wstring folder = rf::FromUtf8(argv[1]);
    const std::wstring zipPath = rf::FromUtf8(argv[2]);
    const std::string out = argv[3];
    TestHost host;

    {
        rf::Image src(32, 32, 0);
        for (int y = 2; y < 30; ++y)
        {
            for (int x = 2; x < 30; ++x)
            {
                const bool edge = x == 2 || y == 2 || x == 29 || y == 29;
                src.Set(x, y, edge ? rf::Rgb(0, 0, 0) : rf::Rgb(255, 255, 0));
            }
        }
        const rf::Image small = rf::MakeIcon16(src);
        Check(small.Get(1, 8) == rf::Rgb(0, 0, 0) && small.Get(14, 8) == rf::Rgb(0, 0, 0) && small.Get(8, 8) == rf::Rgb(255, 255, 0),
              "32x32 outline survives pixel shrink");
        Check(rf::Alpha(small.Get(0, 0)) == 0, "transparent corner stays transparent");
        rf::Image exact(16, 16, 0);
        exact.Set(3, 4, rf::Rgb(10, 20, 30));
        exact.Set(5, 6, rf::Argb(200, 1, 2, 3));
        exact.Set(7, 7, rf::Argb(60, 1, 2, 3));
        const rf::Image same = rf::MakeIcon16(exact);
        Check(same.Get(3, 4) == rf::Rgb(10, 20, 30) && same.Get(5, 6) == rf::Rgb(1, 2, 3) && same.Get(7, 7) == 0, "16x16 kept pixel for pixel, alpha cut");
        Check(rf::NaturalLess(L"icon2.ico", L"icon10.ico") && !rf::NaturalLess(L"icon10.ico", L"icon2.ico"), "natural sort");
    }

    rf::LibrarySource dir;
    dir.kind = rf::SourceKind::Folder;
    dir.path = folder;
    int steps = 0;
    const bool doneDir = rf::LoadSource(host, dir, [&](const std::wstring&, double) {
        ++steps;
        return true;
    });
    std::printf("folder: %zu icons, failed %d, steps %d\n", dir.icons.size(), dir.failed, steps);
    Check(doneDir && dir.loaded && !dir.partial, "folder loaded");
    Check(dir.icons.size() >= 20 && dir.failed == 1, "folder icons found, one broken file counted");
    bool nested = false;
    for (const auto& icon : dir.icons)
    {
        nested = nested || icon.where.find(L"\\") != std::wstring::npos;
    }
    Check(nested, "icons from subfolders included");

    rf::LibrarySource zip;
    zip.kind = rf::SourceKind::Archive;
    zip.path = zipPath;
    const bool doneZip = rf::LoadSource(host, zip, [](const std::wstring&, double) { return true; });
    std::printf("zip: %zu icons, failed %d, problem '%s'\n", zip.icons.size(), zip.failed, rf::ToUtf8(zip.problem).c_str());
    Check(doneZip && zip.icons.size() == dir.icons.size() && zip.failed == dir.failed, "zip gives the same icons as the folder");
    bool cyr = false;
    for (const auto& icon : zip.icons)
    {
        cyr = cyr || icon.where.find(L"Значки") != std::wstring::npos;
    }
    Check(cyr, "cyrillic folder name inside zip decoded");

    rf::LibrarySource cancelled;
    cancelled.kind = rf::SourceKind::Folder;
    cancelled.path = folder;
    int budget = 5;
    const bool doneCancel = rf::LoadSource(host, cancelled, [&](const std::wstring&, double) { return budget-- > 0; });
    Check(!doneCancel && cancelled.partial, "cancel stops loading");

    rf::LibrarySource seven;
    seven.kind = rf::SourceKind::Archive;
    seven.path = L"/nowhere/pack.7z";
    rf::LoadSource(host, seven, [](const std::wstring&, double) { return true; });
    Check(seven.needsSevenZip && seven.icons.empty(), "7z without 7-Zip asks for it");

    rf::LibrarySource missing;
    missing.kind = rf::SourceKind::Folder;
    missing.path = L"/nowhere/at/all";
    rf::LoadSource(host, missing, [](const std::wstring&, double) { return true; });
    Check(!missing.problem.empty(), "missing folder reported");

    rf::LibrarySource back;
    Check(rf::DecodeSource(rf::EncodeSource(zip), back) && back.kind == rf::SourceKind::Archive && back.path == zip.path, "source encode round trip");

    {
        rf::AppSettings s = rf::DefaultSettings();
        s.library = {rf::EncodeSource(dir), rf::EncodeSource(zip)};
        s.libraryFolder = L"C:\\Иконки";
        rf::AppSettings t = rf::DefaultSettings();
        rf::ParseSettings(rf::SerializeSettings(s), t);
        Check(t.library == s.library && t.libraryFolder == s.libraryFolder, "library list saved in settings");
        rf::AppSettings e = rf::DefaultSettings();
        rf::ParseSettings(rf::SerializeSettings(rf::DefaultSettings()), e);
        Check(e.library.empty(), "empty library stays empty");
    }

    std::vector<rf::LibrarySource> sources;
    sources.push_back(dir);
    sources.push_back(zip);
    sources.push_back(seven);
    std::wstring last;
    {
        rf::LibraryView view(host, sources, last, 0);
        Shot(view, out + "/lib_all.ppm");
        Click(view, 210 + 2 + 2 + 26 * 3 + 10, 52 + 4 + 26 + 10);
        Shot(view, out + "/lib_selected.ppm");
    }
    {
        rf::LibraryView view(host, sources, last, 3);
        Shot(view, out + "/lib_zip.ppm");
        Click(view, 260, 300);
        Click(view, 210 + 2 + 2 + 26 * 1 + 10, 52 + 4 + 10, 2);
        Check(view.closeRequested && view.result == 1 && view.HasPick(), "double click picks icon");
    }
    {
        rf::LibraryView view(host, sources, last, 4);
        Shot(view, out + "/lib_7z.ppm");
    }
    {
        rf::LibraryView view(host, sources, last, 2);
        view.root.SetFocus(nullptr);
        Click(view, 400, 37);
        for (wchar_t ch : std::wstring(L"sun"))
        {
            view.root.Char(ch);
        }
        Shot(view, out + "/lib_search.ppm");
    }
    {
        std::vector<rf::LibrarySource> none;
        rf::LibraryView view(host, none, last, 0);
        Shot(view, out + "/lib_empty.ppm");
    }
    {
        std::vector<rf::Image> icons;
        for (const auto& icon : dir.icons)
        {
            icons.push_back(rf::ScaleNearest(icon.icon, 2));
        }
        rf::IconPickerView picker(L"shell32.dll", icons);
        Click(picker, 10 + 2 + 26 * 4 + 12, 48 + 4 + 10);
        Shot(picker, out + "/picker.ppm");
    }
    {
        rf::MainView main(host);
        main.MutableSettings().customIcon = dir.icons[3].icon;
        main.MutableSettings().looks[0].iconIndex = rf::CustomIconIndex;
        main.Refresh();
        Shot(main, out + "/main_icons.ppm");
        main.MutableSettings().looks[0].iconIndex = 9;
        main.Refresh();
        rf::Image photo(320, 200, rf::Rgb(90, 140, 200));
        main.AddImage(photo, L"", L"диск.png", true);
        Shot(main, out + "/main_disc.ppm");
    }
    std::printf(failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures == 0 ? 0 : 2;
}
