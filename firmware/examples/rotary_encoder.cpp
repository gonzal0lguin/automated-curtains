#include <Arduino.h>
#include "rotary_encoder.h"

// Define the GPIO pins connected to your ESP32.
// Adjust these numbers to match your actual breadboard wiring!
const uint8_t ENCODER_PIN_A = 32;
const uint8_t ENCODER_PIN_B = 33;
const uint8_t ENCODER_SW = 25;

// Instantiate the encoder object
RotaryEncoder encoder(ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_SW);

void setup() {
    // Start serial communication at a high baud rate
    Serial.begin(115200);
    
    // Give the serial monitor a moment to connect
    delay(1000); 
    Serial.println("\n--- Rotary Encoder Module Test ---");
    Serial.println("Turn the knob or press the button.");

    // Initialize the encoder (this configures pins and attaches ISRs)
    encoder.begin();
}

void loop() {
    // 1. Check for rotation using our handy delta function
    int move = encoder.getDelta();
    
    if (move != 0) {
        if (move > 0) {
            Serial.print("-> Turned Right (+). ");
        } else {
            Serial.print("<- Turned Left (-). ");
        }
        
        // Fetch and print the absolute position just to verify it's tracking correctly
        Serial.print("Absolute Position: ");
        Serial.println(encoder.getPosition());
    }

    // 2. Check for button clicks
    if (encoder.isClicked()) {
        Serial.println("🔘 BUTTON CLICKED! (Menu Selection Triggered)");
    }

    // A tiny delay yields time to FreeRTOS background tasks 
    // and prevents the idle loop from triggering the Watchdog Timer.
    delay(10); 
}