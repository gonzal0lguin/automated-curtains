# Pin map — control panel board

ESP32 NodeMCU 38-pin (ESP32-WROOM-32). This is the map the PCB is drawn to.

> **The firmware still uses the breadboard pins.** `firmware/include/config.h`
> has not been changed, so the current prototype keeps working. Apply the
> edits at the bottom when the PCB is assembled.

## Signals

| Signal | GPIO | Breadboard pin | Direction | External parts |
|---|---|---|---|---|
| I2C SDA | 21 | 21 | bidir | 10k pull-up to 3V3; RJ45 pin 4, I2C header |
| I2C SCL | 22 | 22 | out | 10k pull-up to 3V3; RJ45 pin 7, I2C header |
| Knob A | 32 | 32 | in | 10k pull-up, 10k series, 10 nF to GND |
| Knob B | 33 | 33 | in | 10k pull-up, 10k series, 10 nF to GND |
| Knob switch (centre press) | **34** | 13 | in | 10k pull-up, 100 nF to GND |
| Motor STEP | 27 | 27 | out | — |
| Motor DIR | 14 | 14 | out | — |
| Motor EN | **26** | 12 | out | 10k pull-up to 3V3 (driver off until firmware enables it) |
| TMC2209 UART TX | 17 | 17 | out | 1k series to PDN_UART |
| TMC2209 UART RX | **35** | 16 | in | direct to PDN_UART |
| TFT SCLK | 18 | 18 | out | — |
| TFT MOSI | 23 | 23 | out | — |
| TFT CS | **13** | 5 | out | — |
| TFT DC | 19 | 19 | out | — |
| TFT RST | 4 | 4 | out | — |
| TFT backlight (PWM) | 25 | 25 | out | — |
| WS2812 data | **16** | — | out | 10k pull-down, 74AHCT1G125, 100 Ω |
| 12 V sense | **36** (VP) | — | analog in | 100k / 22k divider, 100 nF |
| Ambient light | **39** (VN) | — | analog in | TEPT4400 phototransistor, 4.7k to GND |

## Pins deliberately left unconnected

| GPIO | Why |
|---|---|
| 0 | Strapping: selects flash-download mode. Only the BOOT button touches it. |
| 2 | Strapping: must be low/floating to enter download mode. Also the on-board LED. |
| 5 | Strapping: SDIO timing; has PWM activity at boot. |
| 12 | Strapping: selects flash voltage. Pulled high = ESP32 does not boot. |
| 15 | Strapping: boot-log enable; has PWM activity at boot. |
| 1, 3 | USB serial (flashing and console). |
| 6–11 | Connected to the module's internal flash. |

With these free, the NodeMCU can be flashed over USB while plugged into the
board. The RESET (EN) and BOOT (GPIO0) buttons are a fallback if automatic
download mode fails.

## Why some signals moved

- **EN 12 → 26:** GPIO12 high at reset makes the ESP32 select 1.8 V flash and
  fail to boot; a TMC2209 EN pull-up does exactly that.
- **TFT CS 5 → 13:** GPIO5 is a strapping pin. It worked, but moving it keeps
  every strapping pin unloaded.
- **Knob switch 13 → 34 and UART RX 16 → 35:** inputs can live on the
  input-only pins (34–39), which frees output-capable pins for CS and the LEDs.
- **36 / 39 for analog only:** these two can show short glitches when WiFi
  and the ADC are active, so nothing interrupt-driven uses them.
- **TMC2209 DIAG has no GPIO:** no pin left. It is left unconnected. Stalls
  are already detected with the AS5600, and StallGuard can be read over UART.

## Connector pinouts

**LCD (JST-XH 8-pin), same order as the module's silkscreen:**

| Pin | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| Signal | BLK | CS | DC | RES | SDA (MOSI) | SCL (SCLK) | VIN | GND |
| GPIO | 25 | 13 | 19 | 4 | 23 | 18 | 3V3 | — |

**Motor + encoder (RJ45, straight-through T568B cable):**

| Pin | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| Signal | Coil A1 | Coil A2 | Coil B1 | SDA | GND | Coil B2 | SCL | 3V3 |
| Pair | 1–2 | 1–2 | 3–6 | 4–5 | 4–5 | 3–6 | 7–8 | 7–8 |

Each coil uses one twisted pair, so its magnetic field largely cancels. SDA
is paired with GND and SCL with 3V3, which gives each signal a quiet return.
**This port is not Ethernet** — never plug it into a network device.

**Spare I2C (JST-XH 4-pin):** 3V3, GND, SDA, SCL.

## Firmware edits for the PCB

In `firmware/include/config.h`:

```c
#define I2C_FREQ_HZ    100000   // was 400000: 1 m cable next to motor phases
#define PIN_KNOB_SW    34       // was 13 (input-only pin, external pull-up)
#define PIN_MOTOR_EN   26       // was 12
#define PIN_TMC_RX     35       // was 16
#define PIN_TFT_CS     13       // was 5
#define PIN_LED_DATA   16       // new: WS2812 ring
#define PIN_VIN_SENSE  36       // new: 12 V sense (ADC1_CH0)
#define PIN_AMBIENT    39       // new: ambient light (ADC1_CH3)
```

Also check that the knob code does not depend on `INPUT_PULLUP` for the
switch: GPIO34 has no internal pull-up (the board provides one).
