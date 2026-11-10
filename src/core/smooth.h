#pragma once

#include "paint.h"

#include <functional>
#include <utility>
#include <vector>

namespace rf
{
    struct Box
    {
        double x = 0.0;
        double y = 0.0;
        double w = 0.0;
        double h = 0.0;
    };

    using Shade = std::function<Color(double fx, double fy)>;

    Color WithAlpha(Color c, double alpha);
    Color Ramp(const std::vector<std::pair<double, Color>>& stops, double t);
    void BlendPixel(Image& img, int x, int y, Color c, double cover);
    void FillRound(Image& img, const Box& b, double radius, const Shade& shade);
    void FillRound(Image& img, const Box& b, double radius, Color c);
    void FillRoundRows(Image& img, const Box& b, double radius, const std::function<Color(double fy)>& rowColor);
    void StrokeRound(Image& img, const Box& b, double radius, double width, Color c);
    void FillDisc(Image& img, double cx, double cy, double r, const Shade& shade);
    void FillDisc(Image& img, double cx, double cy, double r, Color c);
    void StrokeLine(Image& img, double x0, double y0, double x1, double y1, double width, Color c);
    void StrokeArc(Image& img, double cx, double cy, double r, double a0, double a1, double width, Color c);
    void FillPolygon(Image& img, const std::vector<std::pair<double, double>>& points, Color c);
    void FillGlow(Image& img, double cx, double cy, double rx, double ry, Color c);
}
