#!/usr/bin/env bash
# Build, flash and open the serial monitor for one module test firmware.
#
#   ./scripts/test.sh <test> [serial-port]
#   ./scripts/test.sh build-all          # just compile every test (no board needed)
#
# Tests: knob encoder motor closed_loop display ui web net sim
set -euo pipefail
cd "$(dirname "$0")/.."

PIO="${PIO:-pio}"
command -v "$PIO" >/dev/null 2>&1 || PIO="$HOME/.platformio/penv/bin/pio"

TESTS="knob encoder motor closed_loop display ui web net sim"

usage() {
    echo "usage: $0 <test> [port]   |   $0 build-all"
    echo
    echo "  knob         rotary encoder + button (detents, click, long press)"
    echo "  encoder      AS5600: I2C scan, magnet status, multi-turn position"
    echo "  motor        stepper + TMC2209 open loop (serial commands)"
    echo "  closed_loop  motor + AS5600 closed loop, calibration, stall test"
    echo "  display      GC9A01 colors, fonts, icons, FPS, backlight"
    echo "  ui           full knob UI/menus with simulated motor (display + knob only)"
    echo "  web          WiFi + web app/API with simulated motor (ESP32 only)"
    echo "  net          WiFi, NTP, weather and bus fetch"
    echo "  sim          the complete main firmware with simulated motor + encoder"
    exit 1
}

[ $# -ge 1 ] || usage

if [ "$1" = "build-all" ]; then
    for t in $TESTS; do
        echo "=== building $t"
        TEST=$t "$PIO" run -e test -s || { echo "FAILED: $t"; exit 1; }
    done
    "$PIO" run -e blinds -s
    echo "all builds OK"
    exit 0
fi

TEST_NAME="$1"
case " $TESTS " in *" $TEST_NAME "*) ;; *) usage ;; esac

PORT_ARGS=()
if [ $# -ge 2 ]; then PORT_ARGS=(--upload-port "$2" --monitor-port "$2"); fi

TEST="$TEST_NAME" "$PIO" run -e test -t upload -t monitor "${PORT_ARGS[@]}"
