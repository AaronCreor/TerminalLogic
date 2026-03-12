#include "render/Renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace
{
constexpr std::array<std::array<std::uint8_t, 8>, 128> kFont = {{
    {{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},{{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0}},{{24,24,24,24,24,0,24,0}},{{54,54,20,0,0,0,0,0}},{{54,54,127,54,127,54,54,0}},
    {{8,62,40,62,11,62,8,0}},{{99,103,14,28,56,115,99,0}},{{28,54,28,59,102,102,59,0}},{{24,24,48,0,0,0,0,0}},
    {{12,24,48,48,48,24,12,0}},{{48,24,12,12,12,24,48,0}},{{0,54,28,127,28,54,0,0}},{{0,24,24,126,24,24,0,0}},
    {{0,0,0,0,0,24,24,48}},{{0,0,0,126,0,0,0,0}},{{0,0,0,0,0,24,24,0}},{{3,6,12,24,48,96,64,0}},
    {{62,99,107,107,99,99,62,0}},{{24,56,24,24,24,24,126,0}},{{62,99,3,6,24,48,127,0}},{{62,99,3,30,3,99,62,0}},
    {{6,14,30,54,127,6,6,0}},{{127,96,126,3,3,99,62,0}},{{30,48,96,126,99,99,62,0}},{{127,99,6,12,24,24,24,0}},
    {{62,99,99,62,99,99,62,0}},{{62,99,99,63,3,6,60,0}},{{0,24,24,0,0,24,24,0}},{{0,24,24,0,0,24,24,48}},
    {{6,12,24,48,24,12,6,0}},{{0,0,126,0,126,0,0,0}},{{48,24,12,6,12,24,48,0}},{{62,99,6,12,24,0,24,0}},
    {{62,99,123,123,123,96,62,0}},{{24,60,102,102,126,102,102,0}},{{124,102,102,124,102,102,124,0}},{{60,102,96,96,96,102,60,0}},
    {{120,108,102,102,102,108,120,0}},{{126,96,96,124,96,96,126,0}},{{126,96,96,124,96,96,96,0}},{{60,102,96,110,102,102,62,0}},
    {{102,102,102,126,102,102,102,0}},{{60,24,24,24,24,24,60,0}},{{30,12,12,12,12,108,56,0}},{{102,108,120,112,120,108,102,0}},
    {{96,96,96,96,96,96,126,0}},{{99,119,127,107,99,99,99,0}},{{102,118,126,126,110,102,102,0}},{{60,102,102,102,102,102,60,0}},
    {{124,102,102,124,96,96,96,0}},{{60,102,102,102,110,60,14,0}},{{124,102,102,124,120,108,102,0}},{{62,96,96,60,6,6,124,0}},
    {{126,24,24,24,24,24,24,0}},{{102,102,102,102,102,102,60,0}},{{102,102,102,102,102,60,24,0}},{{99,99,99,107,127,119,99,0}},
    {{99,99,54,28,54,99,99,0}},{{102,102,102,60,24,24,24,0}},{{127,7,14,28,56,112,127,0}},{{60,48,48,48,48,48,60,0}},
    {{64,96,48,24,12,6,3,0}},{{60,12,12,12,12,12,60,0}},{{8,28,54,99,0,0,0,0}},{{0,0,0,0,0,0,0,255}},
    {{24,24,12,0,0,0,0,0}},{{0,0,60,6,62,102,59,0}},{{96,96,124,102,102,102,124,0}},{{0,0,60,102,96,102,60,0}},
    {{6,6,62,102,102,102,62,0}},{{0,0,60,102,126,96,60,0}},{{14,24,24,126,24,24,24,0}},{{0,0,62,102,102,62,6,124}},
    {{96,96,124,102,102,102,102,0}},{{24,0,56,24,24,24,60,0}},{{6,0,6,6,6,102,102,60}},{{96,96,102,108,120,108,102,0}},
    {{56,24,24,24,24,24,60,0}},{{0,0,118,107,107,99,99,0}},{{0,0,124,102,102,102,102,0}},{{0,0,60,102,102,102,60,0}},
    {{0,0,124,102,102,124,96,96}},{{0,0,62,102,102,62,6,6}},{{0,0,108,118,96,96,96,0}},{{0,0,62,96,60,6,124,0}},
    {{24,24,126,24,24,24,14,0}},{{0,0,102,102,102,102,62,0}},{{0,0,102,102,102,60,24,0}},{{0,0,99,99,107,127,54,0}},
    {{0,0,102,60,24,60,102,0}},{{0,0,102,102,102,62,6,124}},{{0,0,126,12,24,48,126,0}},{{14,24,24,112,24,24,14,0}},
    {{24,24,24,0,24,24,24,0}},{{112,24,24,14,24,24,112,0}},{{59,110,0,0,0,0,0,0}},{{0,8,28,54,99,99,127,0}}
}};

void setColor(const Color& color)
{
    glColor4f(color.r, color.g, color.b, color.a);
}
}

Theme Theme::amber()
{
    return {
        {0.02f, 0.015f, 0.01f, 1.0f},
        {0.08f, 0.04f, 0.015f, 1.0f},
        {0.10f, 0.06f, 0.02f, 0.92f},
        {0.08f, 0.04f, 0.015f, 0.92f},
        {0.95f, 0.55f, 0.18f, 1.0f},
        {1.0f, 0.72f, 0.32f, 1.0f},
        {1.0f, 0.82f, 0.56f, 1.0f},
        {0.85f, 0.60f, 0.36f, 1.0f},
        {1.0f, 0.46f, 0.28f, 1.0f},
        {0.52f, 1.0f, 0.65f, 1.0f},
    };
}

Theme Theme::green()
{
    return {
        {0.01f, 0.02f, 0.015f, 1.0f},
        {0.01f, 0.07f, 0.04f, 1.0f},
        {0.03f, 0.10f, 0.07f, 0.92f},
        {0.02f, 0.08f, 0.05f, 0.92f},
        {0.35f, 0.95f, 0.62f, 1.0f},
        {0.62f, 1.0f, 0.82f, 1.0f},
        {0.78f, 1.0f, 0.90f, 1.0f},
        {0.44f, 0.78f, 0.63f, 1.0f},
        {1.0f, 0.56f, 0.40f, 1.0f},
        {0.56f, 1.0f, 0.74f, 1.0f},
    };
}

void Renderer::resize(int width, int height)
{
    width_ = width;
    height_ = height;
    glViewport(0, 0, width_, height_);
}

void Renderer::beginFrame(const Theme& theme)
{
    currentTheme_ = theme;

    glViewport(0, 0, width_, height_);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width_, height_, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(theme.backgroundBottom.r, theme.backgroundBottom.g, theme.backgroundBottom.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_QUADS);
    setColor(theme.backgroundTop);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(width_), 0.0f);
    setColor(theme.backgroundBottom);
    glVertex2f(static_cast<float>(width_), static_cast<float>(height_));
    glVertex2f(0.0f, static_cast<float>(height_));
    glEnd();
}

