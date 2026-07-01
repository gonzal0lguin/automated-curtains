#include "magnetic_encoder.h"

MagneticEncoder::MagneticEncoder() {
    _lastRawAngle = 0;
    _currentAbsolutePosition = 0;
    _openPosition = 0;
    _closedPosition = 0;
}

bool MagneticEncoder::begin(uint8_t sdaPin, uint8_t sclPin) {
    // Start I2C bus
    Wire.begin(sdaPin, sclPin);
    _as5600.begin();
    
    if (!_as5600.isConnected()) {
        return false;
    }

    // Initialize the starting position
    _lastRawAngle = _as5600.rawAngle();
    _currentAbsolutePosition = _lastRawAngle;
    
    return true;
}

void MagneticEncoder::update() {
    // 1. Get current 0-4095 reading
    int currentRawAngle = _as5600.rawAngle();
    
    // 2. Calculate the difference since the last check
    int delta = currentRawAngle - _lastRawAngle;
    
    // 3. Detect overflow/underflow (crossing the 0/360 boundary)
    // 2048 is half a rotation. If the delta jumps by more than half a rotation 
    // between reads, it means we crossed the boundary.
    if (delta > 2048) {
        // Spun counter-clockwise past 0
        delta -= 4096;
    } else if (delta < -2048) {
        // Spun clockwise past 4095
        delta += 4096; 
    }
    
    // 4. Update total running position
    _currentAbsolutePosition += delta;
    _lastRawAngle = currentRawAngle;
}

long MagneticEncoder::getAbsolutePosition() {
    return _currentAbsolutePosition;
}

int MagneticEncoder::getRawAngle() {
    return _as5600.rawAngle();
}

void MagneticEncoder::setOpenPosition() {
    _openPosition = _currentAbsolutePosition;
}

void MagneticEncoder::setClosedPosition() {
    _closedPosition = _currentAbsolutePosition;
}

int MagneticEncoder::getPercentage() {
    // Prevent divide-by-zero if uncalibrated
    if (_openPosition == _closedPosition) return 0; 
    
    // Map the current position between the closed (0%) and open (100%) limits
    // Note: Depends on which direction rolls the blinds up! 
    // You might need to use absolute values or map() here depending on mechanical setup.
    long totalRange = _openPosition - _closedPosition;
    long currentProgress = _currentAbsolutePosition - _closedPosition;
    
    int percentage = (currentProgress * 100) / totalRange;
    
    // Constrain to 0-100 just in case it stretches slightly past limits
    if (percentage < 0) return 0;
    if (percentage > 100) return 100;
    
    return percentage;
}