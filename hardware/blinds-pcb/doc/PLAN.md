# Blinds control panel PCB — working plan and log

Living document. Read this first when starting a new session; update the
**Status** and **Log** sections after every work block.

- Project: `hardware/blinds-pcb` (KiCad 10.0.8), part of `automated-curtains`.
- Inputs: `doc/components.md`, `doc/Curtain control panel hardware design.pdf`
  (earlier design conversation), `firmware/include/config.h`, `firmware/README.md`.
- Goal now: finish the **schematic**. PCB layout comes after.

## Status

| # | Step | State |
|---|---|---|
| 0 | Plan agreed with user (2026-11-12) | done |
| 1 | Git + project cleanup (baseline commit, `.gitignore`, untrack `.history`, library restructure) | **next** — not started |
| 2 | Verify datasheets + LCSC part numbers, freeze BOM | partly done (see Verified facts) |
| 3 | Symbols/footprints: TMC2209 BTT module, RJ45, DC jack, NodeMCU fixes | not started |
| 4 | Wire schematic (one A3 sheet, boxed blocks), ERC to zero | not started |
| 5 | Docs: `hardware-design.md`, `bom.md`, `pinmap.md`, hardware `README.md`, schematic PDF; update `firmware/include/config.h` + firmware README wiring table | not started |
| 6 | PCB layout (separate phase, see below) | not started |

**Blocker for steps 1b/3/4:** KiCad was open on the project (lock files
`~blinds-pcb.*.lck`, process `/usr/bin/kicad`). It must be closed before the
schematic, `sym-lib-table`, `fp-lib-table` or library files are edited from
outside, otherwise KiCad overwrites the changes. Check with
`pgrep -a kicad` before editing.

**Nothing in the project has been modified yet** except this file.

## Decisions (agreed with the user)

- **Driver stays on the panel board** (TMC2209 module in a socket). The PDF
  recommended moving it to a motor board; the user chose not to, for now.
  Consequence: the RJ45 carries motor phases + encoder I2C (see Risks).
- **TMC2209 module: BigTreeTech** (V1.3 assumed — confirm version on silkscreen).
- **Hand soldering everything.** Use KiCad `_HandSolder` footprints and
  **0805** passives (not 0603). Avoid QFN/BGA/0402. LCSC numbers are still
  recorded for ordering parts, but JLC basic/extended fees don't matter.
- **Full pin remap** — no strapping pins used (table below).
- **10 WS2812B LEDs** (not 12).
- **Extras included:** side-mounted BOOT + RESET buttons, spare I2C header,
  ambient light sensor, 12 V sense divider.
- **Library restructure approved** (rename/merge local libraries).
- LCD module header order as printed, left to right:
  `BLK, CS, DC, RES, SDA, SCL, VIN, GND`. Backlight is driven directly by PWM
  on BLK (works today on GPIO25), so no external MOSFET.
- One schematic sheet (A3) with boxed, titled blocks — simpler than
  hierarchical sheets for a first board.
- Layers: start with **2-layer**, solid GND pour on the bottom. 4-layer is the
  fallback if routing around the LED ring gets tight.

## Pin map (final)

GPIO 0, 2, 5, 12, 15 are left unconnected so USB flashing works with the
NodeMCU in its socket. GPIO 1/3 = USB serial, 6–11 = flash: never used.

