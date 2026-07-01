// #include <Arduino.h>
// #include "motor.h"
// #include "rotary_encoder.h"

// // Define your ESP32 pins connected to the TMC2208
// #define DIR_PIN 14
// #define STEP_PIN 27
// #define EN_PIN 12

// #define RENC_PIN_A 32
// #define RENC_PIN_B 33
// #define RENC_PIN_SW 25

// Motor myMotor(STEP_PIN, DIR_PIN, EN_PIN);
// RotaryEncoder uiEncoder(RENC_PIN_A, RENC_PIN_B, RENC_PIN_SW);


// // FreeRTOS Task Handle
// TaskHandle_t MotorTaskHandle;

// // The function that will run endlessly on Core 1
// void MotorTask(void *pvParameters) {
//     for (;;) {
//         // This runs constantly. Because it's non-blocking, it executes 
//         // extremely fast and allows the RTOS to process other things.
//         myMotor.update(); 
        
//         // A tiny 1-tick delay yields the task just enough to keep the 
//         // Watchdog Timer happy without messing up the motor timing.
//         // vTaskDelay(pdMS_TO_TICKS(1)); 
//     }
// }

// void setup() {
//     Serial.begin(115200);
//     myMotor.begin();

//     // Tell the motor to move 5000 steps as a test
//     myMotor.moveToPosition(5000);

//     // Pin the MotorTask to Core 1
//     // xTaskCreatePinnedToCore(
//     //     MotorTask,         // Task function
//     //     "MotorTask",       // Name of task
//     //     10000,             // Stack size (bytes)
//     //     NULL,              // Task parameters
//     //     1,                 // Priority (1 is standard)
//     //     &MotorTaskHandle,  // Task handle
//     //     1                  // Core ID (0 or 1)
//     // );
//     // myMotor.update(); 
// }
 
// void loop() {
//     // Core 0 loop (Main loop). We do absolutely nothing here right now.
//     // Eventually, the webserver and LCD logic will live here.
//     // myMotor.moveToPosition(5000);

//     // // Pin the MotorTask to Core 1
//     // xTaskCreatePinnedToCore(
//     //     MotorTask,         // Task function
//     //     "MotorTask",       // Name of task
//     //     10000,             // Stack size (bytes)
//     //     NULL,              // Task parameters
//     //     1,                 // Priority (1 is standard)
//     //     &MotorTaskHandle,  // Task handle
//     //     1                  // Core ID (0 or 1)
//     // );
//     myMotor.update(); 
// }

#include <Arduino.h>
#include "rotary_encoder.h"

// Define the GPIO pins connected to your ESP32.
// Adjust these numbers to match your actual breadboard wiring!
const uint8_t ENCODER_PIN_A = 32;
const uint8_t ENCODER_PIN_B = 33;
const uint8_t ENCODER_SW = 25;

// Instantiate the encoder object
RotaryEncoder myEncoder(ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_SW);

void setup() {
    // Start serial communication at a high baud rate
    Serial.begin(115200);
    
    // Give the serial monitor a moment to connect
    delay(1000); 
    Serial.println("\n--- Rotary Encoder Module Test ---");
    Serial.println("Turn the knob or press the button.");

    // Initialize the encoder (this configures pins and attaches ISRs)
    myEncoder.begin();
}

void loop() {
    // 1. Check for rotation using our handy delta function
    int move = myEncoder.getDelta();
    
    if (move != 0) {
        if (move > 0) {
            Serial.print("-> Turned Right (+). ");
        } else {
            Serial.print("<- Turned Left (-). ");
        }
        
        // Fetch and print the absolute position just to verify it's tracking correctly
        Serial.print("Absolute Position: ");
        Serial.println(myEncoder.getPosition());
    }

    // 2. Check for button clicks
    if (myEncoder.isClicked()) {
        Serial.println("🔘 BUTTON CLICKED! (Menu Selection Triggered)");
    }

    // A tiny delay yields time to FreeRTOS background tasks 
    // and prevents the idle loop from triggering the Watchdog Timer.
    delay(10); 
}