void Renderer::endFrame(bool crtEnabled, bool scanlinesEnabled)
{
    if (crtEnabled)
    {
        drawGlow(currentTheme_);
        drawVignette(currentTheme_);
    }
    if (crtEnabled && scanlinesEnabled)
    {
        drawScanlines(currentTheme_);
    }
}

void Renderer::drawRect(const Rect& rect, const Color& color) const
{
    glBegin(GL_QUADS);
    setColor(color);
    glVertex2f(rect.x, rect.y);
    glVertex2f(rect.x + rect.w, rect.y);
    glVertex2f(rect.x + rect.w, rect.y + rect.h);
    glVertex2f(rect.x, rect.y + rect.h);
    glEnd();
}

void Renderer::drawOutline(const Rect& rect, const Color& color, float thickness) const
{
    drawRect({rect.x, rect.y, rect.w, thickness}, color);
    drawRect({rect.x, rect.y + rect.h - thickness, rect.w, thickness}, color);
    drawRect({rect.x, rect.y, thickness, rect.h}, color);
    drawRect({rect.x + rect.w - thickness, rect.y, thickness, rect.h}, color);
}

void Renderer::drawLine(float x1, float y1, float x2, float y2, const Color& color, float thickness) const
{
    glLineWidth(thickness);
    glBegin(GL_LINES);
    setColor(color);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void Renderer::drawText(float x, float y, const std::string& text, float scale, const Color& color) const
{
    float cursorX = x;
    float cursorY = y;
    for (unsigned char ch : text)
    {
        if (ch == '\n')
        {
            cursorX = x;
            cursorY += glyphHeight(scale) + scale;
            continue;
        }
        if (ch >= kFont.size())
        {
            ch = '?';
        }

        const auto& glyph = kFont[ch];
        for (int row = 0; row < 8; ++row)
        {
            for (int col = 0; col < 8; ++col)
            {
                if ((glyph[row] & (1u << col)) != 0)
                {
                    drawRect({cursorX + (7 - col) * scale, cursorY + row * scale, scale, scale}, color);
                }
            }
        }
        cursorX += glyphWidth(scale);
    }
}

void Renderer::drawScanlines(const Theme& theme) const
{
    const Color lineColor{theme.backgroundBottom.r, theme.backgroundBottom.g, theme.backgroundBottom.b, 0.24f};
    for (int y = 0; y < height_; y += 3)
    {
        drawRect({0.0f, static_cast<float>(y), static_cast<float>(width_), 1.0f}, lineColor);
    }
}

void Renderer::drawGlow(const Theme& theme) const
{
    const Color glowTop{theme.accent.r, theme.accent.g, theme.accent.b, 0.05f};
    const Color glowBottom{theme.text.r, theme.text.g, theme.text.b, 0.03f};
    drawRect({0.0f, 0.0f, static_cast<float>(width_), 16.0f}, glowTop);
    drawRect({0.0f, static_cast<float>(height_ - 16), static_cast<float>(width_), 16.0f}, glowBottom);
}

void Renderer::drawVignette(const Theme& theme) const
{
    const Color edge{theme.backgroundTop.r, theme.backgroundTop.g, theme.backgroundTop.b, 0.16f};
    drawRect({0.0f, 0.0f, static_cast<float>(width_), 22.0f}, edge);
    drawRect({0.0f, static_cast<float>(height_ - 22), static_cast<float>(width_), 22.0f}, edge);
    drawRect({0.0f, 0.0f, 18.0f, static_cast<float>(height_)}, edge);
    drawRect({static_cast<float>(width_ - 18), 0.0f, 18.0f, static_cast<float>(height_)}, edge);
}
