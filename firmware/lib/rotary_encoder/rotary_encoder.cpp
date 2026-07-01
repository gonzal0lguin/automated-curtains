#include "rotary_encoder.h"

// Define a debounce delay for the mechanical button (in milliseconds)
const unsigned long DEBOUNCE_DELAY_MS = 200;

RotaryEncoder::RotaryEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW)
    : _pinA(pinA), _pinB(pinB), _pinSW(pinSW), _position(0), _buttonPressed(false), _lastButtonPress(0), _lastReadClicks(0) {
}

void RotaryEncoder::begin() {
    pinMode(_pinA, INPUT_PULLUP);
    pinMode(_pinB, INPUT_PULLUP);
    pinMode(_pinSW, INPUT_PULLUP);

    // Initialize the starting state of the pins
    _encoderState = (digitalRead(_pinA) << 1) | digitalRead(_pinB);

    attachInterruptArg(digitalPinToInterrupt(_pinA), isrEncoder, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(_pinB), isrEncoder, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(_pinSW), isrButton, this, FALLING);
}

void IRAM_ATTR RotaryEncoder::isrEncoder(void* arg) {
    RotaryEncoder* encoder = static_cast<RotaryEncoder*>(arg);
    
    uint8_t stateA = digitalRead(encoder->_pinA);
    uint8_t stateB = digitalRead(encoder->_pinB);

    portENTER_CRITICAL_ISR(&(encoder->_mux));
    
    // Shift the old state left by 2 bits, and add the new state to the bottom 2 bits
    encoder->_encoderState = ((encoder->_encoderState << 2) | (stateA << 1) | stateB) & 0x0F;

    // State Machine Lookup Table
    // Only increments/decrements on valid quadrature sequences, eliminating bounce.
    if (encoder->_encoderState == 0b0001 || encoder->_encoderState == 0b0111 || 
        encoder->_encoderState == 0b1110 || encoder->_encoderState == 0b1000) {
        encoder->_position++; // Clockwise
    } 
    else if (encoder->_encoderState == 0b0010 || encoder->_encoderState == 0b1011 || 
             encoder->_encoderState == 0b1101 || encoder->_encoderState == 0b0100) {
        encoder->_position--; // Counter-Clockwise
    }
    
    portEXIT_CRITICAL_ISR(&(encoder->_mux));
}

void IRAM_ATTR RotaryEncoder::isrButton(void* arg) {
    RotaryEncoder* encoder = static_cast<RotaryEncoder*>(arg);
    
    // Grab current time for debouncing
    unsigned long currentTime = millis();

    portENTER_CRITICAL_ISR(&(encoder->_mux));
    // Check if enough time has passed since the last interrupt (debounce)
    if ((currentTime - encoder->_lastButtonPress) > DEBOUNCE_DELAY_MS) {
        encoder->_buttonPressed = true;
        encoder->_lastButtonPress = currentTime;
    }
    portEXIT_CRITICAL_ISR(&(encoder->_mux));
}

// --- UI Access Methods ---

int RotaryEncoder::getPosition() {
    int pos;
    portENTER_CRITICAL(&(this->_mux));
    // Divide raw pulses by 2 to get actual physical detent clicks
    pos = _position / 2; 
    portEXIT_CRITICAL(&(this->_mux));
    return pos;
}

int RotaryEncoder::getDelta() {
    int delta = 0;
    portENTER_CRITICAL(&(this->_mux));
    
    // Calculate total physical clicks made so far
    int currentClicks = _position / 2; 
    
    // The delta is just the difference between now and our last check
    delta = currentClicks - _lastReadClicks;
    
    // Save the current state for the NEXT time getDelta() is called
    _lastReadClicks = currentClicks; 
    
    portEXIT_CRITICAL(&(this->_mux));
    return delta;
}

bool RotaryEncoder::isClicked() {
    bool clicked = false;
    portENTER_CRITICAL(&(this->_mux));
    // If pressed, register it and immediately clear the flag (consume the click)
    if (_buttonPressed) {
        clicked = true;
        _buttonPressed = false;
    }
    portEXIT_CRITICAL(&(this->_mux));
    return clicked;
}
