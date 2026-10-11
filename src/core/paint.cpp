#include "paint.h"

#include <cmath>
#include <cstring>

namespace rf
{
    namespace
    {
        struct Tap
        {
            int index;
            float weight;
        };

        std::vector<std::vector<Tap>> Weights(int srcLen, int dstLen)
        {
            std::vector<std::vector<Tap>> taps(static_cast<std::size_t>(dstLen));
            const double scale = double(srcLen) / double(dstLen);
            for (int i = 0; i < dstLen; ++i)
            {
                auto& list = taps[std::size_t(i)];
                if (scale >= 1.0)
                {
                    const double start = i * scale;
                    const double end = (i + 1) * scale;
                    const int first = int(std::floor(start));
                    const int last = std::min(srcLen - 1, int(std::ceil(end)) - 1);
                    double total = 0.0;
                    for (int j = first; j <= last; ++j)
                    {
                        const double w = std::min(end, double(j + 1)) - std::max(start, double(j));
                        if (w > 0.0)
                        {
                            list.push_back(Tap{j, float(w)});
                            total += w;
                        }
                    }
                    for (auto& t : list)
                    {
                        t.weight = float(t.weight / total);
                    }
                }
                else
                {
                    const double center = (i + 0.5) * scale - 0.5;
                    int j0 = int(std::floor(center));
                    const double t = center - j0;
                    const int a = std::clamp(j0, 0, srcLen - 1);
                    const int b = std::clamp(j0 + 1, 0, srcLen - 1);
                    list.push_back(Tap{a, float(1.0 - t)});
                    list.push_back(Tap{b, float(t)});
                }
            }
            return taps;
        }
    }

    Color Mix(Color a, Color b, double t)
    {
        const auto ch = [t](int x, int y) { return int(std::lround(x + (y - x) * t)); };
        return Argb(ch(Alpha(a), Alpha(b)), ch(Red(a), Red(b)), ch(Green(a), Green(b)), ch(Blue(a), Blue(b)));
    }

    Color Over(Color dst, Color src)
    {
        const int sa = Alpha(src);
        if (sa == 255)
        {
            return src;
        }
        if (sa == 0)
        {
            return dst;
        }
        const int da = Alpha(dst);
        const int oa = sa + da * (255 - sa) / 255;
        if (oa == 0)
        {
            return 0;
        }
        const auto ch = [&](int s, int d) { return (s * sa + d * da * (255 - sa) / 255) / oa; };
        return Argb(oa, ch(Red(src), Red(dst)), ch(Green(src), Green(dst)), ch(Blue(src), Blue(dst)));
    }

    int Luminance(Color c)
    {
        return (Red(c) * 30 + Green(c) * 59 + Blue(c) * 11) / 100;
    }

    Image ScaleNearest(const Image& src, int factor)
    {
        if (factor <= 1)
        {
            return src;
        }
        Image out(src.width * factor, src.height * factor);
        for (int y = 0; y < out.height; ++y)
        {
            const Color* s = src.Row(y / factor);
            Color* d = out.Row(y);
            for (int x = 0; x < out.width; ++x)
            {
                d[x] = s[x / factor];
            }
        }
        return out;
    }

    Image ResizeSmooth(const Image& src, int width, int height)
    {
        if (src.Empty() || width <= 0 || height <= 0)
        {
            return Image();
        }
        if (width == src.width && height == src.height)
        {
            return src;
        }
        const auto hx = Weights(src.width, width);
        const auto vy = Weights(src.height, height);
        std::vector<float> mid(std::size_t(width) * std::size_t(src.height) * 4u);
        for (int y = 0; y < src.height; ++y)
        {
            const Color* row = src.Row(y);
            float* out = mid.data() + std::size_t(y) * std::size_t(width) * 4u;
            for (int x = 0; x < width; ++x)
            {
                float a = 0, r = 0, g = 0, b = 0;
                for (const Tap& t : hx[std::size_t(x)])
                {
                    const Color c = row[t.index];
                    const float ca = float(Alpha(c)) * t.weight;
                    a += ca;
                    r += float(Red(c)) * ca;
                    g += float(Green(c)) * ca;
                    b += float(Blue(c)) * ca;
                }
                out[x * 4 + 0] = a;
                out[x * 4 + 1] = r;
                out[x * 4 + 2] = g;
                out[x * 4 + 3] = b;
            }
        }
        Image result(width, height);
        for (int y = 0; y < height; ++y)
        {
            Color* d = result.Row(y);
            for (int x = 0; x < width; ++x)
            {
                float a = 0, r = 0, g = 0, b = 0;
                for (const Tap& t : vy[std::size_t(y)])
                {
                    const float* m = mid.data() + (std::size_t(t.index) * std::size_t(width) + std::size_t(x)) * 4u;
                    a += m[0] * t.weight;
                    r += m[1] * t.weight;
                    g += m[2] * t.weight;
                    b += m[3] * t.weight;
                }
                if (a <= 0.001f)
                {
                    d[x] = 0;
                    continue;
                }
                const auto clampByte = [](float v) { return std::clamp(int(std::lround(v)), 0, 255); };
                d[x] = Argb(clampByte(a), clampByte(r / a), clampByte(g / a), clampByte(b / a));
            }
        }
        return result;
    }

