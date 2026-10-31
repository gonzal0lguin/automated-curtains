// TEST: rotary encoder (knob) + push button
//   ./scripts/test.sh knob
//
// Turn the knob one detent at a time and check:
//   - "raw" changes by the same amount per detent (2 or 4) -> set
//     KNOB_TRANSITIONS_PER_DETENT in include/config.h to that number
//   - each detent produces exactly one +1 / -1 step, clockwise = +
// Press: "CLICK", hold 0.7 s: "LONG PRESS".
#include <Arduino.h>

#include "config.h"
#include "rotary_encoder.h"

RotaryEncoder knob(PIN_KNOB_A, PIN_KNOB_B, PIN_KNOB_SW, KNOB_TRANSITIONS_PER_DETENT);

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== KNOB TEST ===");
    Serial.printf("pins A=%d B=%d SW=%d, %d transitions/detent\n", PIN_KNOB_A, PIN_KNOB_B, PIN_KNOB_SW,
                  KNOB_TRANSITIONS_PER_DETENT);
    Serial.println("Turn slowly one detent at a time; press and hold the button.");
    knob.begin();
}

void loop() {
    static long lastRaw = 0;
    static uint32_t lastPrint = 0;
    knob.tick();

    int d = knob.getDelta();
    long raw = knob.getRawCount();
    if (d != 0) {
        Serial.printf("%s %+d   position=%d   raw=%ld (raw change %+ld)\n", d > 0 ? "CW  >>" : "CCW <<", d,
                      knob.getPosition(), raw, raw - lastRaw);
        lastRaw = raw;
    } else if (raw != lastRaw && millis() - lastPrint > 400) {
        // raw moved but no full detent yet (half-way between detents)
        lastPrint = millis();
        Serial.printf("   raw=%ld (between detents)\n", raw);
    }

    switch (knob.getEvent()) {
        case RotaryEncoder::Event::Click: Serial.println("[ CLICK ]"); break;
        case RotaryEncoder::Event::LongPress: Serial.println("[ LONG PRESS ]"); break;
        default: break;
    }
    delay(2);
}
