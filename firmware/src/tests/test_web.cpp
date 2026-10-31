// TEST: WiFi + web app + REST API with a SIMULATED blind (only the ESP32 is
// needed: no motor, encoder or display).
//   ./scripts/test.sh web
//
// First run opens the "Blinds-Setup" WiFi portal: connect with your phone and
// choose your network. Then browse to http://blinds.local (or the IP printed
// below) and try the buttons, schedules and settings, or from a terminal:
//   curl http://blinds.local/api/status
//   curl http://blinds.local/api/open
//   curl "http://blinds.local/api/position?value=30&duration=20"
// Settings live in the separate "blinds-sim" namespace.
#include <Arduino.h>

#include "app_log.h"
#include "blinds.h"
#include "connectivity.h"
#include "console.h"
#include "scheduler.h"
#include "services.h"
#include "settings.h"
#include "web.h"

void setup() {
    applog::begin(115200);
    delay(300);
    LOGI("test", "=== WEB TEST (simulated blind) ===");
    settings::begin("blinds-sim");
    blinds.begin(true);
    net::onPortalStart([]() { web::stop(); });
    net::onConnected([]() { web::begin(); });
    net::begin();
    if (net::state() != net::State::Portal) web::begin();
    services::begin();
}

void loop() {
    net::loop();
    web::loop();
    console::poll();

    static uint32_t lastSched = 0, lastInfo = 0;
    static bool shown = false;
    if (millis() - lastSched >= 1000) {
        lastSched = millis();
        scheduler::tick();
    }
    if (net::connected() && !shown) {
        shown = true;
        LOGI("test", "open  http://%s.local  or  http://%s", settings::get().hostname, net::ip().c_str());
    }
    BlindStatus st = blinds.status();
    if (st.state == BlindState::Moving && millis() - lastInfo > 500) {
        lastInfo = millis();
        LOGI("test", "simulated blind at %.1f%%", st.percent);
    }
    delay(2);
}
