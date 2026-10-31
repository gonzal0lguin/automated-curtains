// TEST: GC9A01 round display + backlight
//   ./scripts/test.sh display
//
// Pages change every 4 s (or press 'n' + Enter):
//   1 colors   - top RED, right GREEN, bottom BLUE, left WHITE.
//                Colors negative?  -> flip cfg.invert in lib/display/lgfx_gc9a01.h
//                Red/blue swapped? -> flip cfg.rgb_order
//   2 fonts    - all anti-aliased fonts
//   3 icons    - every icon glyph
//   4 gauge    - animated arcs, prints the frame time (FPS) on serial
//   5 backlight- fades the backlight 0 -> 100% -> 0
#include <Arduino.h>

#include "display.h"
#include "ui_theme.h"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

Display display;
int page = 0;
uint32_t pageStart = 0;
const int PAGES = 5;

static const char* ICONS[] = {
    ICON_CHECK, ICON_TIMES, ICON_POWER, ICON_SIGNAL, ICON_COG, ICON_HOME, ICON_CLOCK, ICON_REFRESH, ICON_LIST,
    ICON_MARKER, ICON_TINT, ICON_PLAY, ICON_PAUSE, ICON_STOP, ICON_LEFT, ICON_RIGHT, ICON_INFO, ICON_CROSSHAIRS,
    ICON_ARROW_UP, ICON_ARROW_DOWN, ICON_PLUS, ICON_MINUS, ICON_WARNING, ICON_CALENDAR, ICON_MAGNET, ICON_UP,
    ICON_DOWN, ICON_UPDOWN, ICON_GLOBE, ICON_WRENCH, ICON_CLOUD, ICON_SAVE, ICON_UNDO, ICON_GAUGE, ICON_BOLT,
    ICON_UMBRELLA, ICON_BULB, ICON_DOWNLOAD, ICON_BELL, ICON_COFFEE, ICON_SUN, ICON_MOON, ICON_SLIDERS, ICON_WIFI,
    ICON_BUS, ICON_BED, ICON_HOURGLASS, ICON_THERMO, ICON_CHIP, ICON_SNOW};

static void drawPage(Canvas& c, float t) {
    c.clear(theme::BG);
    switch (page) {
        case 0:
            c.fillRect(0, 0, 240, 80, 0xFF0000);
            c.fillRect(160, 80, 80, 80, 0x00FF00);
            c.fillRect(0, 160, 240, 80, 0x0000FF);
            c.fillRect(0, 80, 80, 80, 0xFFFFFF);
            c.text("RED", 120, 50, Font::Title, 0x000000);
            c.text("GREEN", 200, 120, Font::Small, 0x000000);
            c.text("BLUE", 120, 190, Font::Title, 0xFFFFFF);
            c.text("WHITE", 40, 120, Font::Small, 0x000000);
            c.text("colors", 120, 120, Font::Body, theme::TEXT);
            break;
        case 1:
            c.text("Small 16px", 120, 40, Font::Small, theme::MUTED);
            c.text("Body 21px", 120, 66, Font::Body, theme::TEXT);
            c.text("Title 28px", 120, 98, Font::Title, theme::ACCENT);
            c.text("12:34", 120, 152, Font::Big, theme::TEXT);
            c.text("áéíóú ñ ¿? 23°", 120, 204, Font::Body, theme::SKY);
            break;
        case 2: {
            int n = sizeof(ICONS) / sizeof(ICONS[0]);
            for (int i = 0; i < n; i++) {
                int col = i % 8, row = i / 8;
                c.text(ICONS[i], 36 + col * 24, 52 + row * 26, Font::Icon, i % 3 ? theme::TEXT : theme::ACCENT);
            }
            c.text(ICON_SUN, 120, 214, Font::IconBig, theme::ACCENT);
            break;
        }
        case 3: {
            float p = (sinf(t * 1.5f) + 1) * 50;
            c.arc(120, 120, 118, 8, 225, 270, theme::TRACK);
            c.arc(120, 120, 118, 8, 225, 270 * p / 100, theme::ACCENT);
            c.ring(120, 120, 100, 3, theme::SURFACE);
            c.arc(120, 120, 100, 3, fmodf(t * 200, 360), 60, theme::SKY);
            char b[16];
            snprintf(b, sizeof(b), "%d", (int)p);
            c.text(b, 120, 112, Font::Big, theme::TEXT);
            snprintf(b, sizeof(b), "%lu ms/frame", (unsigned long)display.lastFrameMs());
            c.text(b, 120, 170, Font::Small, theme::MUTED);
            break;
        }
        case 4:
            c.ring(120, 120, 118, 6, theme::ACCENT);
            c.text(ICON_BULB, 120, 90, Font::IconBig, theme::ACCENT);
            c.text("backlight fade", 120, 150, Font::Body, theme::TEXT);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== DISPLAY TEST ===");
    Serial.printf("SCLK=%d MOSI=%d CS=%d DC=%d RST=%d BL=%d @ %d MHz\n", PIN_TFT_SCLK, PIN_TFT_MOSI, PIN_TFT_CS,
                  PIN_TFT_DC, PIN_TFT_RST, PIN_TFT_BL, TFT_SPI_HZ / 1000000);
    if (!display.begin(0)) Serial.println("display init FAILED (sprite allocation or fonts)");
    display.setBrightness(220, 0);
    Serial.printf("free heap after init: %lu\n", (unsigned long)ESP.getFreeHeap());
    pageStart = millis();
}

void loop() {
    if (Serial.available()) {
        char ch = Serial.read();
        if (ch == 'n') {
            page = (page + 1) % PAGES;
            pageStart = millis();
        }
    }
    if (millis() - pageStart > 4000) {
        page = (page + 1) % PAGES;
        pageStart = millis();
        display.setBrightness(220, 0);
        Serial.printf("page %d\n", page + 1);
    }
    float t = millis() / 1000.0f;
    if (page == 4) {
        float k = (millis() - pageStart) / 4000.0f;
        display.setBrightness((uint8_t)(255 * (k < 0.5f ? k * 2 : (1 - k) * 2)), 0);
    }
    display.render([t](Canvas& c) { drawPage(c, t); });
    static uint32_t lastLog = 0;
    if (page == 3 && millis() - lastLog > 1000) {
        lastLog = millis();
        Serial.printf("frame %lu ms (%.0f fps)\n", (unsigned long)display.lastFrameMs(),
                      1000.0f / max<uint32_t>(1, display.lastFrameMs()));
    }
    delay(page == 3 ? 1 : 50);
}
