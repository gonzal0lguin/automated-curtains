#include "canvas.h"

#include <math.h>

static constexpr float DEG = (float)M_PI / 180.0f;

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

bool Canvas::begin(int stripHeight) {
    _sh = stripHeight;
    _spr.setColorDepth(16);
    if (!_spr.createSprite(W, stripHeight)) return false;
    _spr.setTextWrap(false);
    return fontsLoad();
}

void Canvas::beginStrip(int y0) {
    _oy = y0;
    _spr.clearClipRect();
}

float Canvas::px(int cx, float r, float deg) { return cx + r * sinf(deg * DEG); }
float Canvas::py(int cy, float r, float deg) { return cy - r * cosf(deg * DEG); }

uint32_t Canvas::mix(uint32_t a, uint32_t b, float t) {
    t = clamp01(t);
    auto ch = [&](int s) {
        int ca = (a >> s) & 0xFF, cb = (b >> s) & 0xFF;
        return (uint32_t)(ca + (cb - ca) * t + 0.5f) << s;
    };
    return ch(16) | ch(8) | ch(0);
}

void Canvas::clear(uint32_t c) { _spr.fillScreen(c); }

void Canvas::fillRect(int x, int y, int w, int h, uint32_t c) {
    if (rowsVisible(y, y + h - 1)) _spr.fillRect(x, y - _oy, w, h, c);
}

void Canvas::fillRectAlpha(int x, int y, int w, int h, uint8_t alpha, uint32_t c) {
    if (rowsVisible(y, y + h - 1)) _spr.fillRectAlpha(x, y - _oy, w, h, alpha, c);
}

void Canvas::fillRoundRect(int x, int y, int w, int h, int r, uint32_t c) {
    if (rowsVisible(y, y + h - 1)) _spr.fillSmoothRoundRect(x, y - _oy, w, h, r, c);
}

void Canvas::drawRoundRect(int x, int y, int w, int h, int r, uint32_t c) {
    if (rowsVisible(y, y + h - 1)) _spr.drawRoundRect(x, y - _oy, w, h, r, c);
}

void Canvas::fillCircle(int x, int y, int r, uint32_t c) {
    if (rowsVisible(y - r, y + r)) _spr.fillSmoothCircle(x, y - _oy, r, c);
}

void Canvas::drawCircle(int x, int y, int r, uint32_t c) {
    if (rowsVisible(y - r, y + r)) _spr.drawCircle(x, y - _oy, r, c);
}

void Canvas::line(float x0, float y0, float x1, float y1, float width, uint32_t c) {
    float r = width / 2;
    if (!rowsVisible((int)(fminf(y0, y1) - r - 1), (int)(fmaxf(y0, y1) + r + 1))) return;
    _spr.drawWideLine((int)lroundf(x0), (int)lroundf(y0) - _oy, (int)lroundf(x1), (int)lroundf(y1) - _oy, r, c);
}

void Canvas::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
    _spr.fillTriangle(x0, y0 - _oy, x1, y1 - _oy, x2, y2 - _oy, c);
}

void Canvas::blendPixel(int x, int y, uint32_t c, float alpha) {
    if (alpha <= 0.004f) return;
    if (alpha >= 0.996f) _spr.drawPixel(x, y - _oy, c);
    else _spr.fillRectAlpha(x, y - _oy, 1, 1, (uint8_t)(alpha * 255.0f + 0.5f), c);
}

