#include "test_support.h"

#include "../src/core/dialogs.h"
#include "../src/core/mainview.h"

#include <cstdio>

namespace
{
    class StubHost : public rf::Host
    {
    public:
        std::vector<std::wstring> AskOpenImages(const std::wstring&) override { return {}; }
        std::wstring AskOpenIcon(const std::wstring&) override { return L""; }
        std::wstring AskSavePath(const std::wstring&, const std::wstring&, rf::ImageFormat) override { return L""; }
        std::wstring AskFolder(const std::wstring&, const std::wstring&) override { return L""; }
        std::wstring AskOpenArchive(const std::wstring&) override { return L""; }
        bool AskColor(rf::Color, rf::Color&) override { return false; }
        bool ReadImage(const std::wstring&, rf::Image&, std::wstring& e) override
        {
            e = L"нет";
            return false;
        }
        bool ReadIcon(const std::wstring&, rf::Image&, std::wstring&) override { return false; }
        bool WriteImage(const std::wstring&, const rf::Image&, rf::ImageFormat, int, std::wstring&) override { return true; }
        bool DecodeIcon(const std::vector<unsigned char>&, rf::Image&, std::wstring&) override { return false; }
        bool ReadFileIcons(const std::wstring&, std::vector<rf::Image>&, std::wstring&) override { return false; }
        bool ClipboardImage(rf::Image&) override { return false; }
        std::vector<std::wstring> ClipboardFiles() override { return {}; }
        bool SetClipboardImage(const rf::Image&) override { return true; }
        bool FileExists(const std::wstring&) override { return false; }
        std::wstring SettingsFolder() override { return L""; }
        bool ReadText(const std::wstring&, std::string&) override { return false; }
        bool WriteText(const std::wstring&, const std::string&) override { return true; }
        bool RemoveFile(const std::wstring&) override { return true; }
        bool ListFolder(const std::wstring&, std::vector<rf::FolderItem>&) override { return false; }
        std::unique_ptr<rf::ByteSource> OpenFile(const std::wstring&) override { return nullptr; }
        std::wstring SevenZipPath() override { return L""; }
        bool ExtractArchive(const std::wstring&, const std::wstring&, const std::function<bool()>&, std::wstring&) override { return false; }
        std::wstring NewTempFolder() override { return L""; }
        void RemoveFolder(const std::wstring&) override {}
        int RunModal(rf::View&) override { return 0; }
        void* OpenModeless(rf::View&) override { return nullptr; }
        void CloseModeless(void*) override {}
        void Pump() override {}
        void Minimize() override {}
        void Quit() override {}
        unsigned long long Ticks() override { return 0; }
    };

    void Shot(rf::View& v, const std::string& path)
    {
        rf::Image img;
        v.root.Paint(img);
        rftest::WritePpm(path, img);
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
    StubHost host;
    rf::MainView main(host);
    const std::string out = argv[3];
    Shot(main, out + "/ui_empty.ppm");
    rf::Image a, b;
    rftest::ReadPpm(argv[1], a);
    rftest::ReadPpm(argv[2], b);
    main.AddImage(a, L"", L"закат.jpg", true);
    main.AddImage(b, L"", L"поле.jpg", true);
    main.AddImage(a, L"", L"ещё одно фото с длинным именем.jpg", true);
    main.AddImage(b, L"", L"четыре.png", true);
    main.AddImage(a, L"", L"пять.png", true);
    main.AddImage(b, L"", L"шесть.png", true);
    main.MutableSettings().frame.menu = true;
    main.MutableSettings().frame.status = true;
    main.Refresh();
    Shot(main, out + "/ui_photos.ppm");
    main.root.MouseDown(20, 30, false, false, 1);
    main.root.MouseUp(20, 30);
    main.root.MouseMove(40, 70);
    Shot(main, out + "/ui_menu.ppm");
    main.root.Key(rf::ui::KeyEvent{rf::ui::KeyCode::Escape});
    main.root.MouseDown(800, 377, false, false, 1);
    main.root.MouseUp(800, 377);
    main.root.MouseMove(780, 430);
    Shot(main, out + "/ui_combo.ppm");

    rf::FrameSettings fs;
    rf::TextsView texts(fs);
    Shot(texts, out + "/ui_texts.ppm");
    rf::MessageView msg(L"Ретро-рамка", L"Не удалось открыть некоторые файлы:\n\nфото.heic: формат не поддерживается", rf::MessageKind::Warning);
    Shot(msg, out + "/ui_msg.ppm");
    rf::MessageView info(L"О программе", L"Ретро-рамка, версия 0.1.0\n\nОформляет фото в окна в стиле Windows 95, 98 и 2000.", rf::MessageKind::Info);
    Shot(info, out + "/ui_info.ppm");
    rf::ProgressView prog(L"Сохранение");
    prog.Update(L"Сохраняется: закат.jpg", L"3 из 10", 0.3);
    Shot(prog, out + "/ui_progress.ppm");
    std::printf("ok\n");
    return 0;
}
