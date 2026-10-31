#include "settings.h"

#include <Preferences.h>

#include "app_log.h"

namespace settings {

static Settings current;
static uint32_t rev = 1;
static SemaphoreHandle_t mtx = nullptr;
static Preferences prefs;

static void lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
static void unlock() { xSemaphoreGive(mtx); }

template <typename T>
static void readNum(JsonObjectConst in, const char* key, T& field, double lo, double hi) {
    JsonVariantConst v = in[key];
    if (v.isNull() || !(v.is<double>() || v.is<long>())) return;
    double d = v.as<double>();
    if (d < lo) d = lo;
    if (d > hi) d = hi;
    field = (T)d;
}

static void readBool(JsonObjectConst in, const char* key, bool& field) {
    JsonVariantConst v = in[key];
    if (v.is<bool>()) field = v.as<bool>();
    else if (v.is<int>()) field = v.as<int>() != 0;
}

static void readStr(JsonObjectConst in, const char* key, char* field, size_t cap) {
    JsonVariantConst v = in[key];
    if (!v.is<const char*>()) return;
    strlcpy(field, v.as<const char*>(), cap);
}

const char* triggerName(ScheduleTrigger t) {
    switch (t) {
        case ScheduleTrigger::Sunrise: return "sunrise";
        case ScheduleTrigger::Sunset: return "sunset";
        default: return "time";
    }
}

void schedulesToJson(const Settings& s, JsonArray out) {
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        const Schedule& sc = s.schedules[i];
        JsonObject o = out.add<JsonObject>();
        o["enabled"] = sc.enabled;
        o["days"] = sc.days;
        o["trigger"] = triggerName(sc.trigger);
        o["minutes"] = sc.minutes;
        o["position"] = sc.position;
        o["gentle"] = sc.gentle;
    }
}

void schedulesFromJson(Settings& s, JsonArrayConst in) {
    int i = 0;
    for (JsonObjectConst o : in) {
        if (i >= MAX_SCHEDULES) break;
        Schedule& sc = s.schedules[i++];
        readBool(o, "enabled", sc.enabled);
        readNum(o, "days", sc.days, 0, 0x7F);
        const char* t = o["trigger"] | (const char*)nullptr;
        if (t) {
            if (!strcmp(t, "sunrise")) sc.trigger = ScheduleTrigger::Sunrise;
            else if (!strcmp(t, "sunset")) sc.trigger = ScheduleTrigger::Sunset;
            else sc.trigger = ScheduleTrigger::Time;
        }
        if (sc.trigger == ScheduleTrigger::Time) readNum(o, "minutes", sc.minutes, 0, 24 * 60 - 1);
        else readNum(o, "minutes", sc.minutes, -240, 240);
        readNum(o, "position", sc.position, 0, 100);
        readBool(o, "gentle", sc.gentle);
    }
}

void toJson(const Settings& s, JsonObject o, bool includeSecrets) {
    o["openTicks"] = s.openTicks;
    o["closedTicks"] = s.closedTicks;
    o["openSet"] = s.openSet;
    o["closedSet"] = s.closedSet;
    o["calibrated"] = s.calibrated();
    o["stepsPerTick"] = s.stepsPerTick;
    o["ratioMeasured"] = s.ratioMeasured;
    o["motorReversed"] = s.motorReversed;
    o["encoderReversed"] = s.encoderReversed;

    o["maxSpeed"] = s.maxSpeed;
    o["acceleration"] = s.acceleration;
    o["gentleSeconds"] = s.gentleSeconds;
    o["stallTolerance"] = s.stallTolerance;
    o["positionTolerance"] = s.positionTolerance;
    o["holdWhenIdle"] = s.holdWhenIdle;
    o["runCurrent"] = s.runCurrent;
    o["holdCurrentPct"] = s.holdCurrentPct;
    o["microsteps"] = s.microsteps;
    o["stealthChop"] = s.stealthChop;

    o["brightness"] = s.brightness;
    o["dimBrightness"] = s.dimBrightness;
    o["dimAfterS"] = s.dimAfterS;
    o["offAfterS"] = s.offAfterS;
    o["rotation"] = s.rotation;
    o["knobReversed"] = s.knobReversed;
    o["knobStepPct"] = s.knobStepPct;
    o["clock24h"] = s.clock24h;

    o["hostname"] = s.hostname;
    o["timezone"] = s.timezone;
    o["latitude"] = s.latitude;
    o["longitude"] = s.longitude;
    o["weatherEnabled"] = s.weatherEnabled;
    o["busStop"] = s.busStop;
    o["busServices"] = s.busServices;
    if (includeSecrets) {
        o["apiToken"] = s.apiToken;
        o["otaPassword"] = s.otaPassword;
    } else {
        o["apiTokenSet"] = s.apiToken[0] != 0;
    }
    o["schedulesPaused"] = s.schedulesPaused;
}