    Image FitInside(const Image& src, int maxWidth, int maxHeight, bool allowUpscale)
    {
        if (src.Empty() || maxWidth <= 0 || maxHeight <= 0)
        {
            return Image();
        }
        double k = std::min(double(maxWidth) / src.width, double(maxHeight) / src.height);
        if (!allowUpscale)
        {
            k = std::min(k, 1.0);
        }
        const int w = std::max(1, int(std::lround(src.width * k)));
        const int h = std::max(1, int(std::lround(src.height * k)));
        return ResizeSmooth(src, w, h);
    }

    Image Crop(const Image& src, const Rect& r)
    {
        const Rect c = r.Intersect(Rect{0, 0, src.width, src.height});
        Image out(c.w, c.h);
        for (int y = 0; y < c.h; ++y)
        {
            std::memcpy(out.Row(y), src.Row(c.y + y) + c.x, std::size_t(c.w) * sizeof(Color));
        }
        return out;
    }

    Image MakeIcon16(const Image& src)
    {
        Image out(16, 16, 0);
        if (src.Empty())
        {
            return out;
        }
        Image fitted = src;
        if (src.width != 16 || src.height != 16)
        {
            fitted = FitInside(src, 16, 16, true);
        }
        const int ox = (16 - fitted.width) / 2;
        const int oy = (16 - fitted.height) / 2;
        for (int y = 0; y < fitted.height; ++y)
        {
            for (int x = 0; x < fitted.width; ++x)
            {
                const Color c = fitted.Get(x, y);
                if (Alpha(c) >= 110)
                {
                    out.Set(ox + x, oy + y, Opaque(c));
                }
            }
        }
        return out;
    }

    Image Rotate90(const Image& src)
    {
        Image out(src.height, src.width);
        for (int y = 0; y < src.height; ++y)
        {
            for (int x = 0; x < src.width; ++x)
            {
                out.Set(src.height - 1 - y, x, src.Get(x, y));
            }
        }
        return out;
    }

    Image Rotate180(const Image& src)
    {
        Image out(src.width, src.height);
        for (int y = 0; y < src.height; ++y)
        {
            for (int x = 0; x < src.width; ++x)
            {
                out.Set(src.width - 1 - x, src.height - 1 - y, src.Get(x, y));
            }
        }
        return out;
    }

    Image Rotate270(const Image& src)
    {
        Image out(src.height, src.width);
        for (int y = 0; y < src.height; ++y)
        {
            for (int x = 0; x < src.width; ++x)
            {
                out.Set(y, src.width - 1 - x, src.Get(x, y));
            }
        }
        return out;
    }

    Image FlipHorizontal(const Image& src)
    {
        Image out(src.width, src.height);
        for (int y = 0; y < src.height; ++y)
        {
            for (int x = 0; x < src.width; ++x)
            {
                out.Set(src.width - 1 - x, y, src.Get(x, y));
            }
        }
        return out;
    }

    Image FlipVertical(const Image& src)
    {
        Image out(src.width, src.height);
        for (int y = 0; y < src.height; ++y)
        {
            std::memcpy(out.Row(src.height - 1 - y), src.Row(y), std::size_t(src.width) * sizeof(Color));
        }
        return out;
    }

    int GlyphWidth(const Glyph& g)
    {
        return g.empty() ? 0 : int(std::strlen(g[0]));
    }

    int GlyphHeight(const Glyph& g)
    {
        return int(g.size());
    }

    Painter::Painter(Image& target)
        : target_(&target), ox_(0), oy_(0), clip_(Rect{0, 0, target.width, target.height})
    {
    }

    Painter::Painter(Image* target, int ox, int oy, const Rect& clip)
        : target_(target), ox_(ox), oy_(oy), clip_(clip)
    {
    }

    Painter Painter::Sub(const Rect& local) const
    {
        const Rect abs = local.Offset(ox_, oy_);
        return Painter(target_, abs.x, abs.y, abs.Intersect(clip_));
    }

