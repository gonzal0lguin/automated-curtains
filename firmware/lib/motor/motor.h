#ifndef MOTOR_H
#define MOTOR_H

#include <AccelStepper.h>
#include "magnetic_encoder.h" // Your previously built AS5600 library

class Motor {
private:
    AccelStepper stepper;
    MagneticEncoder* encoder; // Pointer to the encoder instance
    
    uint8_t enablePin;
    float stepsPerEncoderTick; // The ratio of motor steps to one AS5600 tick
    
    // Stall detection variables
    long targetEncoderPosition;
    long lastEncoderValue;
    unsigned long lastEncoderMoveTime;
    unsigned long stallTimeoutMs;
    bool isMoving;
    bool isStalled;

public:
    // Constructor
    Motor(uint8_t stepPin, uint8_t dirPin, uint8_t enPin, MagneticEncoder* enc);

    // Initialization and settings
    void begin(float stepsPerTick);
    void setSpeedAndAccel(float maxSpeed, float acceleration);
    void setStallTimeout(unsigned long timeoutMs);

    // Movement commands
    void moveToEncoderPosition(long targetTicks);
    
    // The core non-blocking loop
    void update();

    // Emergency stops & state checks
    void stop();
    bool getStallState();
    void clearStallState();
};

#endif