void fromJson(Settings& s, JsonObjectConst in) {
    readNum(in, "openTicks", s.openTicks, -2e9, 2e9);
    readNum(in, "closedTicks", s.closedTicks, -2e9, 2e9);
    readBool(in, "openSet", s.openSet);
    readBool(in, "closedSet", s.closedSet);
    readNum(in, "stepsPerTick", s.stepsPerTick, 0.001, 1000);
    readBool(in, "ratioMeasured", s.ratioMeasured);
    readBool(in, "motorReversed", s.motorReversed);
    readBool(in, "encoderReversed", s.encoderReversed);

    readNum(in, "maxSpeed", s.maxSpeed, 50, 40000);
    readNum(in, "acceleration", s.acceleration, 50, 60000);
    readNum(in, "gentleSeconds", s.gentleSeconds, 10, 3600);
    readNum(in, "stallTolerance", s.stallTolerance, 50, 20000);
    readNum(in, "positionTolerance", s.positionTolerance, 1, 500);
    readBool(in, "holdWhenIdle", s.holdWhenIdle);
    readNum(in, "runCurrent", s.runCurrent, 100, 2000);
    readNum(in, "holdCurrentPct", s.holdCurrentPct, 0, 100);
    readNum(in, "microsteps", s.microsteps, 1, 256);
    readBool(in, "stealthChop", s.stealthChop);

    readNum(in, "brightness", s.brightness, 5, 255);
    readNum(in, "dimBrightness", s.dimBrightness, 0, 255);
    readNum(in, "dimAfterS", s.dimAfterS, 0, 3600);
    readNum(in, "offAfterS", s.offAfterS, 0, 36000);
    readNum(in, "rotation", s.rotation, 0, 3);
    readBool(in, "knobReversed", s.knobReversed);
    readNum(in, "knobStepPct", s.knobStepPct, 1, 20);
    readBool(in, "clock24h", s.clock24h);

    readStr(in, "hostname", s.hostname, sizeof(s.hostname));
    readStr(in, "timezone", s.timezone, sizeof(s.timezone));
    readNum(in, "latitude", s.latitude, -90, 90);
    readNum(in, "longitude", s.longitude, -180, 180);
    readBool(in, "weatherEnabled", s.weatherEnabled);
    readStr(in, "busStop", s.busStop, sizeof(s.busStop));
    readStr(in, "busServices", s.busServices, sizeof(s.busServices));
    readStr(in, "apiToken", s.apiToken, sizeof(s.apiToken));
    readStr(in, "otaPassword", s.otaPassword, sizeof(s.otaPassword));
    readBool(in, "schedulesPaused", s.schedulesPaused);

    JsonArrayConst sch = in["schedules"];
    if (!sch.isNull()) schedulesFromJson(s, sch);

    // normalise
    for (char* p = s.busStop; *p; p++) *p = toupper(*p);
    if (!s.hostname[0]) strlcpy(s.hostname, DEVICE_NAME, sizeof(s.hostname));
    if (s.dimBrightness > s.brightness) s.dimBrightness = s.brightness;
}

static void persistLocked() {
    JsonDocument doc;
    JsonObject o = doc.to<JsonObject>();
    toJson(current, o, true);
    schedulesToJson(current, o["schedules"].to<JsonArray>());
    String out;
    serializeJson(doc, out);
    prefs.putString("cfg", out);
}

void begin(const char* ns) {
    if (!mtx) mtx = xSemaphoreCreateMutex();
    prefs.begin(ns, false);
    String json = prefs.getString("cfg", "");
    if (json.length()) {
        JsonDocument doc;
        if (deserializeJson(doc, json) == DeserializationError::Ok) {
            fromJson(current, doc.as<JsonObjectConst>());
            LOGI("cfg", "settings loaded (%u bytes)", json.length());
        } else {
            LOGW("cfg", "stored settings corrupted, using defaults");
        }
    } else {
        // sensible default schedules, disabled until the user enables them
        current.schedules[0] = {false, 0x1F, ScheduleTrigger::Time, 7 * 60, 100, true};
        current.schedules[1] = {false, 0x60, ScheduleTrigger::Time, 9 * 60 + 30, 100, true};
        current.schedules[2] = {false, 0x7F, ScheduleTrigger::Sunset, 15, 0, false};
        LOGI("cfg", "no stored settings, using defaults");
    }
}

Settings get() {
    lock();
    Settings copy = current;
    unlock();
    return copy;
}

void update(const std::function<void(Settings&)>& fn, bool persist) {
    lock();
    fn(current);
    rev++;
    if (persist) persistLocked();
    unlock();
}

uint32_t revision() { return rev; }

void factoryReset() {
    lock();
    prefs.clear();
    current = Settings();
    rev++;
    unlock();
    LOGW("cfg", "factory reset");
}

bool loadPosition(int32_t& ticks) {
    lock();
    bool ok = prefs.isKey("pos");
    if (ok) ticks = prefs.getInt("pos", 0);
    unlock();
    return ok;
}

void savePosition(int32_t ticks) {
    lock();
    prefs.putInt("pos", ticks);
    unlock();
}

}  // namespace settings
