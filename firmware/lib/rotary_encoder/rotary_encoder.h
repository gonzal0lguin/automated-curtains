#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Arduino.h>

class RotaryEncoder {
public:
    RotaryEncoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW);

    // Initialization (call this in your setup / Task 1 init)
    void begin();

    // UI Control Methods
    int getPosition();       // Get absolute position
    int getDelta();          // Get movement since last check (perfect for menus)
    bool isClicked();        // Returns true once per press, consuming the click

private:
    uint8_t _pinA;
    uint8_t _pinB;
    uint8_t _pinSW;

    // Volatile variables modified by ISRs
    volatile int _position;
    volatile bool _buttonPressed;
    volatile unsigned long _lastButtonPress;
    
    // State tracker of A and B pins
    volatile uint8_t _encoderState; 

    // Used to read the absolute position of the encoder in terms of "clicks"
    int _lastReadClicks;

    // FreeRTOS Mutex for thread-safe variable access
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

    static void IRAM_ATTR isrEncoder(void* arg);
    static void IRAM_ATTR isrButton(void* arg);
};

#endif