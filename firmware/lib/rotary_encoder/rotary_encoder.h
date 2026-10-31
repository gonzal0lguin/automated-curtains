#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Arduino.h>

// EC11-style rotary encoder with push button.
//  - Rotation is decoded in a GPIO interrupt with a quadrature state table
//    (contact bounce cancels itself out).
//  - The button is polled from tick() for robust debouncing and to detect
//    click / long-press events.
class RotaryEncoder {
public:
    enum class Event : uint8_t { None, Click, LongPress };

    RotaryEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW, uint8_t transitionsPerDetent = 4);

    void begin();

    // Call every few ms from the UI loop (button debouncing / events).
    void tick();

    // --- rotation
    int getDelta();          // detents since the previous call (signed)
    int getPosition();       // absolute detent count since boot
    long getRawCount();      // raw quadrature transitions (for calibration/debug)
    void setReversed(bool reversed) { _reversed = reversed; }

    // --- button
    Event getEvent();        // pops the next pending button event
    bool isPressed() const { return _stable; }
    bool isClicked();        // convenience: true once per Click event

    void setLongPressMs(uint16_t ms) { _longPressMs = ms; }

private:
    static void IRAM_ATTR isrEncoder(void* arg);
    void pushEvent(Event e);

    uint8_t _pinA, _pinB, _pinSW, _perDetent;
    bool _reversed = false;

    volatile int32_t _raw = 0;
    volatile uint8_t _state = 0;
    int32_t _consumedRaw = 0;
    int32_t _position = 0;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

    // button state (only touched from tick()/getEvent(), same task)
    bool _stable = false, _lastRead = false, _longFired = false;
    uint32_t _changedAt = 0, _pressedAt = 0;
    uint16_t _debounceMs = 25, _longPressMs = 700;
    Event _queue[4];
    uint8_t _qHead = 0, _qLen = 0;
};

#endif