    Painter Painter::Clipped(const Rect& local) const
    {
        return Painter(target_, ox_, oy_, local.Offset(ox_, oy_).Intersect(clip_));
    }

    Rect Painter::LocalClip() const
    {
        return clip_.Offset(-ox_, -oy_);
    }

    void Painter::Fill(int x, int y, int w, int h, Color c)
    {
        const Rect r = Rect{x + ox_, y + oy_, w, h}.Intersect(clip_);
        if (r.Empty())
        {
            return;
        }
        if (Alpha(c) == 255)
        {
            for (int yy = r.y; yy < r.Bottom(); ++yy)
            {
                Color* row = target_->Row(yy);
                std::fill(row + r.x, row + r.Right(), c);
            }
            return;
        }
        for (int yy = r.y; yy < r.Bottom(); ++yy)
        {
            Color* row = target_->Row(yy);
            for (int xx = r.x; xx < r.Right(); ++xx)
            {
                row[xx] = Over(row[xx], c);
            }
        }
    }

    void Painter::Pixel(int x, int y, Color c)
    {
        const int ax = x + ox_;
        const int ay = y + oy_;
        if (clip_.Contains(ax, ay))
        {
            target_->Set(ax, ay, c);
        }
    }

    void Painter::Blend(int x, int y, Color c)
    {
        const int ax = x + ox_;
        const int ay = y + oy_;
        if (clip_.Contains(ax, ay))
        {
            target_->Set(ax, ay, Over(target_->Get(ax, ay), c));
        }
    }

    void Painter::Frame(const Rect& r, Color c)
    {
        HLine(r.x, r.y, r.w, c);
        HLine(r.x, r.Bottom() - 1, r.w, c);
        VLine(r.x, r.y, r.h, c);
        VLine(r.Right() - 1, r.y, r.h, c);
    }

    void Painter::Draw(const Image& src, int x, int y)
    {
        Draw(src, Rect{0, 0, src.width, src.height}, x, y);
    }

    void Painter::Draw(const Image& src, const Rect& from, int x, int y)
    {
        const Rect f = from.Intersect(Rect{0, 0, src.width, src.height});
        const Rect dst = Rect{x + ox_, y + oy_, f.w, f.h}.Intersect(clip_);
        if (dst.Empty())
        {
            return;
        }
        const int sx0 = f.x + (dst.x - (x + ox_));
        const int sy0 = f.y + (dst.y - (y + oy_));
        for (int yy = 0; yy < dst.h; ++yy)
        {
            const Color* s = src.Row(sy0 + yy) + sx0;
            Color* d = target_->Row(dst.y + yy) + dst.x;
            for (int xx = 0; xx < dst.w; ++xx)
            {
                const Color c = s[xx];
                if (Alpha(c) == 255)
                {
                    d[xx] = c;
                }
                else if (Alpha(c) != 0)
                {
                    d[xx] = Over(d[xx], c);
                }
            }
        }
    }

    void Painter::DrawOpaque(const Image& src, int x, int y)
    {
        const Rect dst = Rect{x + ox_, y + oy_, src.width, src.height}.Intersect(clip_);
        if (dst.Empty())
        {
            return;
        }
        const int sx0 = dst.x - (x + ox_);
        const int sy0 = dst.y - (y + oy_);
        for (int yy = 0; yy < dst.h; ++yy)
        {
            const Color* s = src.Row(sy0 + yy) + sx0;
            Color* d = target_->Row(dst.y + yy) + dst.x;
            for (int xx = 0; xx < dst.w; ++xx)
            {
                d[xx] = Opaque(s[xx]);
            }
        }
    }

    void Painter::Edge(int x, int y, int w, int h, Color topLeft, Color bottomRight)
    {
        if (w <= 0 || h <= 0)
        {
            return;
        }
        Fill(x, y, w - 1, 1, topLeft);
        Fill(x, y, 1, h - 1, topLeft);
        Fill(x + w - 1, y, 1, h, bottomRight);
        Fill(x, y + h - 1, w, 1, bottomRight);
    }

    void Painter::GradientH(int x, int y, int w, int h, Color from, Color to)
    {
        for (int i = 0; i < w; ++i)
        {
            const double t = w > 1 ? double(i) / double(w - 1) : 0.0;
            Fill(x + i, y, 1, h, Mix(from, to, t));
        }
    }

