// TEST: stepper motor + TMC2209, open loop (no encoder needed)
//   ./scripts/test.sh motor
//
// Checks wiring of STEP/DIR/EN and (optionally) the TMC2209 UART.
// Start with the motor NOT connected to the blind (or the blind half open).
//
// Commands (type + Enter):
//   m <steps>     move relative, e.g. "m 1600" (1 rev at 1/8 microstep)
//   g <pos>       go to absolute step position
//   v <steps/s>   max speed            a <steps/s2>  acceleration
//   s             stop (decelerate)    x             emergency stop
//   h             toggle holding torque when idle
//   r             reverse direction    p             print position
//   w             wiggle demo: +-1 rev back and forth 3 times
// TMC2209 UART only:
//   c <mA>        run current           u <n>         microsteps (8,16,32,64)
//   t             toggle stealthChop / spreadCycle   i  driver report
#include <Arduino.h>

#include "config.h"
#include "motor.h"

FastStepper motor(PIN_MOTOR_STEP, PIN_MOTOR_DIR, PIN_MOTOR_EN);
uint32_t speed = 1600, accel = 2000;
uint16_t current = 600, microsteps = 16;
bool stealth = true, hold = false, reversed = false;
char line[64];
size_t len = 0;
int wiggle = 0;

static void help() {
    Serial.println("m <steps> | g <pos> | v <speed> | a <accel> | s | x | h | r | p | w | c <mA> | u <ustep> | t | i");
}

static void exec(char* cmd) {
    char c = cmd[0];
    long n = atol(cmd + 1);
    switch (c) {
        case 'm': motor.moveTo(motor.position() + n); Serial.printf("move %+ld\n", n); break;
        case 'g': motor.moveTo(n); Serial.printf("goto %ld\n", n); break;
        case 'v': speed = n; motor.setSpeedAccel(speed, accel); Serial.printf("speed %lu\n", (unsigned long)speed); break;
        case 'a': accel = n; motor.setSpeedAccel(speed, accel); Serial.printf("accel %lu\n", (unsigned long)accel); break;
        case 's': motor.stop(); wiggle = 0; Serial.println("stop"); break;
        case 'x': motor.forceStop(); wiggle = 0; Serial.println("EMERGENCY STOP"); break;
        case 'h': hold = !hold; motor.setHold(hold); Serial.printf("hold when idle: %s\n", hold ? "ON" : "off"); break;
        case 'r': reversed = !reversed; motor.setReversed(reversed); Serial.printf("reversed: %d\n", reversed); break;
        case 'p': Serial.printf("position %ld, running %d\n", (long)motor.position(), motor.isRunning()); break;
        case 'w': wiggle = 6; break;
        case 'c': current = n; motor.configureDriver(current, 30, microsteps, stealth); Serial.println(motor.report()); break;
        case 'u': microsteps = n; motor.configureDriver(current, 30, microsteps, stealth); Serial.println(motor.report()); break;
        case 't': stealth = !stealth; motor.configureDriver(current, 30, microsteps, stealth); Serial.println(motor.report()); break;
        case 'i': Serial.println(motor.report()); break;
        default: help();
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== MOTOR TEST (open loop) ===");
    Serial.printf("STEP=%d DIR=%d EN=%d, UART RX=%d TX=%d\n", PIN_MOTOR_STEP, PIN_MOTOR_DIR, PIN_MOTOR_EN, PIN_TMC_RX,
                  PIN_TMC_TX);
    if (!motor.begin()) Serial.println("stepper init FAILED");
    if (motor.beginUart(Serial2, PIN_TMC_RX, PIN_TMC_TX, TMC_R_SENSE, TMC_ADDRESS)) {
        Serial.println("TMC2209 UART: connected");
        motor.configureDriver(current, 30, microsteps, stealth);
    } else {
        Serial.println("TMC2209 UART: not detected (standalone mode, that's fine)");
    }
    Serial.println(motor.report());
    motor.setSpeedAccel(speed, accel);
    help();
}

void loop() {
    while (Serial.available()) {
        char ch = Serial.read();
        if (ch == '\r') continue;
        if (ch == '\n') {
            line[len] = 0;
            if (len) exec(line);
            len = 0;
        } else if (len < sizeof(line) - 1) {
            line[len++] = ch;
        }
    }
    if (wiggle && !motor.isRunning()) {
        long rev = 200L * (motor.uartConnected() ? microsteps : 8);
        motor.moveTo(motor.position() + ((wiggle % 2) ? -rev : rev));
        wiggle--;
    }
    static bool wasRunning = false;
    if (wasRunning && !motor.isRunning()) Serial.printf("done at %ld\n", (long)motor.position());
    wasRunning = motor.isRunning();
    delay(5);
}
