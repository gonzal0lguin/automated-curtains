# PlatformIO pre-build script for [env:test]: picks which test sketch to build
# from the TEST environment variable, e.g.  TEST=knob pio run -e test -t upload
import os

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

TESTS = {
    "knob": "Rotary encoder + button: detents, raw transitions, click/long-press",
    "encoder": "AS5600: I2C scan, magnet status, raw angle, multi-turn tracking",
    "motor": "Stepper + TMC2209 open loop: serial commands to move/enable/configure",
    "closed_loop": "Motor + AS5600: ratio measurement, closed-loop moves, stall test",
    "display": "GC9A01: colors, fonts, icons, backlight fade, FPS benchmark",
    "ui": "Full knob UI and menus with simulated motor (no WiFi, no motor needed)",
    "web": "WiFi + web server + REST API with simulated motor (no display needed)",
    "net": "WiFi, NTP time, weather and Red bus fetch printed to serial",
    "sim": "Full main firmware with simulated motor + encoder",
}

name = os.environ.get("TEST", "").strip().lower().replace("-", "_")
if name not in TESTS:
    lines = "\n".join(f"  {k:12s} {v}" for k, v in TESTS.items())
    print(f"\n*** Set TEST to one of:\n{lines}\n\nExample: TEST=knob pio run -e test -t upload -t monitor\n")
    env.Exit(1)  # noqa: F821

if name == "sim":
    env.Replace(SRC_FILTER=["-<*>", "+<app/>"])  # noqa: F821
    env.Append(CPPDEFINES=[("SIMULATE_MECHANICS", 1)])  # noqa: F821
else:
    env.Replace(SRC_FILTER=["-<*>", f"+<tests/test_{name}.cpp>"])  # noqa: F821
print(f"Building test firmware: {name} - {TESTS[name]}")
