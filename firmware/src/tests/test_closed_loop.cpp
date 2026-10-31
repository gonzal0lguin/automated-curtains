// TEST: closed-loop blind control (motor + AS5600), no display / WiFi.
//   ./scripts/test.sh closed_loop
//
// Runs the real motion controller with the serial console. Calibrations made
// here are saved and used by the main firmware.
//
// Suggested sequence:
//   status              check "encoder=ok"
//   jog 4096            one encoder turn; the encoder must follow the motor
//   ratio               measure steps per tick (do it with the blind half open)
//   jog -20000 ...      move to fully closed, then:  setclosed
//   jog 20000 ...       move to fully open, then:    setopen
//   goto 50 / open / close / stop
//   Stall test: hold the shaft/blind while it moves -> "motor stalled" error.
#include <Arduino.h>

#include "app_log.h"
#include "blinds.h"
#include "console.h"
#include "settings.h"

void setup() {
    applog::begin(115200);
    delay(500);
    Serial.println("\n=== CLOSED LOOP TEST === (type help)");
    settings::begin();
    blinds.begin(false);
}

void loop() {
    console::poll();
    static uint32_t last = 0;
    BlindStatus st = blinds.status();
    if (st.state == BlindState::Moving && millis() - last > 250) {
        last = millis();
        Serial.printf("  %5.1f%%  ticks %7ld -> %7ld  steps %8ld\n", st.percent, (long)st.ticks, (long)st.targetTicks,
                      (long)st.motorSteps);
    }
    delay(5);
}
