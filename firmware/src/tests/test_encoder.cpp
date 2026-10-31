// TEST: AS5600 magnetic encoder
//   ./scripts/test.sh encoder
//
// 1. Scans the I2C bus (AS5600 answers at 0x36).
// 2. Reports magnet status: it must say "magnet OK" (not too weak/strong).
//    AGC should sit roughly mid-range (~60-200 on 5 V, lower on 3.3 V).
// 3. Streams raw angle + multi-turn position. Rotate the shaft by hand:
//    one full turn must change the position by ~4096 and the count must keep
//    going up past 4095 (multi-turn tracking).
// Serial keys:  s = start/stop streaming   d = diagnostics   z = zero position
//               r = reverse direction       i = I2C scan
#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "magnetic_encoder.h"

MagneticEncoder enc;
bool streaming = true;
uint32_t reads = 0, lastRate = 0;

static void scan() {
    Serial.println("I2C scan:");
    int found = 0;
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  device at 0x%02X%s\n", a, a == 0x36 ? "  <- AS5600" : "");
            found++;
        }
    }
    if (!found) Serial.println("  nothing found: check SDA/SCL, 3.3V/GND and pull-ups");
}

static void diag() {
    MagneticEncoder::Diag d = enc.diagnostics();
    if (!d.connected) {
        Serial.println("AS5600 NOT CONNECTED");
        return;
    }
    const char* m = !d.magnet ? "NO MAGNET" : d.tooWeak ? "magnet TOO WEAK (move closer)"
                                         : d.tooStrong ? "magnet TOO STRONG (move away)" : "magnet OK";
    Serial.printf("%s | AGC %u | magnitude %u | I2C errors %lu\n", m, d.agc, d.magnitude,
                  (unsigned long)enc.errorCount());
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== AS5600 ENCODER TEST ===");
    Serial.printf("SDA=%d SCL=%d @ %d Hz\n", PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
    bool ok = enc.begin();
    scan();
    if (!ok) Serial.println("AS5600 not responding! (tie the AS5600 DIR pin to GND, check wiring)");
    diag();
    Serial.println("keys: s=stream d=diag z=zero r=reverse i=scan");
}

void loop() {
    // poll as fast as the real firmware does (every 2 ms)
    if (enc.update()) reads++;

    if (Serial.available()) {
        switch (Serial.read()) {
            case 's': streaming = !streaming; break;
            case 'd': diag(); break;
            case 'z': enc.setPosition(0); Serial.println("position zeroed"); break;
            case 'r':
                enc.setReversed(!enc.reversed());
                Serial.printf("direction %s\n", enc.reversed() ? "REVERSED" : "normal");
                break;
            case 'i': scan(); break;
        }
    }

    static uint32_t lastPrint = 0;
    if (streaming && millis() - lastPrint >= 200) {
        lastPrint = millis();
        int32_t p = enc.position();
        Serial.printf("raw %4u (%6.1f deg) | position %8ld | turns %+8.3f | %s\n", enc.rawAngle(),
                      enc.rawAngle() * 360.0f / ENCODER_CPR, (long)p, p / (float)ENCODER_CPR,
                      enc.connected() ? "ok" : "I2C ERROR");
    }
    if (millis() - lastRate >= 5000) {
        Serial.printf("-- %lu reads/s, %lu errors total\n", (unsigned long)(reads / 5), (unsigned long)enc.errorCount());
        reads = 0;
        lastRate = millis();
    }
    delay(2);
}
