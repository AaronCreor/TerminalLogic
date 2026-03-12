#pragma once

#include <string>

struct Color
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct Rect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool contains(float px, float py) const
    {
        return px >= x && py >= y && px <= x + w && py <= y + h;
    }
};

struct Theme
{
    Color backgroundTop;
    Color backgroundBottom;
    Color panelFill;
    Color panelFillAlt;
    Color border;
    Color accent;
    Color text;
    Color textMuted;
    Color warning;
    Color success;

    static Theme amber();
    static Theme green();
};

class Renderer
{
public:
    void resize(int width, int height);
    void beginFrame(const Theme& theme);
    void endFrame(bool crtEnabled, bool scanlinesEnabled, bool curvatureEnabled);

    void drawRect(const Rect& rect, const Color& color) const;
    void drawOutline(const Rect& rect, const Color& color, float thickness = 1.0f) const;
    void drawLine(float x1, float y1, float x2, float y2, const Color& color, float thickness = 1.0f) const;
    void drawText(float x, float y, const std::string& text, float scale, const Color& color) const;

    int width() const { return width_; }
    int height() const { return height_; }
    float glyphWidth(float scale) const { return 8.0f * scale; }
    float glyphHeight(float scale) const { return 8.0f * scale; }

private:
    void drawScanlines(const Theme& theme) const;
    void drawCurvatureMask() const;

    int width_ = 1280;
    int height_ = 720;
    Theme currentTheme_ = Theme::amber();
};
