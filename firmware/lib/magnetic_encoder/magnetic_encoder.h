#ifndef MAGNETIC_ENCODER_H
#define MAGNETIC_ENCODER_H
// AS5600 magnetic encoder with multi-turn tracking.
//
// The AS5600 is absolute only within one revolution (0..4095). update() must
// be called often (every few ms) so a wrap-around is never missed; the
// running total is the multi-turn position in "ticks".
//
// After a reboot, restore() re-attaches the saved multi-turn position to the
// current single-turn reading (valid as long as the shaft did not move more
// than half a turn while powered off).
#include <AS5600.h>
#include <Wire.h>

#include <atomic>

#include "config.h"

class MagneticEncoder {
public:
    virtual ~MagneticEncoder() {}

    virtual bool begin(int sda = PIN_I2C_SDA, int scl = PIN_I2C_SCL, uint32_t freq = I2C_FREQ_HZ);
    bool update();                    // read + unwrap; false on I2C error

    int32_t position() const { return _pos.load(); }
    uint16_t rawAngle() const { return _lastRaw; }
    bool connected() const { return _connected; }
    uint32_t errorCount() const { return _errors; }
    uint32_t consecutiveErrors() const { return _consecutive; }

    void restore(int32_t savedPosition);
    void setPosition(int32_t position);
    // Flips counting direction. The position is negated so that previously
    // stored tick values stay consistent if the caller negates them too.
    void setReversed(bool reversed);
    bool reversed() const { return _reversed; }

    struct Diag {
        bool connected = false, magnet = false, tooWeak = false, tooStrong = false;
        uint8_t agc = 0;
        uint16_t magnitude = 0;
    };
    virtual Diag diagnostics();

    // --- legacy calibration API (kept for the old examples)
    void setOpenPosition() { _open = position(); }
    void setClosedPosition() { _closed = position(); }
    long getOpenPosition() const { return _open; }
    long getClosedPosition() const { return _closed; }
    long getAbsolutePosition() const { return position(); }
    int getRawAngle() const { return _lastRaw; }

protected:
    virtual bool readRaw(uint16_t& raw);
    uint16_t transform(uint16_t raw) const { return _reversed ? (ENCODER_CPR - raw) & (ENCODER_CPR - 1) : raw; }

    AS5600 _as{&Wire};
    std::atomic<int32_t> _pos{0};
    uint16_t _lastRaw = 0;  // already direction-transformed
    bool _reversed = false;
    bool _connected = false;
    uint32_t _errors = 0, _consecutive = 0;
    long _open = 0, _closed = 0;
};

class SimStepper;

// Reads the simulated mechanism instead of the I2C sensor.
class SimMagneticEncoder : public MagneticEncoder {
public:
    explicit SimMagneticEncoder(SimStepper* motor) : _motor(motor) {}
    bool begin(int = 0, int = 0, uint32_t = 0) override;
    Diag diagnostics() override;

protected:
    bool readRaw(uint16_t& raw) override;
    SimStepper* _motor;
};

#endif
