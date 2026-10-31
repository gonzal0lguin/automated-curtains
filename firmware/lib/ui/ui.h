#pragma once
// Knob + round display user interface.
//
//   Home (clock/weather/position)
//     turn  -> position dial (blinds follow the knob)
//     click -> main menu            long press -> open/close toggle (or stop)
//   Menus: turn to scroll, click to select, long press = back.
#include <Arduino.h>

#include "display.h"
#include "rotary_encoder.h"
#include "ui_theme.h"

class Screen {
public:
    virtual ~Screen() {}
    virtual void enter() {}                       // became the visible screen
    virtual void onTurn(int detents, int accel) {} // accel: 1 normal, >1 when spinning fast
    virtual void onClick() {}
    virtual void onLongPress();                   // default: back
    virtual void update() {}                      // every loop, while visible
    virtual void draw(Canvas& c) = 0;
    virtual uint32_t refreshMs() { return 0; }    // periodic redraw (0 = only when invalidated)
    virtual bool keepAwake() { return false; }    // prevents dimming
    virtual bool isHome() { return false; }
};

namespace ui {

void begin(Display* display, RotaryEncoder* knob);
void loop();                      // call from the Arduino loop (UI task)

void push(Screen* s);             // takes ownership
void pop();
void home();                      // back to the home screen
void invalidate();                // redraw as soon as possible
void toast(const char* msg, uint32_t color = theme::TEXT, uint16_t ms = 1800);
void wake();                      // full brightness, restart the idle timer
Display& display();

// Debug helpers (also exposed as serial commands by registerConsoleCommands)
void injectTurn(int detents);
void injectClick();
void injectLongPress();
void screenshot(Print& out);  // dumps the current frame as base64 RGB565
void registerConsoleCommands();

// Helpers shared by screens
void drawWeatherIcon(Canvas& c, int x, int y, int code, bool isDay, bool big);
const char* weatherIcon(int code, bool isDay);
void formatClock(char* out, size_t cap, char* ampm, size_t ampmCap);  // "07:42" (+ "PM")

}  // namespace ui
