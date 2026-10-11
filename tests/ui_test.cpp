#include "test_support.h"

#include "../src/core/dialogs.h"
#include "../src/core/mainview.h"
#include "test_host.h"

#include <cstdio>

namespace
{
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
    rftest::TestHost host;
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
    main.MutableSettings().looks[0].menu = true;
    main.MutableSettings().looks[0].status = true;
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
