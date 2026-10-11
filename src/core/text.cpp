#include "text.h"

namespace rf
{
    namespace
    {
        TextEngine* currentEngine = nullptr;
    }

    void SetTextEngine(TextEngine* engine)
    {
        currentEngine = engine;
    }

    TextEngine& Text()
    {
        return *currentEngine;
    }

    std::wstring Ellipsize(const std::wstring& text, const Font& font, int maxWidth)
    {
        TextEngine& engine = Text();
        if (engine.Width(text, font) <= maxWidth)
        {
            return text;
        }
        const std::wstring dots = L"...";
        std::wstring s = text;
        while (!s.empty() && engine.Width(s + dots, font) > maxWidth)
        {
            s.pop_back();
        }
        while (!s.empty() && s.back() == L' ')
        {
            s.pop_back();
        }
        return s + dots;
    }
}
