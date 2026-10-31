#include "motor.h"

#include <FastAccelStepper.h>
#include <TMCStepper.h>

static FastAccelStepperEngine s_engine;
static bool s_engineInit = false;

// ============================================================== FastStepper

bool FastStepper::begin() {
    if (!s_engineInit) {
        s_engine.init();
        s_engineInit = true;
    }
    _s = s_engine.stepperConnectToPin(_step);
    if (!_s) return false;
    _s->setDirectionPin(_dir, !_reversed);
    _s->setEnablePin(_en, true);  // TMC2209 EN is active low
    _s->setAutoEnable(true);
    _s->setDelayToEnable(2000);   // us, lets the driver wake before the first step
    _s->setDelayToDisable(800);   // ms
    _s->setSpeedInHz(1000);
    _s->setAcceleration(1000);
    return true;
}

void FastStepper::setSpeedAccel(uint32_t speedHz, uint32_t accel) {
    if (!_s) return;
    _s->setSpeedInHz(speedHz < 1 ? 1 : speedHz);
    _s->setAcceleration(accel < 1 ? 1 : accel);
    if (_s->isRunning()) _s->applySpeedAcceleration();
}

bool FastStepper::moveTo(int32_t position) {
    if (!_s) return false;
    return _s->moveTo(position) == MoveResultCode::OK;
}

void FastStepper::stop() {
    if (_s) _s->stopMove();
}

void FastStepper::forceStop() {
    if (_s) _s->forceStop();
}

bool FastStepper::isRunning() { return _s && _s->isRunning(); }

int32_t FastStepper::position() { return _s ? _s->getCurrentPosition() : 0; }

void FastStepper::setReversed(bool reversed) {
    _reversed = reversed;
    if (_s && !_s->isRunning()) _s->setDirectionPin(_dir, !_reversed);
}

void FastStepper::setHold(bool hold) {
    if (!_s) return;
    _s->setAutoEnable(!hold);
    if (hold) _s->enableOutputs();
    else if (!_s->isRunning()) _s->disableOutputs();
}

bool FastStepper::beginUart(HardwareSerial& serial, int rxPin, int txPin, float rSense, uint8_t address) {
    serial.begin(115200, SERIAL_8N1, rxPin, txPin);
    if (!_tmc) _tmc = new TMC2209Stepper(&serial, rSense, address);
    _tmc->begin();
    _uartOk = _tmc->test_connection() == 0;
    return _uartOk;
}

void FastStepper::configureDriver(uint16_t runMa, uint8_t holdPct, uint16_t microsteps, bool stealth) {
    if (!_uartOk) return;
    _tmc->pdn_disable(true);       // PDN_UART pin is used for UART
    _tmc->mstep_reg_select(true);  // microsteps from register, not MS1/MS2 pins
    _tmc->I_scale_analog(false);   // current from register, ignore the VREF pot
    _tmc->toff(4);
    _tmc->blank_time(24);
    _tmc->rms_current(runMa, holdPct / 100.0f);
    _tmc->iholddelay(8);
    _tmc->TPOWERDOWN(40);
    _tmc->microsteps(microsteps);
    _tmc->en_spreadCycle(!stealth);
    _tmc->pwm_autoscale(true);
    _tmc->pwm_autograd(true);
}

String FastStepper::report() {
    if (!_uartOk) return "TMC2209 standalone (UART not connected)";
    char buf[220];
    uint32_t st = _tmc->DRV_STATUS();
    snprintf(buf, sizeof(buf),
             "TMC2209 v0x%02X | %u mA RMS | 1/%u ustep | %s | cs=%u | SG=%u | %s%s%s%s%s",
             _tmc->version(), _tmc->rms_current(), _tmc->microsteps(),
             _tmc->stealth() ? "stealthChop" : "spreadCycle", _tmc->cs_actual(), _tmc->SG_RESULT(),
             _tmc->ot() ? "OVERTEMP " : (_tmc->otpw() ? "temp-warning " : "temp-ok "),
             (_tmc->s2ga() || _tmc->s2gb()) ? "SHORT-GND " : "",
             (_tmc->ola() || _tmc->olb()) ? "open-load " : "", _tmc->stst() ? "standstill" : "moving",
             st == 0xFFFFFFFF ? " (read error)" : "");
    return String(buf);
}

// =============================================================== SimStepper

bool SimStepper::moveTo(int32_t position) {
    _target = position;
    _running = true;
    return true;
}

void SimStepper::stop() {
    if (!_running) return;
    float dist = (_v * _v) / (2 * _acc);
    _target = _pos + (_v >= 0 ? dist : -dist);
}

void SimStepper::forceStop() {
    _v = 0;
    _target = _pos;
    _running = false;
}

void SimStepper::tick() {
    uint32_t now = micros();
    float dt = (now - _lastUs) / 1e6f;
    _lastUs = now;
    if (!_running || dt <= 0) return;
    if (dt > 0.05f) dt = 0.05f;

    float dist = _target - _pos;
    float dir = dist >= 0 ? 1.0f : -1.0f;
    float brake = (_v * _v) / (2 * _acc);
    bool sameDir = (_v * dir) >= 0;
    if (!sameDir || fabsf(dist) <= brake) {
        // decelerate toward zero
        float dv = _acc * dt;
        _v = fabsf(_v) <= dv ? 0 : _v - (_v > 0 ? dv : -dv);
    } else {
        _v += dir * _acc * dt;
        if (fabsf(_v) > _vmax) _v = dir * _vmax;
    }
    float step = _v * dt;
    _pos += step;
    if (fabsf(_target - _pos) < 1.0f && fabsf(_v) < _acc * 0.02f) {
        step += _target - _pos;
        _pos = _target;
        _v = 0;
        _running = false;
    }
    // the blind follows the motor until it hits a mechanical end
    float dTicks = (_reversed ? -step : step) / stepsPerTick;
    mechTicks += dTicks;
    if (mechTicks < mechMinTicks) mechTicks = mechMinTicks;
    if (mechTicks > mechMaxTicks) mechTicks = mechMaxTicks;
}
