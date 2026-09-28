#include <Arduino.h>
#include "magnetic_encoder.h"
#include "rotary_encoder.h" // Your previously written library


// --- Pin Definitions ---
// AS5600 I2C
#define I2C_SDA 21
#define I2C_SCL 22

// Rotary Encoder
#define ROTARY_CLK 32
#define ROTARY_DT 33
#define ROTARY_SW 13

MagneticEncoder magEncoder;
RotaryEncoder rotEncoder(ROTARY_CLK, ROTARY_DT, ROTARY_SW);

// --- Mini UI State Machine ---
int testMenuState = 0; 
const int MAX_STATES = 3;
// State 0 = View Positions
// State 1 = Set Open Limit (100%)
// State 2 = Set Closed Limit (0%)
// State 3 = View Percentage

// Timers for non-blocking operations
unsigned long lastPrintTime = 0;
unsigned long lastDebounceTime = 0;
bool lastButtonState = HIGH;

void setup() {
    Serial.begin(115200);
    delay(1000); // Give serial monitor time to connect

    Serial.println("--- BLINDS SENSOR TESTBED ---");

    // 1. Initialize Magnetic Encoder
    if (!magEncoder.begin(I2C_SDA, I2C_SCL)) {
        Serial.println("ERROR: AS5600 not found! Check I2C wiring.");
        while (1); // Halt if sensor is missing
    }
    Serial.println("AS5600 Initialized.");

    // 2. Initialize Rotary Encoder
    // Assuming your begin() attaches the ISRs automatically
    rotEncoder.begin();
    
    Serial.println("Setup Complete. Turn knob to change modes.");
    Serial.println("-----------------------------------------");
}

void loop() {
    // 1. CRITICAL: Constantly update the absolute encoder to catch multi-turns
    magEncoder.update();

    // 2. Read Rotary Encoder to navigate our test menu
    int delta = rotEncoder.getDelta(); 
    if (delta != 0) {
        testMenuState += delta;
        
        // Wrap around the menu
        if (testMenuState > MAX_STATES) testMenuState = 0;
        if (testMenuState < 0) testMenuState = MAX_STATES;

        Serial.print("\n>>> Switched to Mode: ");
        Serial.println(testMenuState);
    }

    if (rotEncoder.isClicked()) { 
        Serial.println("\n[ CLICK! ]");
        
        if (testMenuState == 1) {
            magEncoder.setOpenPosition();
            Serial.println(">>> SUCCESS: Open Position (100%) Registered! <<<");
        } 
        else if (testMenuState == 2) {
            magEncoder.setClosedPosition();
            Serial.println(">>> SUCCESS: Closed Position (0%) Registered! <<<");
        } 
        else {
            Serial.println("Nothing to save in this mode.");
        }
    }

    // 4. Non-blocking Serial Print (Update UI every 300ms)
    if (millis() - lastPrintTime > 300) {
        lastPrintTime = millis();

        Serial.print("Raw Angle: ");
        Serial.print(magEncoder.getRawAngle());
        Serial.print(" \t| Abs Total: ");
        Serial.print(magEncoder.getAbsolutePosition());

        // Append UI context based on knob position
        if (testMenuState == 0) {
            Serial.print(" \t<-- [MODE 0: View Raw Data]");
        }
        else if (testMenuState == 1) {
            Serial.print(" \t<-- [MODE 1: Press to SET OPEN]");
        }
        else if (testMenuState == 2) {
            Serial.print(" \t<-- [MODE 2: Press to SET CLOSED]");
        }
        else if (testMenuState == 3) {
            Serial.print(" \t| UI %: ");
            Serial.print(magEncoder.getPercentage());
            Serial.print("% \t<-- [MODE 3: View Calibration %]");
        }
        Serial.println();
    }
}