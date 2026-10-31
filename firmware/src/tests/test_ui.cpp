// TEST: knob UI and menus, with SIMULATED motor/encoder (only display + knob
// needed, no WiFi). The clock starts at 07:41 and fake weather/bus data are
// shown so every screen can be checked.
//   ./scripts/test.sh ui
//
// Everything in the menus works against the simulator (positions, schedules,
// settings). Settings are stored in a separate "blinds-sim" namespace, so the
// real calibration is never touched. The serial console also works ("help").
#include <Arduino.h>
#include <sys/time.h>

#include "app_log.h"
#include "blinds.h"
#include "config.h"
#include "connectivity.h"
#include "console.h"
#include "display.h"
#include "rotary_encoder.h"
#include "services.h"
#include "settings.h"
#include "ui.h"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

Display display;
RotaryEncoder knob(PIN_KNOB_A, PIN_KNOB_B, PIN_KNOB_SW, KNOB_TRANSITIONS_PER_DETENT);

void setup() {
    applog::begin(115200);
    delay(300);
    LOGI("test", "=== UI TEST (simulated mechanics) ===");

    settings::begin("blinds-sim");
    Settings cfg = settings::get();
    net::applyTimezone(cfg.timezone);
    // fake clock: 2026-10-20 07:41 local
    struct tm t = {};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 9;
    t.tm_mday = 20;
    t.tm_hour = 7;
    t.tm_min = 41;
    t.tm_isdst = -1;
    timeval tv = {mktime(&t), 0};
    settimeofday(&tv, nullptr);

    WeatherData w;
    w.valid = true;
    w.temp = 17.4f;
    w.tMax = 24;
    w.tMin = 9;
    w.code = 2;
    w.humidity = 48;
    w.wind = 11;
    w.precipProb = 10;
    w.isDay = true;
    w.updated = time(nullptr);
    services::setWeather(w);

    BusData b;
    b.valid = true;
    strlcpy(b.stop, "PA433", sizeof(b.stop));
    strlcpy(b.stopName, "Av. Providencia / Esq. Los Leones", sizeof(b.stopName));
    BusArrival arr[] = {{"506", 2, 4, 650}, {"502", 6, 9, 2100}, {"506", 14, 18, 5200}, {"C01", 21, 26, 7300}};
    memcpy(b.items, arr, sizeof(arr));
    b.count = 4;
    b.updated = time(nullptr);
    services::setBus(b);
    if (!cfg.busStop[0]) settings::update([](Settings& s) { strlcpy(s.busStop, "PA433", sizeof(s.busStop)); });

    display.begin(cfg.rotation);
    knob.begin();
    blinds.begin(true);
    ui::begin(&display, &knob);
    ui::registerConsoleCommands();
    LOGI("test", "turn the knob! (type help for console commands)");
}

void loop() {
    ui::loop();
    console::poll();
    delay(2);
}
