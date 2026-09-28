#include "motor.h"

// Pass the AccelStepper config and store the encoder pointer
Motor::Motor(uint8_t stepPin, uint8_t dirPin, uint8_t enPin, MagneticEncoder* enc) 
    : stepper(AccelStepper::DRIVER, stepPin, dirPin), enablePin(enPin), encoder(enc) {
    isMoving = false;
    isStalled = false;
    stallTimeoutMs = 1000; // Default 1 second timeout
}

void Motor::begin(float stepsPerTick) {
    pinMode(enablePin, OUTPUT);
    digitalWrite(enablePin, LOW); // Enable TMC2208

    stepsPerEncoderTick = stepsPerTick;
    stepper.setMaxSpeed(1000.0);
    stepper.setAcceleration(500.0);
}

void Motor::setSpeedAndAccel(float maxSpeed, float acceleration) {
    stepper.setMaxSpeed(maxSpeed);
    stepper.setAcceleration(acceleration);
}

void Motor::setStallTimeout(unsigned long timeoutMs) {
    stallTimeoutMs = timeoutMs;
}

void Motor::moveToEncoderPosition(long targetTicks) {
    if (isStalled) return; // Prevent movement if we are in an error state

    targetEncoderPosition = targetTicks;
    long currentTicks = encoder->getAbsolutePosition();
    
    // Calculate how many ticks we need to move, and convert to steps
    long ticksToMove = targetEncoderPosition - currentTicks;
    long stepsToMove = ticksToMove * stepsPerEncoderTick;

    stepper.move(stepsToMove);
    
    isMoving = true;
    lastEncoderValue = currentTicks;
    lastEncoderMoveTime = millis();
}

void Motor::update() {
    // 1. Let AccelStepper do its non-blocking step generation
    stepper.run();

    // 2. If we are supposed to be moving, run our supervisor checks
    if (isMoving) {
        long currentTicks = encoder->getAbsolutePosition();
        
        // Check if the encoder has actually moved significantly (e.g., > 5 ticks to ignore noise)
        if (abs(currentTicks - lastEncoderValue) > 5) {
            lastEncoderValue = currentTicks;
            lastEncoderMoveTime = millis(); // Reset the timeout timer
        }

        // 3. STALL DETECTION: If the motor is stepping but the encoder hasn't moved recently
        if ((millis() - lastEncoderMoveTime) > stallTimeoutMs) {
            stop();
            isStalled = true;
            isMoving = false;
            Serial.println("ERROR: MOTOR STALL DETECTED!");
        }

        // 4. POSITION CORRECTION: If AccelStepper thinks it finished the move
        if (stepper.distanceToGo() == 0 && !isStalled) {
            // Check if we actually hit the target encoder ticks
            long errorTicks = targetEncoderPosition - currentTicks;
            
            // If we are off by more than an acceptable margin (e.g., 10 ticks)
            if (abs(errorTicks) > 10) {
                // Command a small correction move
                long correctionSteps = errorTicks * stepsPerEncoderTick;
                stepper.move(correctionSteps);
                lastEncoderMoveTime = millis(); // Reset timer for the correction move
            } else {
                // We successfully arrived!
                isMoving = false;
            }
        }
    }
}

void Motor::stop() {
    stepper.stop(); // Stops gracefully using the deceleration curve
    isMoving = false;
}

bool Motor::getStallState() {
    return isStalled;
}

void Motor::clearStallState() {
    isStalled = false;
}