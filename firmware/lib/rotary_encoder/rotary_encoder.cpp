#include "rotary_encoder.h"

#include <soc/gpio_reg.h>

// Index = (previous AB << 2) | current AB. Valid transitions give +-1,
// invalid ones (double jumps / bounce glitches) give 0.
static const int8_t DRAM_ATTR kQuadTable[16] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};

// Direct register read: safe inside an IRAM ISR even while flash is busy
// (e.g. while settings are being written to NVS).
static inline int IRAM_ATTR fastRead(uint8_t pin) {
    if (pin < 32) return (REG_READ(GPIO_IN_REG) >> pin) & 1;
    return (REG_READ(GPIO_IN1_REG) >> (pin - 32)) & 1;
}

RotaryEncoder::RotaryEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW, uint8_t transitionsPerDetent)
    : _pinA(pinA), _pinB(pinB), _pinSW(pinSW), _perDetent(transitionsPerDetent ? transitionsPerDetent : 1) {}

void RotaryEncoder::begin() {
    pinMode(_pinA, INPUT_PULLUP);
    pinMode(_pinB, INPUT_PULLUP);
    pinMode(_pinSW, INPUT_PULLUP);
    _state = (fastRead(_pinA) << 1) | fastRead(_pinB);
    attachInterruptArg(digitalPinToInterrupt(_pinA), isrEncoder, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(_pinB), isrEncoder, this, CHANGE);
    _stable = _lastRead = (digitalRead(_pinSW) == LOW);
    _longFired = _stable;  // held during boot: ignore until released
}

void IRAM_ATTR RotaryEncoder::isrEncoder(void* arg) {
    RotaryEncoder* self = static_cast<RotaryEncoder*>(arg);
    uint8_t ab = (fastRead(self->_pinA) << 1) | fastRead(self->_pinB);
    portENTER_CRITICAL_ISR(&self->_mux);
    uint8_t idx = ((self->_state << 2) | ab) & 0x0F;
    self->_raw = self->_raw + kQuadTable[idx];
    self->_state = ab;
    portEXIT_CRITICAL_ISR(&self->_mux);
}

long RotaryEncoder::getRawCount() {
    portENTER_CRITICAL(&_mux);
    long r = _raw;
    portEXIT_CRITICAL(&_mux);
    return r;
}

int RotaryEncoder::getDelta() {
    int32_t raw = getRawCount();
    // Truncation toward zero gives natural hysteresis: a contact resting on
    // the edge between two counts cannot generate +1/-1 jitter.
    int32_t detents = (raw - _consumedRaw) / _perDetent;
    _consumedRaw += detents * _perDetent;
    if (_reversed) detents = -detents;
    _position += detents;
    return detents;
}

int RotaryEncoder::getPosition() {
    getDelta();
    return _position;
}

void RotaryEncoder::tick() {
    uint32_t now = millis();
    bool pressed = digitalRead(_pinSW) == LOW;
    if (pressed != _lastRead) {
        _lastRead = pressed;
        _changedAt = now;
    }
    if (pressed != _stable && now - _changedAt >= _debounceMs) {
        _stable = pressed;
        if (pressed) {
            _pressedAt = now;
            _longFired = false;
        } else if (!_longFired) {
            pushEvent(Event::Click);
        }
    }
    if (_stable && !_longFired && now - _pressedAt >= _longPressMs) {
        _longFired = true;
        pushEvent(Event::LongPress);
    }
}

void RotaryEncoder::pushEvent(Event e) {
    if (_qLen == sizeof(_queue) / sizeof(_queue[0])) return;  // drop when full
    _queue[(_qHead + _qLen) % 4] = e;
    _qLen++;
}

RotaryEncoder::Event RotaryEncoder::getEvent() {
    if (!_qLen) return Event::None;
    Event e = _queue[_qHead];
    _qHead = (_qHead + 1) % 4;
    _qLen--;
    return e;
}

bool RotaryEncoder::isClicked() {
    for (uint8_t i = 0; i < _qLen; i++) {
        if (_queue[(_qHead + i) % 4] == Event::Click) {
            // remove that event, keep order of others
            for (uint8_t j = i; j + 1 < _qLen; j++) _queue[(_qHead + j) % 4] = _queue[(_qHead + j + 1) % 4];
            _qLen--;
            return true;
        }
    }
    return false;
}
