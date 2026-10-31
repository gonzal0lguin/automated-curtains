#pragma once
// Persistent runtime settings (NVS). Every module reads a copy with
// settings::get() and watches settings::revision() to re-apply changes.
#include <Arduino.h>
#include <ArduinoJson.h>

#include <functional>

#include "config.h"

enum class ScheduleTrigger : uint8_t { Time = 0, Sunrise = 1, Sunset = 2 };

struct Schedule {
    bool enabled = false;
    uint8_t days = 0x7F;                       // bit0 = Monday ... bit6 = Sunday
    ScheduleTrigger trigger = ScheduleTrigger::Time;
    int16_t minutes = 7 * 60;                  // time of day, or offset (min) for sun triggers
    uint8_t position = 100;                    // target, % open
    bool gentle = false;                       // slow "sunrise" style move
};

static constexpr int MAX_SCHEDULES = 8;

struct Settings {
    // --- calibration (positions in absolute AS5600 ticks)
    int32_t openTicks = 0;
    int32_t closedTicks = 0;
    bool openSet = false;
    bool closedSet = false;
    float stepsPerTick = 1600.0f / ENCODER_CPR;  // auto-measured by "Measure ratio"
    bool ratioMeasured = false;
    bool motorReversed = false;                   // flips motor direction
    bool encoderReversed = false;                 // set by ratio measurement

    // --- motion
    uint16_t maxSpeed = 3000;       // steps/s
    uint16_t acceleration = 2500;   // steps/s^2
    uint16_t gentleSeconds = 300;   // duration of a "gentle" open/close
    uint16_t stallTolerance = 600;  // ticks of following error before declaring a stall
    uint16_t positionTolerance = 8; // ticks: good enough to stop correcting
    bool holdWhenIdle = false;      // keep driver energized when stopped
    // TMC2209 UART only
    uint16_t runCurrent = 700;      // mA RMS
    uint8_t holdCurrentPct = 30;    // % of run current while holding
    uint16_t microsteps = 16;
    bool stealthChop = true;

    // --- display / knob
    uint8_t brightness = 200;      // 0-255
    uint8_t dimBrightness = 25;
    uint16_t dimAfterS = 20;       // 0 = never
    uint16_t offAfterS = 120;      // 0 = never
    uint8_t rotation = 0;          // 0..3 (90° steps)
    bool knobReversed = false;
    uint8_t knobStepPct = 2;       // % per detent when dialing a position
    bool clock24h = true;

    // --- network / services
    char hostname[32] = DEVICE_NAME;
    char timezone[64] = "<-04>4<-03>,M9.1.6/24,M4.1.6/24";  // America/Santiago
    float latitude = -33.45f;
    float longitude = -70.66f;
    bool weatherEnabled = true;
    char busStop[12] = "";          // e.g. "PA433"; empty = bus page disabled
    char busServices[40] = "";      // comma separated filter, e.g. "506,507"
    char apiToken[33] = "";         // optional: required as ?token= for control API
    char otaPassword[33] = "blinds";

    // --- scheduling
    bool schedulesPaused = false;
    Schedule schedules[MAX_SCHEDULES];

    bool calibrated() const { return openSet && closedSet && abs(openTicks - closedTicks) > 50; }
};

namespace settings {

// ns: NVS namespace (the simulator uses its own so it never touches the real config)
void begin(const char* ns = "blinds");
Settings get();
// Atomically modify, bump revision and (optionally) persist.
void update(const std::function<void(Settings&)>& fn, bool persist = true);
uint32_t revision();
void factoryReset();

// JSON (used by the web API and the serial console). fromJson applies only
// the keys present, clamping values to sane ranges.
void toJson(const Settings& s, JsonObject out, bool includeSecrets = false);
void fromJson(Settings& s, JsonObjectConst in);
void schedulesToJson(const Settings& s, JsonArray out);
void schedulesFromJson(Settings& s, JsonArrayConst in);

// Last known blind position (separate key; written after every move).
bool loadPosition(int32_t& ticks);
void savePosition(int32_t ticks);

const char* triggerName(ScheduleTrigger t);

}  // namespace settings
