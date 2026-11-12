# PCB initial component list

## main components
- ESP32 NODE MCU 38 pin
- ALPS EC35A1520402
- TMC2209 (module)
- GC9A01 TFT 1.28" with spi and bkl control, 5/3.3v (with M3 brass mounting points) (off board, traces and connector to esp32 only)
- WS2812b leds 
- 4 pin push btn (footprint undecided)
- AS5600 encoder (off board, traces and connector to esp32 only)
- Nema 17 encoder (off board, traces and connector to tmc2209 only)
- RJ45 female for connectin to motor and encoder
- Standard 12v dc jack for power


# Undefined components
- 12v -> 5v regulator and sorounding circuitery
- Level shifter for controlling ws2812b with 3.3v esp32 pins
- btns for boot, reset, etc (side mounted?).
- power led in board (12v input)
- passive components such as pullups, decoupling, etc.
