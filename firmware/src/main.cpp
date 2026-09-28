#include <Arduino.h>
#include "magnetic_encoder.h"
#include "rotary_encoder.h" 
#include "motor.h" 

// --- Pin Definitions ---
// AS5600 I2C
#define I2C_SDA 21
#define I2C_SCL 22

// Rotary Encoder
#define ROTARY_CLK 32
#define ROTARY_DT 33
#define ROTARY_SW 13 

// TMC2208 Motor Driver
#define MOTOR_STEP 27
#define MOTOR_DIR 14
#define MOTOR_EN 12

// --- Object Instantiation ---
MagneticEncoder magEncoder;
RotaryEncoder rotEncoder(ROTARY_CLK, ROTARY_DT, ROTARY_SW);
Motor myMotor(MOTOR_STEP, MOTOR_DIR, MOTOR_EN, &magEncoder);

// --- FreeRTOS Task Handle ---
TaskHandle_t Core1TaskHandle;

// --- Mini UI State Machine ---
int testMenuState = 0; 
const int MAX_STATES = 4; // Now 5 states (0 to 4)
// State 0 = View Positions
// State 1 = Set Open Limit (100%)
// State 2 = Set Closed Limit (0%)
// State 3 = Toggle Full Open / Full Close
// State 4 = Dial Custom Position

bool isDialing = false;
int targetPercent = 50; 
bool isOpening = false; // Tracks toggle state for Mode 3

// Timers for non-blocking operations
unsigned long lastPrintTime = 0;

// ==============================================================================
// CORE 1 TASK: The Real-Time Hardware Loop
// ==============================================================================
void Core1Task(void *pvParameters) {
    for (;;) {
        // 1. Constantly update absolute encoder to catch multi-turns [cite: 15, 140]
        magEncoder.update();
        
        // 2. Step the motor non-stop using our AccelStepper wrapper [cite: 139]
        myMotor.update();

        // Yield for 1 tick to keep the FreeRTOS Watchdog happy [cite: 179, 180]
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

// ==============================================================================
// CORE 0 TASK: Setup & UI Loop
// ==============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000); 

    Serial.println("--- BLINDS MASTER TESTBED ---");

    // 1. Initialize Magnetic Encoder
    if (!magEncoder.begin(I2C_SDA, I2C_SCL)) {
        Serial.println("ERROR: AS5600 not found! Check I2C wiring.");
        while (1); 
    }
    
    // 2. Initialize Rotary Encoder
    rotEncoder.begin();

    // 3. Initialize Motor (Placeholder ratio: 1.0 steps per tick for testing)
    myMotor.begin(200.0 / 4096.0); // Assuming 200 steps/rev and 4096 ticks/rev

    // 4. Pin the Hardware loop to Core 1 [cite: 129]
    // xTaskCreatePinnedToCore(
    //     Core1Task,         
    //     "Core1Task",       
    //     10000,             
    //     NULL,              
    //     1,                 
    //     &Core1TaskHandle,  
    //     1                  
    // );
    
    Serial.println("System Ready. FreeRTOS running.");
    Serial.println("-----------------------------------------");
}

void loop() {
    // --- ROTARY ENCODER LOGIC ---
    int delta = rotEncoder.getDelta(); 
    
    if (delta != 0) {
        if (!isDialing) {
            // Normal behavior: Change menu state
            testMenuState += delta;
            if (testMenuState > MAX_STATES) testMenuState = 0;
            if (testMenuState < 0) testMenuState = MAX_STATES;

            Serial.print("\n>>> Switched to Mode: ");
            Serial.println(testMenuState);
        } else {
            // Dialing behavior: Change target percentage (increments of 5%)
            targetPercent += (delta * 5);
            targetPercent = constrain(targetPercent, 0, 100);
        }
    }

    // --- BUTTON CLICK LOGIC ---
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
        else if (testMenuState == 3) {
            // Toggle Open/Close
            isOpening = !isOpening;
            long targetTicks = isOpening ? magEncoder.getOpenPosition() : magEncoder.getClosedPosition();
            myMotor.moveToEncoderPosition(targetTicks);
            
            Serial.print(">>> COMMAND: Moving to Full ");
            Serial.println(isOpening ? "OPEN" : "CLOSED");
        }
        else if (testMenuState == 4) {
            // Enter or Exit Dialing Mode
            isDialing = !isDialing; 
            
            if (!isDialing) {
                // We just exited dialing mode, execute the move!
                // Map the 0-100% to actual encoder ticks using the limits stored in the encoder
                long openLimit = magEncoder.getOpenPosition();
                long closedLimit = magEncoder.getClosedPosition();
                
                // Linear mapping from percentage to ticks
                long targetTicks = map(targetPercent, 0, 100, closedLimit, openLimit);
                
                myMotor.moveToEncoderPosition(targetTicks);
                
                Serial.print(">>> COMMAND: Moving to ");
                Serial.print(targetPercent);
                Serial.println("% <<<");
            } else {
                Serial.println(">>> DIALING MODE: Turn knob to set %, Click to execute. <<<");
            }
        }
    }

    // --- UI SERIAL PRINT (Core 0 only, runs every 300ms) ---
    if (millis() - lastPrintTime > 300) {
        lastPrintTime = millis();

        Serial.print("Ticks: ");
        Serial.print(magEncoder.getAbsolutePosition());
        Serial.print(" \t| Motor Stalled: ");
        Serial.print(myMotor.getStallState() ? "YES" : "NO");

        // Contextual UI Text
        if (testMenuState == 0) {
            Serial.print(" \t<-- [MODE 0: View Sensor Data]");
        }
        else if (testMenuState == 1) {
            Serial.print(" \t<-- [MODE 1: Press to SET OPEN]");
        }
        else if (testMenuState == 2) {
            Serial.print(" \t<-- [MODE 2: Press to SET CLOSED]");
        }
        else if (testMenuState == 3) {
            Serial.print(" \t<-- [MODE 3: Press to TOGGLE OPEN/CLOSE]");
        }
        else if (testMenuState == 4) {
            if (isDialing) {
                Serial.print(" \t<-- [DIALING]: Set Target -> ");
                Serial.print(targetPercent);
                Serial.print("%");
            } else {
                Serial.print(" \t<-- [MODE 4: Press to DIAL CUSTOM POSITION]");
            }
        }
        Serial.println();
    }

    magEncoder.update();
        
    myMotor.update();
}