#ifndef MAGNETIC_ENCODER_H
#define MAGNETIC_ENCODER_H

#include <Wire.h>
#include <AS5600.h>

class MagneticEncoder {
private:
    AS5600 _as5600;
    
    // Multi-turn tracking variables
    int _lastRawAngle;
    long _currentAbsolutePosition; // Tracks continuous total rotation
    
    // Stored limit positions for UI and Motor Control
    long _openPosition;
    long _closedPosition;

public:
    MagneticEncoder();

    // Initialization
    bool begin(uint8_t sdaPin, uint8_t sclPin);

    // CRITICAL: Must be called frequently in your Core 1 task loop
    void update(); 

    // Raw data getters
    long getAbsolutePosition(); // Feeds the PID controller
    int getRawAngle();          // 0-4095 single rotation reading

    bool detectMagnetLoss(); // Returns true if the magnet is too far away or misaligned

    // Calibration setters
    void setOpenPosition();     // Saves current position as 100% open
    void setClosedPosition();   // Saves current position as 0% closed
    long getOpenPosition();     // Returns the stored 100% open position
    long getClosedPosition();   // Returns the stored 0% closed position

    // UI Getter
    int getPercentage();        // Returns 0-100 for the LCD display
};

#endif