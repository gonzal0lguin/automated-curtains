// Smart roller blinds - main firmware.
//
// Tasks:
//   motion  (core 1, high prio)  AS5600 polling + closed-loop stepper control (lib/blinds)
//   loop    (core 1)             knob + round display UI, serial console
//   net     (core 0)             WiFi, web server/API, OTA, scheduler
//   fetch   (core 0, low prio)   weather + bus HTTP requests (lib/services)
#include <Arduino.h>

#include "app_log.h"
#include "blinds.h"
#include "config.h"
#include "connectivity.h"
#include "console.h"
#include "display.h"
#include "rotary_encoder.h"
#include "scheduler.h"
#include "services.h"
#include "settings.h"
#include "ui.h"
#include "web.h"

// Large VLW glyphs are rendered via alloca() on the UI task's stack.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static Display display;
static RotaryEncoder knob(PIN_KNOB_A, PIN_KNOB_B, PIN_KNOB_SW, KNOB_TRANSITIONS_PER_DETENT);

static void splash(const char* msg) {
    display.render([msg](Canvas& c) {
        c.clear(theme::BG);
        c.ring(120, 120, 118, 4, theme::TRACK);
        c.arc(120, 120, 118, 4, 225, 270, theme::ACCENT);
        c.text(ICON_SUN, 120, 72, Font::IconBig, theme::ACCENT);
        c.text("Smart Blinds", 120, 122, Font::Title, theme::TEXT);
        c.text(msg, 120, 156, Font::Small, theme::MUTED);
        c.text("v" FW_VERSION, 120, 200, Font::Small, theme::DIM);
    });
}

static void netTask(void*) {
    uint32_t lastSched = 0;
    for (;;) {
        net::loop();
        web::loop();
        if (millis() - lastSched >= 1000) {
            lastSched = millis();
            scheduler::tick();
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void setup() {
    applog::begin(115200);
    delay(200);
    LOGI("main", "=== Smart Blinds v%s%s ===", FW_VERSION, SIMULATE_MECHANICS ? " (SIMULATED MECHANICS)" : "");

    settings::begin(SIMULATE_MECHANICS ? "blinds-sim" : "blinds");
    Settings cfg = settings::get();

    if (!display.begin(cfg.rotation)) LOGE("main", "display init failed");
    splash("starting...");
    display.setBrightness(cfg.brightness, 400);
    for (int i = 0; i < 25; i++) {
        display.tick();
        delay(16);
    }

    knob.begin();
    splash("motor & encoder...");
    if (!blinds.begin(SIMULATE_MECHANICS)) LOGE("main", "blinds hardware init incomplete (see above)");

    splash("connecting WiFi...");
    net::onPortalStart([]() { web::stop(); });
    net::onConnected([]() { web::begin(); });
    net::begin();
    if (net::state() != net::State::Portal) web::begin();
    services::begin();
    xTaskCreatePinnedToCore(netTask, "net", 10240, nullptr, 2, nullptr, 0);

    ui::begin(&display, &knob);
    ui::registerConsoleCommands();
    LOGI("main", "ready - free heap %lu KB, type 'help' on serial", (unsigned long)ESP.getFreeHeap() / 1024);
}

void loop() {
    ui::loop();
    console::poll();
    delay(2);
}