    void Painter::DrawGlyph(int x, int y, const Glyph& g, Color c)
    {
        for (std::size_t j = 0; j < g.size(); ++j)
        {
            const char* row = g[j];
            for (int i = 0; row[i] != '\0'; ++i)
            {
                if (row[i] == 'x')
                {
                    Pixel(x + i, y + int(j), c);
                }
            }
        }
    }

    void Painter::Dither(const Rect& r, Color a, Color b)
    {
        const Rect d = r.Offset(ox_, oy_).Intersect(clip_);
        for (int yy = d.y; yy < d.Bottom(); ++yy)
        {
            Color* row = target_->Row(yy);
            for (int xx = d.x; xx < d.Right(); ++xx)
            {
                row[xx] = ((xx + yy) & 1) ? a : b;
            }
        }
    }

    void Painter::Dotted(const Rect& r, Color c)
    {
        for (int i = 0; i < r.w; ++i)
        {
            if (((r.x + i + ox_ + r.y + oy_) & 1) == 0)
            {
                Pixel(r.x + i, r.y, c);
            }
            if (((r.x + i + ox_ + r.Bottom() - 1 + oy_) & 1) == 0)
            {
                Pixel(r.x + i, r.Bottom() - 1, c);
            }
        }
        for (int j = 0; j < r.h; ++j)
        {
            if (((r.x + ox_ + r.y + j + oy_) & 1) == 0)
            {
                Pixel(r.x, r.y + j, c);
            }
            if (((r.Right() - 1 + ox_ + r.y + j + oy_) & 1) == 0)
            {
                Pixel(r.Right() - 1, r.y + j, c);
            }
        }
    }

    void Painter::Text(int x, int y, const std::wstring& s, const Font& f, Color c)
    {
        if (s.empty() || clip_.Empty())
        {
            return;
        }
        rf::Text().Draw(*target_, clip_, x + ox_, y + oy_, s, f, c);
    }

    void Painter::TextEmbossed(int x, int y, const std::wstring& s, const Font& f, const Shades& sh)
    {
        Text(x + 1, y + 1, s, f, sh.hilite);
        Text(x, y, s, f, sh.shadow);
    }

    void Painter::TextUnderlined(int x, int y, const std::wstring& s, int underline, const Font& f, Color c)
    {
        Text(x, y, s, f, c);
        if (underline < 0 || underline >= int(s.size()))
        {
            return;
        }
        TextEngine& e = rf::Text();
        const int ux = x + e.Width(s.substr(0, std::size_t(underline)), f);
        const int uw = std::max(1, e.Width(s.substr(std::size_t(underline), 1), f) - 1);
        HLine(ux, y + e.Ascent(f) + 1, uw, c);
    }

    void Painter::Raised(const Rect& r, const Shades& sh, bool fill)
    {
        if (fill)
        {
            Fill(r, sh.face);
        }
        Edge(r, sh.hilite, sh.dark);
        Edge(r.Inflate(-1), sh.light, sh.shadow);
    }

    void Painter::Pressed(const Rect& r, const Shades& sh, bool fill)
    {
        if (fill)
        {
            Fill(r, sh.face);
        }
        Edge(r, sh.dark, sh.hilite);
        Edge(r.Inflate(-1), sh.shadow, sh.light);
    }

    void Painter::Sunken(const Rect& r, const Shades& sh, const Color* fill)
    {
        if (fill != nullptr)
        {
            Fill(r, *fill);
        }
        Edge(r, sh.shadow, sh.hilite);
        Edge(r.Inflate(-1), sh.dark, sh.light);
    }

    void Painter::ThinRaised(const Rect& r, const Shades& sh)
    {
        Edge(r, sh.hilite, sh.shadow);
    }

    void Painter::ThinSunken(const Rect& r, const Shades& sh)
    {
        Edge(r, sh.shadow, sh.hilite);
    }

    void Painter::WindowEdge(const Rect& r, const Shades& sh)
    {
        Edge(r, sh.light, sh.dark);
        Edge(r.Inflate(-1), sh.hilite, sh.shadow);
    }

    void Painter::EtchedH(int x, int y, int w, const Shades& sh)
    {
        HLine(x, y, w, sh.shadow);
        HLine(x, y + 1, w, sh.hilite);
    }

    void Painter::EtchedV(int x, int y, int h, const Shades& sh)
    {
        VLine(x, y, h, sh.shadow);
        VLine(x + 1, y, h, sh.hilite);
    }

    void Painter::EtchedFrame(const Rect& r, const Shades& sh)
    {
        Frame(Rect{r.x + 1, r.y + 1, r.w - 1, r.h - 1}, sh.hilite);
        Frame(Rect{r.x, r.y, r.w - 1, r.h - 1}, sh.shadow);
    }
}
