#include "ui.h"

#include <base64.h>

#include <memory>
#include <vector>

#include "console.h"

#include "app_log.h"
#include "blinds.h"
#include "connectivity.h"
#include "screens.h"
#include "settings.h"

void Screen::onLongPress() { ui::pop(); }

namespace ui {

namespace {

Display* disp = nullptr;
RotaryEncoder* enc = nullptr;
std::vector<Screen*> stack;
std::vector<Screen*> graveyard;   // popped screens, deleted after the input dispatch
Screen* homeScreen = nullptr;

enum class Op : uint8_t { Push, Pop, Home };
struct PendingOp {
    Op op;
    Screen* s;
};
std::vector<PendingOp> pending;

bool dirty = true;
uint32_t lastDraw = 0, lastInput = 0, lastTurn = 0;
enum class Power : uint8_t { On, Dim, Off } power = Power::On;

volatile int injDelta = 0;
volatile bool injClick = false, injLong = false;

char toastMsg[48] = "";
uint32_t toastColor = 0, toastUntil = 0;

BlindState lastBlindState = BlindState::Idle;
int lastOta = -1;
net::State lastNet = net::State::Off;

Screen* top() { return stack.empty() ? nullptr : stack.back(); }

void applyPending() {
    if (pending.empty()) return;
    for (auto& p : pending) {
        if (p.op == Op::Push) {
            stack.push_back(p.s);
        } else if (p.op == Op::Pop) {
            if (stack.size() > 1) {
                graveyard.push_back(stack.back());
                stack.pop_back();
            }
        } else {
            while (stack.size() > 1) {
                graveyard.push_back(stack.back());
                stack.pop_back();
            }
        }
    }
    pending.clear();
    for (Screen* s : graveyard) delete s;
    graveyard.clear();
    if (top()) top()->enter();
    dirty = true;
}

void setPower(Power p) {
    Settings s = settings::get();
    power = p;
    switch (p) {
        case Power::On: disp->setBrightness(s.brightness, 200); break;
        case Power::Dim: disp->setBrightness(min(s.dimBrightness, s.brightness), 800); break;
        case Power::Off: disp->setBrightness(0, 600); break;
    }
}

void drawOverlays(Canvas& c) {
    int ota = net::otaProgress();
    if (ota >= 0) {
        c.clear(theme::BG);
        c.ring(120, 120, 110, 10, theme::TRACK);
        c.arc(120, 120, 110, 10, 0, 3.6f * ota, theme::SKY);
        c.text(ICON_DOWNLOAD, 120, 82, Font::IconBig, theme::SKY);
        char b[16];
        snprintf(b, sizeof(b), "%d%%", ota);
        c.text(b, 120, 135, Font::Title, theme::TEXT);
        c.text("Updating firmware", 120, 170, Font::Small, theme::MUTED);
        return;
    }
    if (toastUntil && (int32_t)(toastUntil - millis()) > 0) {
        int w = c.textWidth(toastMsg, Font::Body) + 32;
        if (w > 200) w = 200;
        c.fillRoundRect(120 - w / 2, 184, w, 34, 17, theme::SURFACE);
        c.textFit(toastMsg, 120, 201, w - 20, Font::Body, toastColor);
    }
}

}  // namespace

Display& display() { return *disp; }

void begin(Display* d, RotaryEncoder* k) {
    disp = d;
    enc = k;
    homeScreen = createHomeScreen();
    stack.push_back(homeScreen);
    homeScreen->enter();
    lastInput = millis();
    setPower(Power::On);
}

void push(Screen* s) { pending.push_back({Op::Push, s}); }
void pop() { pending.push_back({Op::Pop, nullptr}); }
void home() { pending.push_back({Op::Home, nullptr}); }
void invalidate() { dirty = true; }

void toast(const char* msg, uint32_t color, uint16_t ms) {
    strlcpy(toastMsg, msg, sizeof(toastMsg));
    toastColor = color;
    toastUntil = millis() + ms;
    dirty = true;
}

void wake() {
    lastInput = millis();
    if (power != Power::On) setPower(Power::On);
}

void loop() {
    uint32_t now = millis();
    Settings cfg = settings::get();
    enc->setReversed(cfg.knobReversed);
    enc->tick();

    // ------------------------------------------------------------- input
    int d = enc->getDelta();
    RotaryEncoder::Event ev = enc->getEvent();
    if (injDelta) {
        d += injDelta;
        injDelta = 0;
    }
    if (ev == RotaryEncoder::Event::None && injClick) {
        ev = RotaryEncoder::Event::Click;
        injClick = false;
    } else if (ev == RotaryEncoder::Event::None && injLong) {
        ev = RotaryEncoder::Event::LongPress;
        injLong = false;
    }
    if (d || ev != RotaryEncoder::Event::None) {
        bool wasOff = power == Power::Off;
        lastInput = now;
        if (power != Power::On) setPower(Power::On);
        if (wasOff) {
            dirty = true;  // the first input on a dark screen only wakes it
        } else if (Screen* s = top()) {
            if (d) {
                uint32_t dt = now - lastTurn;
                lastTurn = now;
                int accel = dt < 25 ? 5 : dt < 60 ? 2 : 1;
                s->onTurn(d, accel);
            }
            if (ev == RotaryEncoder::Event::Click) s->onClick();
            else if (ev == RotaryEncoder::Event::LongPress) s->onLongPress();
            dirty = true;
        }
    }

    // ------------------------------------------------------ outside events
    BlindStatus bs = blinds.status();
    if (bs.state != lastBlindState) {
        if (bs.state == BlindState::Moving || bs.state == BlindState::Error) wake();
        // the home screen shows errors itself; elsewhere pop up a toast
        if (bs.state == BlindState::Error && top() && !top()->isHome())
            toast(Blinds::errorName(bs.error), theme::BAD, 3000);
        lastBlindState = bs.state;
        dirty = true;
    }
    int ota = net::otaProgress();
    if (ota != lastOta) {
        lastOta = ota;
        wake();
        dirty = true;
    }
    net::State ns = net::state();
    if (ns != lastNet) {
        lastNet = ns;
        dirty = true;
    }

    applyPending();
    Screen* s = top();
    if (!s) return;
    s->update();
    applyPending();
    s = top();

    // ---------------------------------------------------------- power
    bool awake = s->keepAwake() || bs.state == BlindState::Moving || ota >= 0;
    if (awake) lastInput = now;
    uint32_t idle = (now - lastInput) / 1000;
    if (power == Power::On && cfg.dimAfterS && idle >= cfg.dimAfterS) setPower(Power::Dim);
    if (power != Power::Off && cfg.offAfterS && idle >= cfg.offAfterS) {
        setPower(Power::Off);
        home();  // next wake starts from the clock
    }
    if (power == Power::On && disp->brightness() != cfg.brightness) setPower(Power::On);  // setting changed
    disp->tick();

    // ---------------------------------------------------------- render
    if (power == Power::Off && disp->brightness() == 0 && !dirty) return;
    uint32_t refresh = s->refreshMs();
    if (toastUntil && (int32_t)(toastUntil - now) <= 0) {
        toastUntil = 0;
        dirty = true;
    }
    if (refresh && now - lastDraw >= refresh) dirty = true;
    if (dirty && now - lastDraw >= 16) {
        dirty = false;
        lastDraw = now;
        disp->render([s](Canvas& c) {
            s->draw(c);
            drawOverlays(c);
        });
    }
}

// -------------------------------------------------------------- debug

void injectTurn(int d) { injDelta = injDelta + d; }
void injectClick() { injClick = true; }
void injectLongPress() { injLong = true; }

void screenshot(Print& out) {
    Screen* s = top();
    if (!disp || !s) {
        out.println("screenshot: UI not running");
        return;
    }
    out.printf("=== SCREENSHOT %d %d RGB565BE ===\n", Canvas::W, Canvas::H);
    disp->render([s](Canvas& c) {
        s->draw(c);
        drawOverlays(c);
    }, [&out](int, const uint8_t* row, size_t len) {
        out.println(base64::encode(row, len));
    });
    out.println("=== END ===");
}

void registerConsoleCommands() {
    console::addCommand("screenshot", "dump the screen (tools/screenshot.py makes a PNG)",
                        [](const char*) { screenshot(Serial); });
    console::addCommand("knob", "simulate knob turns, e.g. knob 3 / knob -1", [](const char* a) {
        injectTurn(atoi(a) ? atoi(a) : 1);
    });
    console::addCommand("click", "simulate a knob click", [](const char*) { injectClick(); });
    console::addCommand("long", "simulate a long press", [](const char*) { injectLongPress(); });
}

// ------------------------------------------------------------ helpers

const char* weatherIcon(int code, bool isDay) {
    if (code <= 1) return isDay ? ICON_SUN : ICON_MOON;
    if (code <= 3 || code == 45 || code == 48) return ICON_CLOUD;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return ICON_SNOW;
    if (code >= 95) return ICON_BOLT;
    return ICON_UMBRELLA;
}

void drawWeatherIcon(Canvas& c, int x, int y, int code, bool isDay, bool big) {
    Font f = big ? Font::IconBig : Font::Icon;
    if (code == 2 && big) {
        // partly cloudy: sun peeking behind a cloud
        c.text(isDay ? ICON_SUN : ICON_MOON, x + 12, y - 10, Font::Icon, isDay ? theme::ACCENT : theme::CLOUD);
        c.text(ICON_CLOUD, x - 2, y + 4, f, theme::CLOUD);
        return;
    }
    const char* icon = weatherIcon(code, isDay);
    uint32_t col = theme::CLOUD;
    if (!strcmp(icon, ICON_SUN) || !strcmp(icon, ICON_BOLT)) col = theme::ACCENT;
    else if (!strcmp(icon, ICON_UMBRELLA)) col = theme::SKY;
    c.text(icon, x, y, f, col);
}

void formatClock(char* out, size_t cap, char* ampm, size_t ampmCap) {
    if (ampm && ampmCap) ampm[0] = 0;
    if (!net::timeValid()) {
        snprintf(out, cap, "--:--");
        return;
    }
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    if (settings::get().clock24h) {
        snprintf(out, cap, "%02d:%02d", t.tm_hour, t.tm_min);
    } else {
        int h = t.tm_hour % 12;
        snprintf(out, cap, "%d:%02d", h ? h : 12, t.tm_min);
        if (ampm) snprintf(ampm, ampmCap, "%s", t.tm_hour < 12 ? "AM" : "PM");
    }
}

}  // namespace ui
