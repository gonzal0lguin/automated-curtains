#pragma once
// =============================================================================
//  Hardware configuration — single source of truth for pins and constants.
//  Runtime-tunable values (speeds, brightness, calibration...) live in
//  Settings (lib/settings) and are editable from the knob menu / web UI.
// =============================================================================

// ---------------------------------------------------------------- AS5600 (I2C)
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define I2C_FREQ_HZ 400000

// ------------------------------------------------- Rotary encoder (EC11 knob)
#define PIN_KNOB_A  32
#define PIN_KNOB_B  33
#define PIN_KNOB_SW 13
// Quadrature transitions per mechanical detent. Most EC11 knobs are 4, some
// are 2. Run the `test-knob` firmware: it prints raw transitions per detent.
#define KNOB_TRANSITIONS_PER_DETENT 2

// ---------------------------------------------- TMC2209 stepper driver (STEP/DIR)
#define PIN_MOTOR_STEP 27
#define PIN_MOTOR_DIR  14
// WARNING: GPIO12 is a boot strapping pin (flash voltage). If your driver board
// pulls EN high, the ESP32 will not boot. If that happens move EN to GPIO26.
#define PIN_MOTOR_EN   12

// Optional TMC2209 UART (PDN_UART pin). Wire ESP TX -> 1k resistor -> PDN_UART
// and ESP RX -> PDN_UART. If not wired, the firmware falls back to standalone
// mode automatically (microsteps/current set by the driver's pins/pot).
#define PIN_TMC_RX 16
#define PIN_TMC_TX 17
#define TMC_ADDRESS 0        // MS1/MS2 low = address 0
#define TMC_R_SENSE 0.11f    // most TMC2209 modules use 0.11 ohm

// ------------------------------------------------- GC9A01 240x240 round display
#define PIN_TFT_SCLK 18
#define PIN_TFT_MOSI 23
#define PIN_TFT_CS   5
#define PIN_TFT_DC   19
#define PIN_TFT_RST  4
#define PIN_TFT_BL   25
#define TFT_SPI_HZ   40000000

// ----------------------------------------------------------------- Constants
#define DEVICE_NAME       "blinds"          // default hostname -> http://blinds.local
#define WIFI_SETUP_AP     "Blinds-Setup"    // captive portal SSID when no WiFi saved
#define ENCODER_CPR       4096              // AS5600 counts per revolution

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

#ifndef SIMULATE_MECHANICS
#define SIMULATE_MECHANICS 0
#endif