| Signal | GPIO | Was | Notes |
|---|---|---|---|
| I2C SDA / SCL | 21 / 22 | same | to RJ45 (AS5600) + spare I2C header; pull-ups on panel; run at 100 kHz |
| Knob A / B | 32 / 33 | same | 10k pull-up, 10k series + 10 nF filter |
| Knob switch | **34** | 13 | input-only pin: external 10k pull-up + 100 nF (no internal pull-up exists) |
| Motor STEP / DIR | 27 / 14 | same | GPIO14 toggles at boot; harmless because EN is pulled high |
| Motor EN | **26** | 12 | 10k pull-up to 3V3 → driver off during boot/flash |
| TMC UART TX | 17 | same | through 1k to PDN_UART |
| TMC UART RX | **35** | 16 | input-only pin, direct to PDN_UART |
| TFT SCLK / MOSI | 18 / 23 | same | |
| TFT CS | **13** | 5 | |
| TFT DC / RST / BL | 19 / 4 / 25 | same | |
| WS2812 data | **16** | new | 10k pull-down, then 74AHCT1G125 |
| 12 V sense | **36** (VP) | new | 100k / 22k divider + 100 nF, ADC1 |
| Ambient light | **39** (VN) | new | phototransistor + resistor, ADC1 |
| TMC DIAG | — | — | **not routed to a GPIO** (no pin left). Brought to a test pad. Stall detection already comes from the AS5600 closed loop, and StallGuard is readable over UART. |

Firmware edits needed in `firmware/include/config.h`:
`PIN_MOTOR_EN 26`, `PIN_TFT_CS 13`, `PIN_KNOB_SW 34`, `PIN_TMC_RX 35`,
`I2C_FREQ_HZ 100000`, plus new `PIN_LED_DATA 16`, `PIN_VIN_SENSE 36`,
`PIN_AMBIENT 39`. Knob code must not rely on internal pull-ups for GPIO34.

## Circuit blocks to draw

| Block | Circuit |
|---|---|
| Input | DC barrel jack 5.5×2.1 (centre +) → PTC resettable fuse 2 A hold (≥30 V rated) → SS54 series Schottky (reverse polarity) → SMBJ15A TVS to GND → net `+12V` |
| 5 V buck | AP63205WU (U2, TSOT-26). Per datasheet Table 3: **L = 4.7 µH**, CIN 10 µF (use 2×10 µF 50 V 1206 + 100 nF, for DC-bias derating), COUT 2×22 µF, CBST 100 nF between BST and SW. FB (pin 1) direct to VOUT. EN (pin 2) tied to VIN. Inductor: shielded, Isat ≥ 3 A. |
| NodeMCU feed | Series Schottky from `+5V` to the NodeMCU 5V pin (so USB and 12 V can be connected together without back-feeding the buck/LEDs); 10 µF + 100 nF at the pin; 100 nF at 3V3 pin. Two 1×19 female sockets. |
| TMC2209 (BTT) | VM: 100 µF/35 V electrolytic + 100 nF right at the socket. VIO = 3V3 + 100 nF. MS1 = MS2 = GND (UART address 0, matches `TMC_ADDRESS 0`). CLK = GND. EN ← GPIO26 with 10k pull-up. PDN_UART ← GPIO35 direct and GPIO17 via 1k. DIAG → test pad. Motor A1/A2/B1/B2 → RJ45. Two 1×8 female sockets. |
| WS2812 ring | 10 × WS2812B 5050, 100 nF each, 220–470 µF bulk on 5 V at ring entry. Data: GPIO16 (10k pull-down) → 74AHCT1G125 (VCC 5 V, /OE = GND, 100 nF) → ~100 Ω series → LED1 DIN. Last LED DOUT → test pad. |
| Knob | ALPS EC35A1520402 (custom symbol/footprint exist). A, B: 10k pull-up to 3V3, 10k series, 10 nF to GND. C → GND. MP mounting pins → GND. |
| Centre button | SMD tact 5.2×5.2 (TS-1187A type) on the knob axis → GPIO34, 10k pull-up, 100 nF. |
| RJ45 (motor + encoder) | Plain 8P8C **without magnetics**, ≥1.5 A/contact. Pinout (T568B pairs): 1–2 coil A, 3–6 coil B, 4 SDA, 5 GND, 7 SCL, 8 3V3. I2C pull-ups 4.7k on the panel (fit only if the AS5600 module has none — add as fitted-by-default with a note), 100 nF on outgoing 3V3. Silkscreen: "NOT ETHERNET — do not unplug while powered". |
| LCD connector | JST-XH 8-pin (J1). Order must match the module: BLK, CS, DC, RES, SDA, SCL, VIN, GND. 3-pad solder jumper to choose VIN = 5 V or 3V3 (default 3V3 as today — confirm what the user currently feeds it). 10 µF + 100 nF. |
| 12 V sense | 100k (top) / 22k (bottom) from `+12V` → GPIO36, 100 nF across the bottom resistor. 12 V → 2.16 V, 15 V → 2.70 V. |
| Ambient light | Phototransistor (TEMT6000-type, hand-solderable) collector to 3V3, emitter → GPIO39 with 10k to GND. |
| Buttons | Right-angle tact switches: RESET (NodeMCU EN → GND), BOOT (GPIO0 → GND). |
| I2C header | 4-pin JST-XH or pin header: 3V3, GND, SDA, SCL. |
| Indicators / test | Power LEDs on 12 V (4.7k) and 5 V (1k). Test points: 12V, 5V, 3V3, GND, DIAG, LED DOUT. |

