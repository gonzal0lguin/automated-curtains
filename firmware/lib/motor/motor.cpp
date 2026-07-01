#include "motor.h"

// Initialize AccelStepper in DRIVER mode (1), specifying step and dir pins
Motor::Motor(uint8_t stepPin, uint8_t dirPin, uint8_t enPin) 
    : stepper(AccelStepper::DRIVER, stepPin, dirPin), enablePin(enPin) {}

void Motor::begin() {
    pinMode(enablePin, OUTPUT);
    digitalWrite(enablePin, LOW); // LOW usually enables TMC2208

    stepper.setMaxSpeed(10000.0);     // Set maximum steps per second
    stepper.setAcceleration(500.0);  // Set acceleration steps per second squared
}

void Motor::moveToPosition(long targetPosition) {
    stepper.moveTo(targetPosition);
}

void Motor::update() {
    // This is the magic non-blocking function. 
    // It checks if a step is due based on the current time and acceleration curve.
    // If it is, it pulses the pin. If not, it does nothing and returns instantly.
    stepper.run();
}

void Motor::stop() {
    stepper.stop(); // Sets a new target based on current speed and deceleration curve
}