void Canvas::arc(int cx, int cy, int rOuter, int thickness, float a0, float span, uint32_t c, bool roundCaps) {
    if (span <= 0.05f) return;
    if (span > 360.0f) span = 360.0f;
    bool full = span >= 359.95f;
    if (full) roundCaps = false;
    a0 = fmodf(a0, 360.0f);
    if (a0 < 0) a0 += 360.0f;

    const float ro = rOuter, ri = rOuter - thickness;
    const float rMid = (ro + ri) / 2, capR = thickness / 2.0f;

    int yStart = std::max(cy - rOuter - 1, _oy);
    int yEnd = std::min(cy + rOuter + 1, _oy + _sh - 1);
    for (int y = yStart; y <= yEnd; y++) {
        float dy = (float)(y - cy);
        float xo2 = (ro + 1) * (ro + 1) - dy * dy;
        if (xo2 < 0) continue;
        int xo = (int)ceilf(sqrtf(xo2));
        float xi2 = (ri - 1) * (ri - 1) - dy * dy;
        int xi = xi2 > 0 ? (int)floorf(sqrtf(xi2)) : -1;  // pixels with |dx| < xi are inside the hole

        // two runs: left [-xo, -xi] and right [xi, xo] (or one run if no hole on this row)
        for (int side = 0; side < 2; side++) {
            int from, to;
            if (xi < 0) {
                if (side) break;
                from = -xo;
                to = xo;
            } else if (side == 0) {
                from = -xo;
                to = -xi;
            } else {
                from = xi;
                to = xo;
            }
            int runStart = INT32_MIN;  // start of current fully-covered run
            for (int dx = from; dx <= to + 1; dx++) {
                float alpha = 0;
                if (dx <= to) {
                    float r = sqrtf((float)(dx * dx) + dy * dy);
                    alpha = clamp01(ro - r + 0.5f) * clamp01(r - ri + 0.5f);
                    if (alpha > 0 && !full) {
                        float ang = atan2f((float)dx, -dy) / DEG;
                        if (ang < 0) ang += 360.0f;
                        float rel = ang - a0;
                        if (rel < 0) rel += 360.0f;
                        if (rel <= span) {
                            float edge = fminf(rel, span - rel) * DEG * r;  // px to nearest end
                            alpha *= roundCaps ? 1.0f : clamp01(edge + 0.5f);
                        } else {
                            float outside = fminf(rel - span, 360.0f - rel) * DEG * r;
                            alpha *= roundCaps ? 0.0f : clamp01(0.5f - outside);
                        }
                    }
                }
                if (alpha >= 0.996f) {
                    if (runStart == INT32_MIN) runStart = dx;
                    continue;
                }
                if (runStart != INT32_MIN) {
                    _spr.drawFastHLine(cx + runStart, y - _oy, dx - runStart, c);
                    runStart = INT32_MIN;
                }
                if (dx <= to) blendPixel(cx + dx, y, c, alpha);
            }
        }
    }
    if (roundCaps) {
        int r = (int)lroundf(capR);
        fillCircle((int)lroundf(px(cx, rMid, a0)), (int)lroundf(py(cy, rMid, a0)), r, c);
        fillCircle((int)lroundf(px(cx, rMid, a0 + span)), (int)lroundf(py(cy, rMid, a0 + span)), r, c);
    }
}

void Canvas::setFont(Font f) { _spr.setFont(fontPtr(f)); }

int Canvas::fontHeight(Font f) {
    setFont(f);
    return _spr.fontHeight();
}

int Canvas::textWidth(const char* s, Font f) {
    setFont(f);
    return _spr.textWidth(s);
}

int Canvas::text(const char* s, int x, int y, Font f, uint32_t color, Datum d) {
    setFont(f);
    int h = _spr.fontHeight();
    // conservative visibility test (datum may be top, middle or bottom)
    if (!rowsVisible(y - h - 2, y + h + 2)) return _spr.textWidth(s);
    _spr.setTextColor(color);
    _spr.setTextDatum(d);
    return _spr.drawString(s, x, y - _oy);
}

int Canvas::textFit(const char* s, int x, int y, int maxWidth, Font f, uint32_t color, Datum d) {
    if (textWidth(s, f) <= maxWidth) return text(s, x, y, f, color, d);
    char buf[96];
    strlcpy(buf, s, sizeof(buf) - 4);
    size_t n = strlen(buf);
    while (n > 0) {
        // drop one UTF-8 character
        do { n--; } while (n > 0 && (buf[n] & 0xC0) == 0x80);
        while (n > 0 && buf[n - 1] == ' ') n--;
        strcpy(buf + n, "...");
        if (textWidth(buf, f) <= maxWidth) break;
    }
    return text(buf, x, y, f, color, d);
}