## Verified facts (with source)

- **AP63205** (Diodes datasheet DS41326, checked 2026-11-12):
  TSOT-26 pins 1 FB, 2 EN, 3 VIN, 4 GND, 5 SW, 6 BST. VIN 3.8–32 V
  (abs max 35 V DC). 1.1 MHz, PFM at light load. EN may be left open or tied
  to VIN (abs max 35 V). Table 3 for 5 V: L 4.7 µH, C1 10 µF, C2 2×22 µF,
  C3 100 nF. Layout: VIN caps close to the IC, GND plane under the device,
  several GND vias at input/output caps.
- **BTT TMC2209** (global.bttwiki.com/TMC2209.html): header J1 =
  EN, MS1, MS2, PDN(UART), PDN(UART alt), CLK, STEP, DIR; header J2 =
  VM, GND, A2, A1, B1, B2, VCC_IO, GND. UART factory-connected on J1 pin 4.
  Rsense 0.11 Ω, VM 4.75–28 V, VREF factory 1.2 V (~0.9 A RMS). DIAG and
  INDEX are extra pins — **physical position not confirmed**; check the real
  module before drawing the footprint. StepStick pitch 2.54 mm, rows 12.7 mm
  apart (confirm).
- **TMC2209 logic** (Watterott SilentStepStick page): EN low = on; MS1/MS2
  have pull-downs and set the UART address; SPREAD low = StealthChop; VIO 3.3–5 V.
- **ESP32 strapping pins:** 0, 2, 5, 12, 15. GPIO34–39 are input-only with no
  internal pull-ups. GPIO36/39 can glitch when WiFi/ADC is active, so they
  are used only for slow analog signals.

## Still to verify (step 2)

- LCSC numbers and stock for every part (none confirmed yet).
- Inductor part: 4.7 µH shielded, Isat ≥ 3 A, hand-solderable (5×5 or 6×6 mm).
- PTC fuse voltage rating; SMBJ15A clamp voltage vs 28 V TMC limit.
- RJ45 jack part: no magnetics, current rating, footprint.
- DC jack part and footprint.
- BTT module DIAG/INDEX pin positions and row spacing (ask user for the
  silkscreen or a photo).
- Whether the user's AS5600 module has I2C pull-ups and how it is powered
  (3V3; VDD5V and VDD3V3 tied together).
- Whether the NodeMCU has a diode between USB 5 V and the 5V pin.
- GC9A01 module supply: fed with 5 V or 3.3 V today?
- EC35A footprint against the ALPS drawing (`EC35A1520402_ALPS_ELECTRIC.pdf`).

## Project problems found (to fix in step 1 / 3 / 4)

- `.history` (KiCad local history) is committed as a nested git repo
  (gitlink, mode 160000, no `.gitmodules`). Untrack with
  `git rm --cached hardware/blinds-pcb/.history` and ignore it.
- No KiCad `.gitignore`. Ignore: `.history/`, `~*.lck`, `_restore_backup_*/`,
  `*-backups/`, `fp-info-cache`, `output/`, `*.kicad_prl`.
  (`output/` is created by the kicad-pro MCP server.)
