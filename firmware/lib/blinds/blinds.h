#pragma once
// Blind controller: owns the stepper + AS5600 and runs the closed-loop motion
// task. Every public method is thread-safe; commands are queued to the motion
// task, status is published as a snapshot.
//
// Position convention: 0% = closed, 100% = open. The AS5600 multi-turn tick
// count is the source of truth; motor steps are only used to command moves.
#include <Arduino.h>

#include "config.h"

enum class BlindState : uint8_t { Idle, Moving, MeasuringRatio, Error };
enum class BlindError : uint8_t { None, Stall, EncoderLost, NotCalibrated, RatioFailed };
enum class CmdSource : uint8_t { Knob, Web, Schedule, Serial, System };

struct BlindStatus {
    BlindState state = BlindState::Idle;
    BlindError error = BlindError::None;
    int32_t ticks = 0;          // current absolute encoder position
    int32_t targetTicks = 0;
    float percent = 0;          // current position, % open (0..100, clamped)
    int8_t targetPercent = -1;  // -1 when not moving to a % target
    int8_t direction = 0;       // +1 opening, -1 closing, 0 stopped
    bool calibrated = false;
    bool encoderOk = false;
    bool magnetOk = false;
    bool tmcUart = false;
    bool simulated = false;
    uint16_t rawAngle = 0;
    int32_t motorSteps = 0;
    uint32_t moveStartMs = 0;
    uint32_t moveEtaMs = 0;     // rough estimate of total move duration
    float stepsPerTick = 0;
};

class Blinds {
public:
    // simulate = true runs a virtual motor + encoder (no hardware needed).
    bool begin(bool simulate = SIMULATE_MECHANICS);

    // --- commands (return false if rejected, e.g. not calibrated)
    bool open(uint16_t durationS = 0, CmdSource src = CmdSource::System);
    bool close(uint16_t durationS = 0, CmdSource src = CmdSource::System);
    bool moveToPercent(float percent, uint16_t durationS = 0, CmdSource src = CmdSource::System);
    bool toggle(CmdSource src = CmdSource::System);  // stop if moving, else open/close
    void stop(CmdSource src = CmdSource::System);

    // --- calibration helpers (ignore soft limits)
    void jog(int32_t ticks);          // relative move in encoder ticks; accumulates while moving
    void setOpenHere();
    void setClosedHere();
    void measureRatio();              // moves ~2 motor turns forward and back
    void clearError();
    void resetCalibration();

    BlindStatus status();
    static const char* stateName(BlindState s);
    static const char* errorName(BlindError e);
    static const char* sourceName(CmdSource s);

    // Extra text from the TMC2209 (if wired), for diagnostics.
    String driverReport();
};

extern Blinds blinds;
