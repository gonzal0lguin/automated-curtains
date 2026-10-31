#include "blinds.h"

#include <atomic>

#include "app_log.h"
#include "magnetic_encoder.h"
#include "motor.h"
#include "settings.h"

Blinds blinds;

namespace {

enum class CmdType : uint8_t { GotoTicks, GotoPercent, Jog, Stop, SetOpen, SetClosed, MeasureRatio, ClearError, ResetCal };

struct Cmd {
    CmdType type;
    float value;
    int32_t ticks;
    uint16_t duration;
    CmdSource src;
};

constexpr uint32_t LOOP_MS = 2;
constexpr uint32_t SETTLE_MS = 120;
constexpr int MAX_CORRECTIONS = 3;
constexpr int32_t SIM_OPEN_TICKS = 40000;

QueueHandle_t queue = nullptr;
SemaphoreHandle_t statusMtx = nullptr;
SemaphoreHandle_t driverMtx = nullptr;
BlindStatus published;

StepperDriver* motor = nullptr;
MagneticEncoder* enc = nullptr;
SimStepper* sim = nullptr;
bool simulated = false;

// ---- state owned by the motion task
Settings cfg;
uint32_t cfgRev = 0;
bool appliedMotorReversed = false;
uint16_t appliedMicrosteps = 0, appliedCurrent = 0;
uint8_t appliedHoldPct = 0;
bool appliedStealth = true, appliedHold = false;

BlindState state = BlindState::Idle;
BlindError error = BlindError::None;
int32_t targetTicks = 0;
int8_t targetPct = -1;
int8_t direction = 0;
bool jogMode = false, stopping = false, settling = false;
int corrections = 0;
uint32_t settleSince = 0, moveStartMs = 0, etaMs = 0;
int32_t moveStartTicks = 0, moveStartSteps = 0;
// stall window
int32_t winTicks0 = 0, winSteps0 = 0;
uint32_t winStart = 0;
int badWindows = 0;
// ratio measurement
int ratioPhase = 0;
int32_t ratioT0 = 0, ratioS0 = 0, ratioDT1 = 0, ratioDS1 = 0;
// persistence
int32_t lastSaved = INT32_MIN;
uint32_t lastSaveMs = 0, stableSince = 0;
int32_t stableTicks = 0;

float pctOf(int32_t ticks) {
    if (!cfg.calibrated()) return 0;
    float p = 100.0f * (ticks - cfg.closedTicks) / (float)(cfg.openTicks - cfg.closedTicks);
    return p < 0 ? 0 : (p > 100 ? 100 : p);
}

// NVS writes can take tens of ms, so they are handed to a low-priority task
// instead of blocking the 2 ms motion loop.
std::atomic<int32_t> savePos{0};
std::atomic<bool> saveRequested{false};

void savePosition(bool force) {
    int32_t pos = enc->position();
    if (!force && abs(pos - lastSaved) < 4) return;
    savePos = pos;
    saveRequested = true;
    lastSaved = pos;
    lastSaveMs = millis();
}

void persistTask(void*) {
    for (;;) {
        if (saveRequested.exchange(false)) settings::savePosition(savePos.load());
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setError(BlindError e) {
    motor->forceStop();
    state = BlindState::Error;
    error = e;
    direction = 0;
    targetPct = -1;
    LOGE("blinds", "ERROR: %s at %ld ticks", Blinds::errorName(e), (long)enc->position());
    savePosition(true);
}

void resetWindow() {
    winTicks0 = enc->position();
    winSteps0 = motor->position();
    winStart = millis();
}

void finishMove(const char* why) {
    state = BlindState::Idle;
    direction = 0;
    targetPct = -1;
    stopping = false;
    settling = false;
    int32_t pos = enc->position();
    LOGI("blinds", "%s: at %ld ticks (%.1f%%), error %ld ticks, %d corrections, %lu ms", why, (long)pos,
         pctOf(pos), (long)(targetTicks - pos), corrections, (unsigned long)(millis() - moveStartMs));
    savePosition(false);
}

void startMove(int32_t target, uint16_t durationS, bool jog) {
    if (!enc->connected()) {
        setError(BlindError::EncoderLost);
        return;
    }
    if (!jog && cfg.calibrated()) {
        int32_t lo = min(cfg.openTicks, cfg.closedTicks), hi = max(cfg.openTicks, cfg.closedTicks);
        target = constrain(target, lo, hi);
    }
    int32_t cur = enc->position();
    int32_t delta = target - cur;
    bool wasMoving = state == BlindState::Moving && motor->isRunning();

    targetTicks = target;
    jogMode = jog;
    corrections = 0;
    stopping = false;
    settling = false;
    if (error != BlindError::None) error = BlindError::None;

    if (abs(delta) <= cfg.positionTolerance && !wasMoving) {
        state = BlindState::Moving;  // finish through the normal path for logging
        moveStartMs = millis();
        finishMove("already there");
        return;
    }

    float spt = cfg.stepsPerTick;
    int32_t steps = lroundf(delta * spt);
    uint32_t v = cfg.maxSpeed, a = cfg.acceleration;
    if (durationS) {
        v = max<uint32_t>(15, (uint32_t)(abs(steps) / (float)durationS));
        v = min<uint32_t>(v, cfg.maxSpeed);
        a = max<uint32_t>(30, min<uint32_t>(cfg.acceleration, v));
    } else if (jog) {
        v = max<uint32_t>(200, cfg.maxSpeed * 2 / 3);
    }
    motor->setSpeedAccel(v, a);
    motor->moveTo(motor->position() + steps);

    if (!wasMoving) {
        moveStartTicks = cur;
        moveStartSteps = motor->position();
        moveStartMs = millis();
        badWindows = 0;
        resetWindow();
    }
    etaMs = (uint32_t)(1000.0f * abs(steps) / v + 1000.0f * v / a) + (millis() - moveStartMs);
    int dirSign = cfg.calibrated() ? (cfg.openTicks > cfg.closedTicks ? 1 : -1) : 1;
    direction = (delta > 0 ? 1 : -1) * dirSign;
    state = BlindState::Moving;
}

void startRatio() {
    if (state == BlindState::Moving) return;
    if (!enc->connected()) {
        setError(BlindError::EncoderLost);
        return;
    }
    error = BlindError::None;
    state = BlindState::MeasuringRatio;
    ratioPhase = 1;
    ratioT0 = enc->position();
    ratioS0 = motor->position();
    // ~2 motor revolutions: 200 full steps/rev x microsteps
    int32_t n = 2 * 200 * (motor->uartConnected() ? cfg.microsteps : 8);
    motor->setSpeedAccel(min<uint32_t>(cfg.maxSpeed, 2000), cfg.acceleration);
    motor->moveTo(ratioS0 + n);
    settling = false;
    moveStartMs = millis();
    LOGI("blinds", "measuring ratio: +%ld steps", (long)n);
}

void superviseRatio() {
    if (motor->isRunning()) return;
    if (!settling) {
        settling = true;
        settleSince = millis();
        return;
    }
    if (millis() - settleSince < 250) return;
    settling = false;
    int32_t t = enc->position(), s = motor->position();
    if (ratioPhase == 1) {
        ratioDT1 = t - ratioT0;
        ratioDS1 = s - ratioS0;
        ratioPhase = 2;
        ratioT0 = t;
        ratioS0 = s;
        motor->moveTo(s - ratioDS1);  // come back
        return;
    }
    int32_t dT2 = t - ratioT0, dS2 = s - ratioS0;
    LOGI("blinds", "ratio: fwd %ld steps -> %ld ticks, back %ld steps -> %ld ticks", (long)ratioDS1, (long)ratioDT1,
         (long)dS2, (long)dT2);
    state = BlindState::Idle;
    if (abs(ratioDT1) < 150 || abs(dT2) < 150) {
        setError(BlindError::RatioFailed);
        LOGE("blinds", "encoder barely moved: check magnet, coupling and motor power");
        return;
    }
    float ratio = (float)(abs(ratioDS1) + abs(dS2)) / (float)(abs(ratioDT1) + abs(dT2));
    bool flip = (ratioDT1 > 0) != (ratioDS1 > 0);
    float asym = fabsf((float)abs(ratioDT1) - abs(dT2)) / abs(ratioDT1);
    if (asym > 0.1f) LOGW("blinds", "forward/back differ by %.0f%% (slip or backlash?)", asym * 100);
    settings::update([&](Settings& s) {
        s.stepsPerTick = ratio;
        s.ratioMeasured = true;
        if (flip) {
            // keep "positive steps -> positive ticks"; mirror stored positions
            s.encoderReversed = !s.encoderReversed;
            s.openTicks = -s.openTicks;
            s.closedTicks = -s.closedTicks;
        }
    });
    if (flip) {
        enc->setReversed(!enc->reversed());
        lastSaved = INT32_MIN;
    }
    LOGI("blinds", "ratio = %.5f steps/tick%s", ratio, flip ? " (encoder direction flipped)" : "");
    savePosition(true);
}

void supervise() {
    if (state == BlindState::MeasuringRatio) {
        superviseRatio();
        return;
    }
    if (state != BlindState::Moving) return;

    int32_t ticks = enc->position();
    int32_t steps = motor->position();
    float spt = cfg.stepsPerTick;
    uint32_t now = millis();

    if (motor->isRunning()) {
        settling = false;
        // 1) window check: is the encoder following the motor at all?
        uint32_t elapsed = now - winStart;
        float exp = (steps - winSteps0) / spt;
        if ((elapsed >= 150 && fabsf(exp) >= 100) || elapsed >= 3000) {
            if (fabsf(exp) >= 60) {
                float act = (float)(ticks - winTicks0) * (exp > 0 ? 1 : -1);
                badWindows = act < 0.3f * fabsf(exp) ? badWindows + 1 : 0;
            }
            resetWindow();
            if (badWindows >= 2) {
                setError(BlindError::Stall);
                return;
            }
        }
        // 2) following error (only meaningful once the ratio is measured)
        if (cfg.ratioMeasured) {
            float fe = (steps - moveStartSteps) / spt - (float)(ticks - moveStartTicks);
            if (fabsf(fe) > cfg.stallTolerance) {
                LOGW("blinds", "following error %.0f ticks", fe);
                setError(BlindError::Stall);
            }
        }
        if (now - lastSaveMs > 5000) savePosition(false);  // survive power loss mid-move
        return;
    }

    // motor stopped: settle, then verify against the encoder
    if (!settling) {
        settling = true;
        settleSince = now;
        return;
    }
    if (now - settleSince < SETTLE_MS) return;
    settling = false;
    if (stopping) {
        targetTicks = ticks;
        finishMove("stopped");
        return;
    }
    int32_t err = targetTicks - ticks;
    if (abs(err) > cfg.positionTolerance && corrections < MAX_CORRECTIONS) {
        int32_t s = lroundf(err * spt);
        if (s != 0) {
            corrections++;
            motor->setSpeedAccel(min<uint32_t>(cfg.maxSpeed, 1200), cfg.acceleration);
            motor->moveTo(steps + s);
            resetWindow();
            return;
        }
    }
    finishMove("arrived");
}

void applySettings(bool force) {
    uint32_t rev = settings::revision();
    if (!force && rev == cfgRev) return;
    cfgRev = rev;
    cfg = settings::get();
    bool idle = !motor->isRunning();

    if (idle && cfg.motorReversed != appliedMotorReversed) {
        // Flip motor and encoder together so "positive steps -> positive ticks"
        // still holds; only the physical jog direction changes.
        appliedMotorReversed = cfg.motorReversed;
        motor->setReversed(cfg.motorReversed);
        enc->setReversed(!enc->reversed());
        settings::update([](Settings& s) {
            s.encoderReversed = !s.encoderReversed;
            s.openTicks = -s.openTicks;
            s.closedTicks = -s.closedTicks;
        });
        cfg = settings::get();
        cfgRev = settings::revision();
        lastSaved = INT32_MIN;
        savePosition(true);
    }
    if (idle && cfg.encoderReversed != enc->reversed()) {
        // edited by hand (web/serial): mirror stored positions to stay consistent
        enc->setReversed(cfg.encoderReversed);
        settings::update([](Settings& s) {
            s.openTicks = -s.openTicks;
            s.closedTicks = -s.closedTicks;
        });
        cfg = settings::get();
        cfgRev = settings::revision();
        savePosition(true);
    }
    if (cfg.holdWhenIdle != appliedHold) {
        appliedHold = cfg.holdWhenIdle;
        motor->setHold(appliedHold);
    }
    if (motor->uartConnected() &&
        (cfg.microsteps != appliedMicrosteps || cfg.runCurrent != appliedCurrent ||
         cfg.holdCurrentPct != appliedHoldPct || cfg.stealthChop != appliedStealth)) {
        if (appliedMicrosteps && cfg.microsteps != appliedMicrosteps && cfg.ratioMeasured) {
            float scaled = cfg.stepsPerTick * cfg.microsteps / appliedMicrosteps;
            settings::update([scaled](Settings& s) { s.stepsPerTick = scaled; });
            cfg.stepsPerTick = scaled;
            cfgRev = settings::revision();
        }
        xSemaphoreTake(driverMtx, portMAX_DELAY);
        motor->configureDriver(cfg.runCurrent, cfg.holdCurrentPct, cfg.microsteps, cfg.stealthChop);
        xSemaphoreGive(driverMtx);
        appliedMicrosteps = cfg.microsteps;
        appliedCurrent = cfg.runCurrent;
        appliedHoldPct = cfg.holdCurrentPct;
        appliedStealth = cfg.stealthChop;
        LOGI("blinds", "TMC2209: %u mA, 1/%u, %s", cfg.runCurrent, cfg.microsteps, cfg.stealthChop ? "stealthChop" : "spreadCycle");
    }
}

void handle(const Cmd& c) {
    if (c.type != CmdType::Stop && c.type != CmdType::ClearError) {
        if (state == BlindState::Error && c.src == CmdSource::Schedule) {
            LOGW("blinds", "schedule ignored: in error state (%s)", Blinds::errorName(error));
            return;
        }
        if (state == BlindState::MeasuringRatio) {
            LOGW("blinds", "busy measuring ratio, command ignored");
            return;
        }
    }
    switch (c.type) {
        case CmdType::GotoPercent: {
            if (!cfg.calibrated()) {
                LOGW("blinds", "not calibrated: set open and closed positions first");
                return;
            }
            float p = constrain(c.value, 0.0f, 100.0f);
            int32_t t = cfg.closedTicks + lroundf((cfg.openTicks - cfg.closedTicks) * p / 100.0f);
            LOGI("blinds", "%s: go to %.0f%%%s", Blinds::sourceName(c.src), p, c.duration ? " (gentle)" : "");
            startMove(t, c.duration, false);
            if (state == BlindState::Moving) targetPct = (int8_t)lroundf(p);
            break;
        }
        case CmdType::GotoTicks:
            startMove(c.ticks, c.duration, false);
            break;
        case CmdType::Jog: {
            int32_t base = (state == BlindState::Moving && jogMode && !stopping) ? targetTicks : enc->position();
            startMove(base + c.ticks, 0, true);
            break;
        }
        case CmdType::Stop:
            if (state == BlindState::Moving) {
                motor->stop();
                stopping = true;
                targetPct = -1;
                LOGI("blinds", "%s: stop", Blinds::sourceName(c.src));
            } else if (state == BlindState::MeasuringRatio) {
                motor->stop();
                state = BlindState::Idle;
                LOGW("blinds", "ratio measurement aborted");
            }
            break;
        case CmdType::SetOpen:
        case CmdType::SetClosed: {
            int32_t pos = enc->position();
            bool open = c.type == CmdType::SetOpen;
            settings::update([&](Settings& s) {
                if (open) {
                    s.openTicks = pos;
                    s.openSet = true;
                } else {
                    s.closedTicks = pos;
                    s.closedSet = true;
                }
            });
            LOGI("blinds", "%s position set to %ld ticks", open ? "OPEN" : "CLOSED", (long)pos);
            savePosition(true);
            break;
        }
        case CmdType::MeasureRatio:
            startRatio();
            break;
        case CmdType::ClearError:
            if (state == BlindState::Error) state = BlindState::Idle;
            error = BlindError::None;
            break;
        case CmdType::ResetCal:
            settings::update([](Settings& s) { s.openSet = s.closedSet = false; });
            LOGI("blinds", "calibration reset");
            break;
    }
}

void publish() {
    BlindStatus st;
    st.state = state;
    st.error = error;
    st.ticks = enc->position();
    st.targetTicks = targetTicks;
    st.percent = pctOf(st.ticks);
    st.targetPercent = targetPct;
    st.direction = direction;
    st.calibrated = cfg.calibrated();
    st.encoderOk = enc->connected();
    st.magnetOk = enc->connected();
    st.tmcUart = motor->uartConnected();
    st.simulated = simulated;
    st.rawAngle = enc->rawAngle();
    st.motorSteps = motor->position();
    st.moveStartMs = moveStartMs;
    st.moveEtaMs = etaMs;
    st.stepsPerTick = cfg.stepsPerTick;
    xSemaphoreTake(statusMtx, portMAX_DELAY);
    published = st;
    xSemaphoreGive(statusMtx);
}

void persistIdle() {
    if (state == BlindState::Moving || state == BlindState::MeasuringRatio) return;
    // blind moved by hand (motor released): save once it has been still for 3 s
    int32_t pos = enc->position();
    uint32_t now = millis();
    if (abs(pos - stableTicks) > 2) {
        stableTicks = pos;
        stableSince = now;
    } else if (now - stableSince > 3000 && abs(pos - lastSaved) > 16) {
        savePosition(false);
    }
}

void motionTask(void*) {
    TickType_t last = xTaskGetTickCount();
    uint32_t lastErrLog = 0;
    for (;;) {
        enc->update();
        motor->tick();
        if (enc->consecutiveErrors() > 25 && (state == BlindState::Moving || state == BlindState::MeasuringRatio)) {
            setError(BlindError::EncoderLost);
        } else if (enc->consecutiveErrors() == 26 && millis() - lastErrLog > 5000) {
            lastErrLog = millis();
            LOGE("blinds", "AS5600 not responding");
        }
        applySettings(false);
        Cmd c;
        while (xQueueReceive(queue, &c, 0) == pdTRUE) handle(c);
        supervise();
        persistIdle();
        publish();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(LOOP_MS));
    }
}

bool send(const Cmd& c) { return queue && xQueueSend(queue, &c, pdMS_TO_TICKS(20)) == pdTRUE; }

}  // namespace

// =================================================================== public

bool Blinds::begin(bool simulate) {
    simulated = simulate;
    statusMtx = xSemaphoreCreateMutex();
    driverMtx = xSemaphoreCreateMutex();
    queue = xQueueCreate(16, sizeof(Cmd));
    cfg = settings::get();
    cfgRev = settings::revision();

    if (simulate) {
        sim = new SimStepper();
        motor = sim;
        enc = new SimMagneticEncoder(sim);
    } else {
        motor = new FastStepper(PIN_MOTOR_STEP, PIN_MOTOR_DIR, PIN_MOTOR_EN);
        enc = new MagneticEncoder();
    }

    bool ok = motor->begin();
    if (!ok) LOGE("blinds", "stepper init failed");
    if (!simulate) {
        auto* fs = static_cast<FastStepper*>(motor);
        if (fs->beginUart(Serial2, PIN_TMC_RX, PIN_TMC_TX, TMC_R_SENSE, TMC_ADDRESS)) {
            LOGI("blinds", "TMC2209 UART connected");
        } else {
            LOGI("blinds", "TMC2209 UART not detected: standalone STEP/DIR mode");
        }
    }
    motor->setReversed(cfg.motorReversed);
    appliedMotorReversed = cfg.motorReversed;
    motor->setHold(cfg.holdWhenIdle);
    appliedHold = cfg.holdWhenIdle;

    int32_t saved = 0;
    bool haveSaved = settings::loadPosition(saved);
    if (simulate && haveSaved) sim->mechTicks = cfg.encoderReversed ? -saved : saved;

    enc->setReversed(cfg.encoderReversed);
    if (!enc->begin()) {
        LOGE("blinds", "AS5600 not found! check I2C wiring (SDA %d, SCL %d) and the DIR pin", PIN_I2C_SDA, PIN_I2C_SCL);
        ok = false;
    } else {
        if (haveSaved) enc->restore(saved);
        LOGI("blinds", "encoder at %ld ticks (raw %u)%s", (long)enc->position(), enc->rawAngle(),
             haveSaved ? " restored" : "");
    }
    lastSaved = enc->position();
    stableTicks = lastSaved;

    if (simulate && !cfg.calibrated()) {
        settings::update([](Settings& s) {
            s.closedTicks = 0;
            s.openTicks = SIM_OPEN_TICKS;
            s.openSet = s.closedSet = true;
            s.stepsPerTick = 1600.0f / ENCODER_CPR;
            s.ratioMeasured = true;
        });
        cfg = settings::get();
        cfgRev = settings::revision();
        LOGI("blinds", "simulator: auto-calibrated 0..%ld ticks", (long)SIM_OPEN_TICKS);
    }

    applySettings(true);
    publish();
    xTaskCreatePinnedToCore(motionTask, "motion", 6144, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(persistTask, "persist", 3072, nullptr, 1, nullptr, 0);
    LOGI("blinds", "ready (%s), %s, %.1f%% open", simulate ? "SIMULATED" : "hardware",
         cfg.calibrated() ? "calibrated" : "NOT calibrated", pctOf(enc->position()));
    return ok;
}

bool Blinds::open(uint16_t d, CmdSource src) { return moveToPercent(100, d, src); }
bool Blinds::close(uint16_t d, CmdSource src) { return moveToPercent(0, d, src); }

bool Blinds::moveToPercent(float pct, uint16_t durationS, CmdSource src) {
    BlindStatus st = status();
    if (!st.calibrated) {
        LOGW("blinds", "%s: rejected, not calibrated", sourceName(src));
        return false;
    }
    if (st.state == BlindState::Error && src == CmdSource::Schedule) return false;
    return send({CmdType::GotoPercent, pct, 0, durationS, src});
}

bool Blinds::toggle(CmdSource src) {
    BlindStatus st = status();
    if (st.state == BlindState::Moving) {
        stop(src);
        return true;
    }
    return st.percent >= 50 ? close(0, src) : open(0, src);
}

void Blinds::stop(CmdSource src) { send({CmdType::Stop, 0, 0, 0, src}); }
void Blinds::jog(int32_t ticks) { send({CmdType::Jog, 0, ticks, 0, CmdSource::Knob}); }
void Blinds::setOpenHere() { send({CmdType::SetOpen, 0, 0, 0, CmdSource::System}); }
void Blinds::setClosedHere() { send({CmdType::SetClosed, 0, 0, 0, CmdSource::System}); }
void Blinds::measureRatio() { send({CmdType::MeasureRatio, 0, 0, 0, CmdSource::System}); }
void Blinds::clearError() { send({CmdType::ClearError, 0, 0, 0, CmdSource::System}); }
void Blinds::resetCalibration() { send({CmdType::ResetCal, 0, 0, 0, CmdSource::System}); }

BlindStatus Blinds::status() {
    BlindStatus s;
    if (!statusMtx) return s;
    xSemaphoreTake(statusMtx, portMAX_DELAY);
    s = published;
    xSemaphoreGive(statusMtx);
    return s;
}

String Blinds::driverReport() {
    if (!motor) return "not initialised";
    xSemaphoreTake(driverMtx, portMAX_DELAY);
    String r = motor->report();
    xSemaphoreGive(driverMtx);
    return r;
}

const char* Blinds::stateName(BlindState s) {
    switch (s) {
        case BlindState::Idle: return "idle";
        case BlindState::Moving: return "moving";
        case BlindState::MeasuringRatio: return "measuring";
        case BlindState::Error: return "error";
    }
    return "?";
}

const char* Blinds::errorName(BlindError e) {
    switch (e) {
        case BlindError::None: return "none";
        case BlindError::Stall: return "motor stalled";
        case BlindError::EncoderLost: return "encoder lost";
        case BlindError::NotCalibrated: return "not calibrated";
        case BlindError::RatioFailed: return "ratio measurement failed";
    }
    return "?";
}

const char* Blinds::sourceName(CmdSource s) {
    switch (s) {
        case CmdSource::Knob: return "knob";
        case CmdSource::Web: return "web";
        case CmdSource::Schedule: return "schedule";
        case CmdSource::Serial: return "serial";
        case CmdSource::System: return "system";
    }
    return "?";
}
