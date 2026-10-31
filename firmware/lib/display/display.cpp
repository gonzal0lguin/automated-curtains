#include "display.h"

bool Display::begin(uint8_t rotation) {
    if (!_lcd.init()) return false;
    _lcd.setRotation(rotation & 3);
    _lcd.setBrightness(0);
    _lcd.fillScreen((uint32_t)0x000000);
    return _canvas.begin(STRIP_H);
}

void Display::setRotation(uint8_t r) { _lcd.setRotation(r & 3); }

void Display::setBrightness(uint8_t level, uint16_t fadeMs) {
    if (level == _target) return;
    _target = level;
    _from = _level;
    _fadeStart = millis();
    _fadeMs = fadeMs;
    if (!fadeMs) {
        _level = level;
        _lcd.setBrightness(level);
    }
}

void Display::tick() {
    if ((uint8_t)lroundf(_level) == _target) return;
    float t = _fadeMs ? (float)(millis() - _fadeStart) / _fadeMs : 1.0f;
    if (t >= 1) t = 1;
    // ease-out so fades feel natural to the eye
    float e = 1 - (1 - t) * (1 - t);
    _level = _from + (_target - _from) * e;
    if (t >= 1) _level = _target;
    _lcd.setBrightness((uint8_t)lroundf(_level));
}

void Display::render(const std::function<void(Canvas&)>& draw, const RowSink& rowSink) {
    uint32_t t0 = millis();
    _lcd.startWrite();
    for (int y = 0; y < Canvas::H; y += STRIP_H) {
        _canvas.beginStrip(y);
        draw(_canvas);
        _canvas.sprite().pushSprite(&_lcd, 0, y);
        // pushSprite hands the sprite buffer straight to the SPI DMA engine
        // (zero copy). There is only one strip buffer, so the transfer has to
        // finish before the next strip draws into it - otherwise the strip
        // still in flight gets overwritten mid-transfer and shows a mix of
        // both strips.
        _lcd.waitDMA();
        if (rowSink) {
            const uint8_t* buf = (const uint8_t*)_canvas.sprite().getBuffer();
            for (int r = 0; r < STRIP_H; r++) rowSink(y + r, buf + r * Canvas::W * 2, Canvas::W * 2);
        }
    }
    _lcd.endWrite();
    _frameMs = millis() - t0;
}
