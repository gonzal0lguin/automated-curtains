#include "magnetic_encoder.h"

#include <math.h>

#include "motor.h"

static constexpr int HALF = ENCODER_CPR / 2;

static int wrapDelta(int d) {
    if (d >= HALF) d -= ENCODER_CPR;
    else if (d < -HALF) d += ENCODER_CPR;
    return d;
}

bool MagneticEncoder::begin(int sda, int scl, uint32_t freq) {
    Wire.begin(sda, scl, freq);
    Wire.setTimeOut(5);
    _connected = _as.begin();
    if (!_connected) return false;
    uint16_t raw;
    if (!readRaw(raw)) return false;
    _lastRaw = transform(raw);
    _pos = _lastRaw;
    return true;
}

bool MagneticEncoder::readRaw(uint16_t& raw) {
    raw = _as.rawAngle();
    return _as.lastError() == AS5600_OK;
}

bool MagneticEncoder::update() {
    uint16_t raw;
    if (!readRaw(raw)) {
        _errors++;
        _consecutive++;
        if (_consecutive > 50) _connected = false;
        return false;
    }
    _consecutive = 0;
    _connected = true;
    raw = transform(raw);
    int delta = wrapDelta((int)raw - (int)_lastRaw);
    _lastRaw = raw;
    if (delta) _pos += delta;
    return true;
}

void MagneticEncoder::restore(int32_t saved) {
    int savedRaw = ((saved % ENCODER_CPR) + ENCODER_CPR) % ENCODER_CPR;
    _pos = saved + wrapDelta((int)_lastRaw - savedRaw);
}

void MagneticEncoder::setPosition(int32_t position) { _pos = position; }

void MagneticEncoder::setReversed(bool reversed) {
    if (reversed == _reversed) return;
    _reversed = reversed;
    // raw' = -raw (mod CPR) and position' = -position keep restore() consistent
    _lastRaw = (ENCODER_CPR - _lastRaw) & (ENCODER_CPR - 1);
    _pos = -_pos.load();
}

MagneticEncoder::Diag MagneticEncoder::diagnostics() {
    Diag d;
    d.connected = _as.isConnected();
    if (!d.connected) return d;
    d.magnet = _as.magnetDetected();
    d.tooWeak = _as.magnetTooWeak();
    d.tooStrong = _as.magnetTooStrong();
    d.agc = _as.readAGC();
    d.magnitude = _as.readMagnitude();
    return d;
}

// ======================================================= SimMagneticEncoder

bool SimMagneticEncoder::begin(int, int, uint32_t) {
    _connected = true;
    uint16_t raw;
    readRaw(raw);
    _lastRaw = transform(raw);
    _pos = _lastRaw;
    return true;
}

bool SimMagneticEncoder::readRaw(uint16_t& raw) {
    long t = lroundf(_motor->mechTicks);
    raw = (uint16_t)(((t % ENCODER_CPR) + ENCODER_CPR) % ENCODER_CPR);
    return true;
}

MagneticEncoder::Diag SimMagneticEncoder::diagnostics() {
    Diag d;
    d.connected = d.magnet = true;
    d.agc = 128;
    d.magnitude = 2000;
    return d;
}
