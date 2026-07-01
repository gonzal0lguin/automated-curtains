#ifndef MOTOR_H
#define MOTOR_H

#include <AccelStepper.h>

class Motor {
private:
    AccelStepper stepper;
    uint8_t enablePin;

public:
    // Constructor
    Motor(uint8_t stepPin, uint8_t dirPin, uint8_t enPin);

    // Initialize motor settings
    void begin();

    // Set a new target position
    void moveToPosition(long targetPosition);

    // Non-blocking run function (must be called continuously in the loop)
    void update();

    // Instantly stop the motor
    void stop();
};

#endif