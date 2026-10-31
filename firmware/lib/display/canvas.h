#pragma once
// Drawing surface for the UI.
//
// A full 240x240 RGB565 frame (115 KB) does not fit in one block of the plain
// ESP32 heap, so frames are rendered in horizontal strips: the same draw
// callback runs once per strip into a 240 x STRIP_H sprite, and Canvas shifts
// every y coordinate by the strip offset. Draw code just uses screen
// coordinates (0..239) and never needs to know about strips.
//
// Colors are RGB888 (0xRRGGBB). Angles: degrees, 0 = 12 o'clock, clockwise.
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

enum class Font : uint8_t { Small, Body, Title, Big, Icon, IconBig, COUNT };

using Datum = lgfx::textdatum_t;

class Canvas {
public:
    static constexpr int W = 240, H = 240, CX = 120, CY = 120;

    bool begin(int stripHeight);
    void beginStrip(int y0);          // called by Display before each strip
    LGFX_Sprite& sprite() { return _spr; }
    int stripTop() const { return _oy; }
    int stripHeight() const { return _sh; }
    // true if any of rows [y0, y1] fall into the current strip (skip work otherwise)
    bool rowsVisible(int y0, int y1) const { return y1 >= _oy && y0 < _oy + _sh; }

    // --- primitives
    void clear(uint32_t c);
    void fillRect(int x, int y, int w, int h, uint32_t c);
    void fillRectAlpha(int x, int y, int w, int h, uint8_t alpha, uint32_t c);
    void fillRoundRect(int x, int y, int w, int h, int r, uint32_t c);   // anti-aliased
    void drawRoundRect(int x, int y, int w, int h, int r, uint32_t c);
    void fillCircle(int x, int y, int r, uint32_t c);                    // anti-aliased
    void drawCircle(int x, int y, int r, uint32_t c);
    void line(float x0, float y0, float x1, float y1, float width, uint32_t c);  // AA, round caps
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c);
    // Anti-aliased ring segment between radius rOuter-thickness and rOuter,
    // starting at startDeg and sweeping spanDeg clockwise.
    void arc(int cx, int cy, int rOuter, int thickness, float startDeg, float spanDeg, uint32_t c,
             bool roundCaps = true);
    void ring(int cx, int cy, int rOuter, int thickness, uint32_t c) { arc(cx, cy, rOuter, thickness, 0, 360, c, false); }

    // --- text (UTF-8)
    int text(const char* s, int x, int y, Font f, uint32_t color, Datum d = lgfx::middle_center);
    // Like text() but shortens with "..." to fit maxWidth.
    int textFit(const char* s, int x, int y, int maxWidth, Font f, uint32_t color, Datum d = lgfx::middle_center);
    int textWidth(const char* s, Font f);
    int fontHeight(Font f);

    // Point on a circle (angle 0 = top, clockwise)
    static float px(int cx, float r, float deg);
    static float py(int cy, float r, float deg);
    static uint32_t mix(uint32_t a, uint32_t b, float t);  // linear blend a->b

private:
    void setFont(Font f);
    void blendPixel(int x, int y, uint32_t c, float alpha);

    LGFX_Sprite _spr;
    int _oy = 0, _sh = 0;
};

bool fontsLoad();  // loads the embedded VLW fonts (called by Canvas::begin)
const lgfx::IFont* fontPtr(Font f);