- Libraries: `local_libraries/custom_symbols.kicad_sym/` is a directory named
  like a file, one library per symbol, a stray `ESP32_NODEMCU.step` duplicate,
  and a datasheet PDF inside `3d_models/`. Target layout:
  `lib/blinds.kicad_sym`, `lib/blinds.pretty/`, `lib/3d/`, `doc/datasheets/`,
  then relink `lib_id`s in the schematic and update both lib tables and the
  3D model paths inside the footprints.
- NodeMCU symbol: default footprint field points to a non-existent library
  nickname (`ESP32_NODEMCU:`), GPIO36/39 are named `SP`/`SN`, input-only pins
  are typed bidirectional.
- Schematic: `A1` is a DRV8825 symbol (wrong for TMC2209); `SW1` is a generic
  encoder instead of the EC35 symbol; no RJ45, no DC jack; only 1 WS2812;
  several parts have no footprint; generic values; no LCSC fields; no
  no-connect flags, `PWR_FLAG`s or title block. ERC: 104 findings (mostly
  unconnected pins — the sheet is almost unwired).
- PCB file already has 7 footprints from an early sync, 2 layers, default
  rules, no outline. It will be re-synced from the finished schematic.
- `doc/README.md` is a copy of the firmware README with broken links; replace
  with a hardware README. `doc/components.md` is superseded by `doc/bom.md`.
- Uncommitted user work exists on `main` — make a baseline commit on a new
  branch (e.g. `hw/panel-schematic`) before changing anything.

## Risks and design considerations

- **Motor phases through the RJ45** (1–1.4 A per the PDF, 24AWG is good for
  roughly 0.6 A continuous by PoE guidance, jack contacts ~1.5 A): cap run
  current to about 0.8–1 A RMS over UART, pure-copper 24AWG cable (not CCA),
  I2C at 100 kHz, never unplug powered (inductive kick can kill the driver).
- **5 V budget:** ESP32 up to ~0.5 A peaks, 10 LEDs at full white ~0.6 A, LCD
  ~40 mA → under 2 A, but cap LED brightness in firmware.
- **3V3 budget** (NodeMCU's AMS1117): ESP32 + TMC VIO + AS5600 + pull-ups
  (+ LCD if fed from 3V3). Fine, but no heavy 3V3 loads.
- **Assembly side:** LEDs and tact switch face the front; connectors, sockets
  and buck go on the back. Hand soldering makes two-sided assembly a non-issue.
- **12 V sense usefulness:** lets the firmware see power dropping and save the
  position before the rails collapse. Cheap (2 resistors + cap), so included.

## PCB phase (after the schematic)

1. Mechanics first: board outline, EC35 bore features from the PDF (tact
   switch at centre, 3 guide-pin holes, wire slot), LED ring radius for 10
   LEDs (36° spacing), mounting holes, connector positions.
2. Board setup for JLC 2-layer, 1.6 mm, 1 oz: min track/clearance 0.2 mm,
   vias 0.6/0.3 mm, copper-to-edge 0.5 mm.
3. Net classes: `+12V` and motor phases 1.0 mm; `+5V` 0.6–0.8 mm; signals 0.25 mm.
4. Placement: buck copied from the datasheet layout with short loops; NodeMCU
   antenna over the board edge or a copper keep-out; motor traces away from
   I2C and knob lines; decoupling caps next to their pins.
5. Hand-solder footprints throughout; thermal reliefs on GND pads.
6. DRC, JLC DFM check, 3D check against printed parts, then gerbers + drill
   files (BOM/CPL not needed for hand assembly).

## Log

- **2026-11-12** — Read PDF, docs, `config.h`, schematic (via kicad-pro MCP:
  19 symbols, 104 ERC findings). Wrote the plan; user approved with answers
  recorded under Decisions. Verified AP63205 datasheet values and the BTT
  TMC2209 header pinout. Ambient sensor took GPIO39, so DIAG goes to a test
  pad only. Created this file. No other project file touched yet.
