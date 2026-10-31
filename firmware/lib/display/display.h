#pragma once
#include <functional>

#include "canvas.h"
#include "icons.h"
#include "lgfx_gc9a01.h"

class Display {
public:
    static constexpr int STRIP_H = 120;  // 2 strips per frame, 57.6 KB buffer

    bool begin(uint8_t rotation = 0);
    void setRotation(uint8_t r);

    // Smoothly fades the backlight to `level` (0-255). Call tick() regularly.
    void setBrightness(uint8_t level, uint16_t fadeMs = 250);
    uint8_t brightness() const { return _target; }
    void tick();

    // Renders one frame: `draw` is called once per strip with the canvas.
    // If `rowSink` is given, every rendered row (raw sprite bytes: big-endian
    // RGB565, 480 bytes) is also passed to it - used for serial screenshots.
    using RowSink = std::function<void(int y, const uint8_t* row, size_t len)>;
    void render(const std::function<void(Canvas&)>& draw, const RowSink& rowSink = nullptr);
    uint32_t lastFrameMs() const { return _frameMs; }

    LGFX_GC9A01& lcd() { return _lcd; }
    Canvas& canvas() { return _canvas; }

private:
    LGFX_GC9A01 _lcd;
    Canvas _canvas;
    uint8_t _target = 0;
    float _level = 0, _from = 0;
    uint32_t _fadeStart = 0;
    uint16_t _fadeMs = 0;
    uint32_t _frameMs = 0;
};
