// TEST: network services - WiFi, NTP time, weather and Red bus arrivals.
//   ./scripts/test.sh net
//
// Uses the saved WiFi (or opens the "Blinds-Setup" portal). Prints the local
// time, sunrise/sunset, the weather and the next buses every 30 s.
// Commands:  stop <CODE>   set bus stop, e.g. "stop PA433"
//            lat <v> / lon <v>  set location      now   fetch immediately
#include <Arduino.h>
#include <WiFi.h>

#include "app_log.h"
#include "bus.h"
#include "connectivity.h"
#include "settings.h"
#include "sun.h"
#include "weather.h"

static char line[64];
static size_t len = 0;
static uint32_t lastFetch = 0;
static bool fetchNow = true;

static void report() {
    Settings s = settings::get();
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    char buf[40];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &t);
    Serial.printf("\n--- %s (time %s) | WiFi %s %s %d dBm\n", buf, net::timeValid() ? "synced" : "NOT synced",
                  net::ssid().c_str(), net::ip().c_str(), net::rssi());
    int rise, set;
    if (net::timeValid() && sun::times(now, s.latitude, s.longitude, rise, set))
        Serial.printf("sunrise %02d:%02d  sunset %02d:%02d  (lat %.4f lon %.4f)\n", rise / 60, rise % 60, set / 60,
                      set % 60, s.latitude, s.longitude);

    WeatherData w;
    if (weather::fetch(s.latitude, s.longitude, w))
        Serial.printf("weather: %.1fC %s  max %.0f min %.0f  hum %d%%  wind %.0f km/h  rain %d%%\n", w.temp,
                      weather::describe(w.code), w.tMax, w.tMin, w.humidity, w.wind, w.precipProb);
    else Serial.println("weather: FAILED");

    if (s.busStop[0]) {
        BusData b;
        bool ok = bus::fetch(s.busStop, s.busServices, b);
        Serial.printf("bus stop %s (%s): %s %s\n", b.stop, b.stopName, ok ? "ok" : "FAILED", b.message);
        for (int i = 0; i < b.count; i++)
            Serial.printf("   %-6s %3d-%-3d min  %5.1f km\n", b.items[i].service, b.items[i].minMin, b.items[i].maxMin,
                          b.items[i].meters / 1000.0f);
    } else {
        Serial.println("bus: no stop set (type: stop PA433)");
    }
    Serial.printf("free heap %lu\n", (unsigned long)ESP.getFreeHeap());
}

static void exec(char* cmd) {
    if (!strncmp(cmd, "stop ", 5)) {
        String code = String(cmd + 5);
        code.trim();
        code.toUpperCase();
        settings::update([&](Settings& s) { strlcpy(s.busStop, code.c_str(), sizeof(s.busStop)); });
        fetchNow = true;
    } else if (!strncmp(cmd, "lat ", 4)) {
        float v = atof(cmd + 4);
        settings::update([v](Settings& s) { s.latitude = v; });
        fetchNow = true;
    } else if (!strncmp(cmd, "lon ", 4)) {
        float v = atof(cmd + 4);
        settings::update([v](Settings& s) { s.longitude = v; });
        fetchNow = true;
    } else if (!strcmp(cmd, "now")) {
        fetchNow = true;
    } else {
        Serial.println("commands: stop <CODE> | lat <v> | lon <v> | now");
    }
}

void setup() {
    applog::begin(115200);
    delay(300);
    LOGI("test", "=== NETWORK TEST ===");
    settings::begin();
    net::begin();
}

void loop() {
    net::loop();
    while (Serial.available()) {
        char ch = Serial.read();
        if (ch == '\r') continue;
        if (ch == '\n') {
            line[len] = 0;
            if (len) exec(line);
            len = 0;
        } else if (len < sizeof(line) - 1) {
            line[len++] = ch;
        }
    }
    if (net::connected() && (fetchNow || millis() - lastFetch > 30000)) {
        if (!net::timeValid() && millis() - lastFetch < 5000 && !fetchNow) return;
        fetchNow = false;
        lastFetch = millis();
        report();
    }
    delay(5);
}
