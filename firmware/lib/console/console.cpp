#include "console.h"

#include <ArduinoJson.h>

#include <vector>

#include "app_log.h"
#include "blinds.h"
#include "connectivity.h"
#include "scheduler.h"
#include "services.h"
#include "settings.h"

namespace console {

static char line[512];
static size_t len = 0;

struct Extra {
    const char* name;
    const char* help;
    Handler fn;
};
static std::vector<Extra> extras;

void addCommand(const char* name, const char* help, Handler fn) { extras.push_back({name, help, fn}); }

static const char* HELP =
    "Commands:\n"
    "  status                   blind / encoder / motor state\n"
    "  open [s] | close [s]     move fully (optional duration in seconds)\n"
    "  goto <pct> [s]           move to a position, e.g. goto 40\n"
    "  stop | toggle | clear    stop / open-close toggle / clear error\n"
    "  jog <ticks>              relative move ignoring limits (4096 = 1 turn)\n"
    "  setopen | setclosed      store the current position as open / closed\n"
    "  ratio                    measure motor steps per encoder tick\n"
    "  resetcal                 forget open/closed positions\n"
    "  settings                 print settings JSON\n"
    "  set {json}               change settings, e.g. set {\"maxSpeed\":2500}\n"
    "  schedules                list schedules\n"
    "  wifi | portal | forget   WiFi status / open setup portal / erase WiFi\n"
    "  weather | bus            print latest weather / bus data\n"
    "  driver                   TMC2209 report\n"
    "  log                      recent log lines\n"
    "  heap | reboot | factory  memory info / restart / erase all settings\n";

static void printStatus() {
    BlindStatus st = blinds.status();
    Serial.printf("state=%s error=%s pos=%.1f%% target=%d ticks=%ld target_ticks=%ld raw=%u steps=%ld\n",
                  Blinds::stateName(st.state), Blinds::errorName(st.error), st.percent, st.targetPercent,
                  (long)st.ticks, (long)st.targetTicks, st.rawAngle, (long)st.motorSteps);
    Settings s = settings::get();
    Serial.printf("calibrated=%d (closed=%ld%s open=%ld%s) ratio=%.5f%s encoder=%s uart=%d sim=%d\n",
                  s.calibrated(), (long)s.closedTicks, s.closedSet ? "" : "?", (long)s.openTicks,
                  s.openSet ? "" : "?", s.stepsPerTick, s.ratioMeasured ? "" : " (default)",
                  st.encoderOk ? "ok" : "MISSING", st.tmcUart, st.simulated);
}

static void execute(char* cmd) {
    while (*cmd == ' ') cmd++;
    char* arg = strchr(cmd, ' ');
    if (arg) *arg++ = 0;
    else arg = cmd + strlen(cmd);
    while (*arg == ' ') arg++;

    for (auto& e : extras) {
        if (!strcmp(cmd, e.name)) {
            e.fn(arg);
            return;
        }
    }
    if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
        Serial.print(HELP);
        for (auto& e : extras) Serial.printf("  %-24s %s\n", e.name, e.help);
    }
    else if (!strcmp(cmd, "status")) printStatus();
    else if (!strcmp(cmd, "open")) Serial.println(blinds.open(atoi(arg), CmdSource::Serial) ? "ok" : "rejected");
    else if (!strcmp(cmd, "close")) Serial.println(blinds.close(atoi(arg), CmdSource::Serial) ? "ok" : "rejected");
    else if (!strcmp(cmd, "goto")) {
        char* sp = strchr(arg, ' ');
        int d = sp ? atoi(sp + 1) : 0;
        Serial.println(blinds.moveToPercent(atof(arg), d, CmdSource::Serial) ? "ok" : "rejected");
    } else if (!strcmp(cmd, "stop")) blinds.stop(CmdSource::Serial);
    else if (!strcmp(cmd, "toggle")) blinds.toggle(CmdSource::Serial);
    else if (!strcmp(cmd, "clear")) blinds.clearError();
    else if (!strcmp(cmd, "jog")) blinds.jog(atol(arg));
    else if (!strcmp(cmd, "setopen")) blinds.setOpenHere();
    else if (!strcmp(cmd, "setclosed")) blinds.setClosedHere();
    else if (!strcmp(cmd, "ratio")) blinds.measureRatio();
    else if (!strcmp(cmd, "resetcal")) blinds.resetCalibration();
    else if (!strcmp(cmd, "settings")) {
        JsonDocument doc;
        settings::toJson(settings::get(), doc.to<JsonObject>(), true);
        serializeJsonPretty(doc, Serial);
        Serial.println();
    } else if (!strcmp(cmd, "set")) {
        JsonDocument doc;
        if (deserializeJson(doc, arg) || !doc.is<JsonObject>()) {
            Serial.println("invalid JSON object");
            return;
        }
        settings::update([&](Settings& s) { settings::fromJson(s, doc.as<JsonObjectConst>()); });
        Serial.println("saved");
    } else if (!strcmp(cmd, "schedules")) {
        Settings s = settings::get();
        for (int i = 0; i < MAX_SCHEDULES; i++) {
            char when[24], days[32];
            scheduler::describe(s.schedules[i], when, sizeof(when));
            scheduler::daysString(s.schedules[i].days, days, sizeof(days));
            Serial.printf("#%d %-3s %-12s %-20s -> %3d%%%s\n", i + 1, s.schedules[i].enabled ? "ON" : "off", when,
                          days, s.schedules[i].position, s.schedules[i].gentle ? " gentle" : "");
        }
        scheduler::Next n = scheduler::next();
        if (n.valid) Serial.printf("next: #%d in %ld min\n", n.index + 1, (long)(n.when - time(nullptr)) / 60);
        if (s.schedulesPaused) Serial.println("(schedules are PAUSED)");
    } else if (!strcmp(cmd, "wifi")) {
        Serial.printf("%s ssid=%s ip=%s rssi=%d time=%s\n", net::stateName(net::state()), net::ssid().c_str(),
                      net::ip().c_str(), net::rssi(), net::timeValid() ? "synced" : "no");
    } else if (!strcmp(cmd, "portal")) net::startPortal();
    else if (!strcmp(cmd, "forget")) net::forgetWifi();
    else if (!strcmp(cmd, "weather")) {
        WeatherData w = services::weather();
        if (!w.valid) Serial.println("no weather data yet");
        else Serial.printf("%.1fC %s, max %.0f min %.0f, hum %d%%, wind %.0f km/h, rain %d%%\n", w.temp,
                           weather::describe(w.code), w.tMax, w.tMin, w.humidity, w.wind, w.precipProb);
    } else if (!strcmp(cmd, "bus")) {
        services::refreshBus();
        BusData b = services::bus();
        Serial.printf("stop %s (%s): %s\n", b.stop, b.stopName, b.message);
        for (int i = 0; i < b.count; i++)
            Serial.printf("  %-6s %d-%d min  %.1f km\n", b.items[i].service, b.items[i].minMin, b.items[i].maxMin,
                          b.items[i].meters / 1000.0f);
    } else if (!strcmp(cmd, "driver")) Serial.println(blinds.driverReport());
    else if (!strcmp(cmd, "log")) Serial.print(applog::recent());
    else if (!strcmp(cmd, "heap")) {
        Serial.printf("free %lu, min free %lu, largest block %lu, this task's free stack %lu\n",
                      (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
                      (unsigned long)ESP.getMaxAllocHeap(), (unsigned long)uxTaskGetStackHighWaterMark(nullptr));
    } else if (!strcmp(cmd, "reboot")) ESP.restart();
    else if (!strcmp(cmd, "factory")) {
        settings::factoryReset();
        ESP.restart();
    } else if (*cmd) Serial.printf("unknown command '%s' (type help)\n", cmd);
}

void poll() {
    while (Serial.available()) {
        char ch = Serial.read();
        if (ch == '\r') continue;
        if (ch == '\n') {
            line[len] = 0;
            if (len) {
                Serial.printf("> %s\n", line);
                execute(line);
            }
            len = 0;
        } else if (len < sizeof(line) - 1) {
            line[len++] = ch;
        }
    }
}

}  // namespace console
