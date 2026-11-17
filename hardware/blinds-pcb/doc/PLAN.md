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
| 1a | Branch `hw/panel-schematic`, `.gitignore`, untrack `.history` + `.kicad_prl`, baseline commit `09e70f2` | done |
| 1b | Library restructure (`lib/blinds.kicad_sym`, `lib/blinds.pretty`, `lib/3d`, `doc/datasheets`) | done |
| 2 | Verify datasheets + LCSC part numbers, freeze BOM | mostly done: `doc/bom.md`; parts marked "?" still unconfirmed |
| 3 | Symbols/footprints: TMC2209 BTT module, RJ45, DC jack, NodeMCU fixes | done (two placeholder footprints, see Still to verify) |
| 4 | Wire schematic (one A3 sheet, boxed blocks), ERC to zero | done: 0 ERC violations, netlist checked against the intended pin map (240 pins) |
| 5 | Docs | `hardware-design.md`, `bom.md`, `pinmap.md`, `README.md` written (uncommitted). Reference designators in `bom.md`; `doc/schematic.pdf` exported. `config.h` deliberately **not** edited (would break the breadboard prototype); edits are listed in `pinmap.md` |
| 6a | Centre mechanics (screen carrier, push button, PCB holes) | done (rev 2: tower + captive riser + knob); 3D print tested by the user, works (2026-11-14). Footprint `Knob_Centre_Mechanics` = MECH1; SW1, SW4, MECH1 placed and locked at (150, 100) |
| 6b | PCB placement | done: 120 × 120 mm outline, 4 × M3, all 87 footprints placed by `tools/pcb_placement/place.py`. J4 changed to an SMD JST-PH near the wire slot (2026-11-14). See `doc/pcb-layout.md` |
| 6c | PCB routing | done by `tools/pcb_routing/route.py`. User moved one `+12V` corner by hand (2026-11-15): **do not re-run `route.py`**, it would undo it. User (2026-11-14): looks fine for now, detailed review later. All nets routed, GND zones on both layers, 0 unconnected, no clearance errors |
| 6d | DRC clean-up and silkscreen | done by `tools/pcb_silk/silk.py`: no DRC errors, no silkscreen warnings; 4 "footprint differs from library" warnings are intended (see `doc/pcb-layout.md`). Front artwork: flag of Chile and four volcanoes (emblem redrawn from real terrain 2026-11-15, `emblem.py`) |
| 6e | JLC DFM check, gerbers, order guide | done 2026-11-15, **uncommitted**: `fab/` (gerber zip, `lcsc-order.csv`, 1:1 print PDFs, `README.md` with the order steps). 3D check not repeated (centre mechanics unchanged since the print test) |
| 6f | Pre-order review | done 2026-11-15, **uncommitted**: full review of parts, values, schematic, PCB. Fixed: J2 part → Amphenol 54602-908LF (the Kinghelm jack did not fit the footprint), J1 → DC005-T20 3 A with 3.6 mm slots, D5 → DSK34, own +5V track C21 → D5, encoder lug keep-outs, KNOB_SW moved off a lug, buck GND pads solid. `tools/pcb_routing/review_fixes.py`. ERC 0, DRC 0 errors, fab files exported again |
| 7 | Order | **next, user**: own final review, paper fit check, checks on owned parts (`fab/README.md`), then JLCPCB + LCSC |

Schematic state committed as `4281a75` (2026-11-13), mechanics and placement
as `dc9236b`, routing and silkscreen as `6022ea4` (2026-11-15). The work
after that (LCSC fields, `fab/`, review fixes, docs) is **uncommitted** — ask
the user before committing. **Do not re-run `place.py` or `route.py`**: the
board has hand edits and the review fixes.

**How the schematic was made:** `tools/gen_schematic/layout.py` (uses
`sch.py`) writes `blinds-pcb.kicad_sch` and compares the netlist exported by
`kicad-cli` with the intended pin→net map. Re-running it overwrites manual
KiCad edits, so use it only until the user starts editing by hand. KiCad
must be closed while it runs (`pgrep -a -x kicad`). The pre-generation files
are in `output/pre-generation-backup/` (git-ignored).

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
- **User answers 2026-11-12:** module is BTT TMC2209 **V1.3** (pin rows in
  `doc/images.jpeg`); AS5600 is the standard module with its own pull-ups and
  caps, powered from 3.3 V; the NodeMCU has **no** diode between USB 5 V and
  the 5V pin; the LCD runs from **3.3 V**.
- Consequences: LCD VIN wired straight to 3V3 (no solder jumper). Panel I2C
  pull-ups are 10k (R7, R8), in parallel with the module's. DIAG/INDEX are not
  socketed and there is no DIAG test pad. TMC pin 5 ("TX") is left open; the
  board's own 1k (R5) feeds pin 4.
- **J4 (LCD) is an SMD side-entry JST-PH** (S8B-PH-SM4-TB), chosen by the
  user on 2026-11-14 instead of the THT JST-XH, so that it can sit 18 mm from
  the wire slot. LCSC C265121.
- **Pre-order review (2026-11-15):** J2 = Amphenol 54602-908LF (C2847314),
  J1 = DC005-T20 (C111567), D5 = DSK34 (C727076); D5 fed by its own track
  from C21. Agreed by the user ("fix the issues that can be done").
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
| TMC DIAG | — | — | **not routed to a GPIO** (no pin left). Left unconnected. Stall detection already comes from the AS5600 closed loop, and StallGuard is readable over UART. |

Firmware edits needed in `firmware/include/config.h`:
`PIN_MOTOR_EN 26`, `PIN_TFT_CS 13`, `PIN_KNOB_SW 34`, `PIN_TMC_RX 35`,
`I2C_FREQ_HZ 100000`, plus new `PIN_LED_DATA 16`, `PIN_VIN_SENSE 36`,
`PIN_AMBIENT 39`. Knob code must not rely on internal pull-ups for GPIO34.

## Circuit blocks (as planned)

The schematic is the reference now. Where this table differs, the Decisions
section wins: no DIAG test pad, no LCD jumper (VIN = 3V3), I2C pull-ups 10k,
LCD connector is J4 and is a JST-PH SMD part.

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

## Still to verify (before / during PCB layout)

- J2: order the Amphenol 54602-908LF (C2847314; LCSC stock not shown, also
  at Digi-Key / RS / element14). No other jack fits the footprint.
- TMC module EN: measure EN to GND; ≈ 10k pull-down → R6 = 2.2k.
- NodeMCU module is a WROOM (GPIO16/17 free), not a WROVER.
- Side switches C285519 (PTS645VK392LFS): pin pattern against the datasheet.
- R14 (4.7k): with the TEPT4400's 200 µA at 100 lx the ADC tops out near
  250 lx; 2.2k if the opening sees daylight. Q1 pad 1 = collector: check the
  part's marking.
- RJ45 contact current rating (≥ 1.5 A) in the Amphenol datasheet.
- J4 pin 1 (BLK) is at x = 143 (left, seen from the front). Check it against
  the cable you will use: a ready-made PH–PH cable may be wired 1:1 or
  reversed.
- The LCD plug slides in 5 mm above the encoder terminals: trim those three
  solder joints flat.
- Final review by the user in KiCad (see `doc/pcb-layout.md`, "Review fixes").
- Mounting holes are not in the schematic; add them in the PCB phase.

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

## PCB phase

Placement, routing and the rules are in `doc/pcb-layout.md` (it replaces the
earlier rule list here). Script order: `place.py` → `route.py` → `silk.py`.
Remaining steps: user review of the routing, JLC DFM check, 3D check against
the printed parts, gerbers + drill files.

## Log

- **2026-11-12** — Read PDF, docs, `config.h`, schematic (via kicad-pro MCP:
  19 symbols, 104 ERC findings). Wrote the plan; user approved with answers
  recorded under Decisions. Verified AP63205 datasheet values and the BTT
  TMC2209 header pinout. Ambient sensor took GPIO39, so DIAG goes to a test
  pad only. Created this file. No other project file touched yet.
- **2026-11-12 (2)** — Branch `hw/panel-schematic` created; baseline commit
  `09e70f2` (adds `.gitignore`, untracks `.history` and `.kicad_prl`).
  `.mcp.json` left untracked. LCSC lookup done with
  `https://jlcsearch.tscircuit.com/api/search?q=<part>` (works with curl; it
  does not list AP63205 or WS2812B 5050, so those numbers are unconfirmed).
  Wrote `doc/bom.md`, `doc/pinmap.md`, `doc/hardware-design.md` and the
  hardware `README.md`; removed `doc/README.md` (identical copy of
  `firmware/README.md`). Docs are not committed yet. PTC changed to a 33 V
  part (1812L200/33GR). KiCad still open at the end of this block, so steps
  1b, 3 and 4 have not started.
- **2026-11-12 (3)** — User closed KiCad and answered the open questions
  (see Decisions). Libraries moved to `lib/` (`blinds.kicad_sym` with
  EC35A1520402, a redrawn ESP32_NODEMCU and a new TMC2209_BTT; footprints in
  `blinds.pretty`; 3D in `lib/3d`), lib tables rewritten, `local_libraries/`
  removed. Schematic regenerated by script on one A3 sheet, 11 boxed blocks,
  footprints and LCSC fields assigned: ERC 0 violations, netlist identical to
  the intended map. `doc/schematic.pdf` exported, docs updated to match.
  The `.kicad_pcb` was not touched. Nothing committed.
- **2026-11-13** — User confirmed: RJ45 footprint OK for now, side switches
  OK, TMC and NodeMCU row spacing correct, AP63205WU-7 = C2071056,
  WS2812B = C114588 (already owned). Ambient sensor must look sideways
  through a side opening of the enclosure (not visible from the front):
  Q1 changed to TEPT4400 (THT 3 mm, horizontal footprint, C132178), R14 to
  4.7k. Schematic regenerated: ERC 0, netlist check OK, PDF re-exported.
  PCB placement note: Q1 at the board edge facing that opening. Nothing
  committed.
- **2026-11-13 (2)** — Committed the schematic state (`4281a75`; `.vscode/`
  ignored, `.mcp.json` left untracked). Designed the centre mechanics in
  CadQuery (`mech/panel_mech.py`, parameters at the top): riser (foot + tube
  + Ø47 plate), bezel Ø50.1, guide block. Display keeps its standoffs and
  JST (PH 2.0, not XH). Differences from the PDF: guide screws at r 7.0 (not
  8.5, so the inserts fit in the Ø21.5 foot), wire slot 7 × 3 at r 7.25 /
  270°, guide block held by 4 × M2 at r 20.5 on the diagonals (clear of the
  encoder's pins, bosses and keep-outs), SW4 rotated 90°. The script also
  writes the footprint `Knob_Centre_Mechanics` and checks interference (all
  zero). Verified in a `kicad-cli` STEP export that the 3D models land in
  the right place. (Superseded by entry (3) below.)
  CadQuery lives in a throw-away venv; recreate it as in `mech/README.md`.
- **2026-11-13 (3)** — Mechanics rev 2 after user review ("riser is
  floating, screen can be pulled off"). Rev 1 did stop pull-out, but only
  through heat-set inserts loaded in their weak direction, with no way to
  set the rest position and an 8 mm guide. Now: fixed tower inside the
  encoder bore (17 mm guide, press hard stop), riser = plate + hub + stem,
  3 × M3×25 from the back into captive nyloc nuts (pull-out stop on the PCB
  back, rest position adjustable), knob designed (Ø50.1, set screws on the
  rotor). Guide block and its 4 × M2 holes are gone; MECH1 now has
  3 × Ø3.4 (unchanged), 2 × Ø3.2 at r 7.5 / 30°, 150°, and the wire slot.
  Layout rule is now: nothing taller than 1 mm inside r 26 (knob), LED ring
  outside it; only screw heads behind the PCB (to −5.1). Checks: zero
  interference in all pairs, DRC around the centre as expected.
- **2026-11-13 (4)** — PCB placement (no routing). Board 120 × 120 mm,
  r 5 corners; smaller does not work because the NodeMCU socket pins must
  miss the LED ring. NodeMCU bottom-left on the back with USB at the bottom
  edge, so the fourth M3 hole is at (124.5, 155) instead of the corner.
  LED ring r 31, D6 at 198° clockwise, D8 at 9 o'clock. RJ45 + TMC top-left,
  light sensor and spare I2C on the top edge, jack + BOOT + RESET on the
  bottom edge, power bottom-right. Antenna keep-out rule area added. 3D:
  sockets under U2/A1, simple models for TMC, RJ45, fuse, SW4
  (`tools/pcb_placement/make_models.py`). Net classes and minimums written
  to `blinds-pcb.kicad_pro`. DRC: only the expected courtyard findings
  (parts under the socketed NodeMCU, SW1/SW4/MECH1) and silk warnings.
  GND plan changed: zones on both layers, front is the main plane.
  Nothing committed.
- **2026-11-14** — User: the printed push-button / screen mechanics work.
  J4 was 37 mm from the wire slot only because it was through-hole (pins
  must miss the knob and the LED ring); user chose an SMD side-entry JST-PH.
  Schematic footprint changed by hand and in `layout.py` (ERC 0); `place.py`
  swaps the footprint on the board and places it at (150, 130), opening
  towards the slot. Placement changes for routing: F1, D2, C1–C3 turned or
  moved, D5 under the NodeMCU, C11 and C13 at the TMC pins. Routing done by
  the new `tools/pcb_routing/route.py` (own grid router; Freerouting could
  not be run in this session). Result: 0 unconnected, no clearance errors,
  parity clean; only the known courtyard / NPTH findings and silk warnings
  remain. `doc/schematic.pdf` re-exported, docs updated. Nothing committed.
- **2026-11-14 (2)** — User: routing looks fine for now, detailed review
  later; asked for DRC fixes, silkscreen and two front drawings. Courtyards
  now describe what touches the PCB: socket strips for U2 and A1 (parts may
  sit under the modules), bore of the encoder left open (SW4 and the MECH1
  holes are inside it). That removed all 13 DRC errors. New
  `tools/pcb_silk/silk.py`: places all reference texts free of pads and
  other silkscreen, adds labels (RJ45 warning, pin names at J3 / J4 / TMC /
  NodeMCU, BOOT, RESET, 12 V jack, board title, `JLCJLCJLCJLC` for the order
  number), clips silkscreen at the board edge, and draws on the front the
  flag of Chile with "Designed in Chile by Gonz" (right of the ring) and
  a logo-style emblem of Lonquimay, Llaima, Villarrica, Osorno with
  "ANDES MADE" (top; redrawn the same day as silhouettes against a banded sky). `route.py`
  keeps stitching vias out of the two drawings (120 instead of 144).
  DRC: 0 errors, 0 unconnected, parity clean; 4 warnings "footprint differs
  from library" (A1, U2, J1, J2), intended. Nothing committed.
- **2026-11-15** — User moved one `+12V` corner by hand (120.75, 152.25 →
  150.25); zones refilled with `pcbnew` only (no script re-run), DRC clean.
  Committed as `6022ea4`. LCSC numbers found for everything: 10k C17414,
  100 Ω C17408, 22k C17560, J4 C265121, 1×8 socket C2905417, side switches
  C285519, LEDs C2295 / C2297; written to the schematic, `layout.py`, the
  board and `bom.md` (ERC 0). J3 pin labels thickened to 0.15 mm (JLCPCB
  minimum), also in `silk.py`. Fabrication files exported to `fab/` with an
  order guide. Not committed. No gerber viewer was available here: the
  visual check is JLCPCB's preview.
- **2026-11-15 (2)** — Pre-order review (user asked for a final check of
  parts, values, schematic, PCB, then to fix what can be fixed). Found and
  fixed: the order list had a Kinghelm RJ45 (1.02 mm pitch, shield pins)
  that does not fit the Amphenol 54602 footprint → J2 is now the Amphenol
  54602-908LF. DC jack C16214 rated 1 A → DC005-T20 (3 A), J1 slots widened
  to 3.6 mm so both fit. The NodeMCU's 5 V came from the far end of the LED
  ring arc (about 0.12 Ω shared with the LEDs) → own track from C21 to D5
  (about 0.06 Ω), and D5 = DSK34 (lower drop). Encoder lug "wiring
  prohibited" areas are now F.Cu keep-out rule areas; KNOB_SW moved off one.
  Buck GND pads solid. D6→D7 data rerouted (56 mm). All by
  `tools/pcb_routing/review_fixes.py` on top of the existing routing; J1
  silkscreen trimmed. ERC 0; DRC 0 errors, 0 unconnected, parity clean, the
  4 intended warnings. Gerbers, zip, prints, `doc/schematic.pdf` exported
  again; docs, `bom.md`, `lcsc-order.csv`, `layout.py` updated. Verified
  against datasheets: AP63205, 74AHCT1G125, WS2812B, NodeMCU 38-pin order,
  BTT TMC2209 V1.3 (UART on pin 4), ALPS EC35A footprint, TEPT4400.
  Not changed (rev B ideas): 22 µF 1206/25 V output caps, series resistors
  on SDA/SCL at the RJ45, unidirectional TVS symbol for D2 (board is right:
  band to pad 1). Nothing committed.
- **2026-11-15 (3)** — User found the volcano silhouettes too cartoony and
  asked for realistic ones and a nicer logo. New `tools/pcb_silk/emblem.py`
  renders the four volcanoes from the Copernicus GLO-90 elevation model
  (true profiles and snow lines, 3.3x vertical exaggeration, placed close
  together), as a line engraving: solid lit snow, lined sky with
  Villarrica's plume, dark forest, lake with ripples; frame, "ANDES MADE"
  in Lato Bold between rules, names below. Output `emblem.json` (polygons),
  drawn by `silk.py --emblem`, which replaces only the drawings inside the
  emblem box (old emblem: 45 drawings; reference texts untouched). Lines
  and gaps ≥ 0.16 mm. DRC unchanged (4 intended warnings, 0 unconnected,
  parity clean). Only `F_Silkscreen.gto` changed in the gerbers; zip and
  prints exported again. Nothing committed.
- **2026-11-17** — New project `../motor-pcb` (motor encoder board, rev A):
  42.3 mm square back plate for the NEMA 17 (M3 on the 31 mm pattern),
  AS5600 on the shaft axis (motor side) with 100 nF + 1 µF and a DIR solder
  jumper; RJ45 with the same pinout as J2, JST-XH 4-pin to the motor,
  4.7k I2C pull-ups, PESD3V3L2BT ESD clamp, 10 µF, 3V3 LED, 3 test pads.
  Made by scripts (`tools/gen_schematic.py`, `build_pcb.py`, `silk.py`,
  `emblem_small.py`). ERC 0, DRC 0, parity clean, fab files in
  `motor-pcb/fab/`. Small flag + volcano emblem regenerated from the DEM
  (the panel's `emblem.json` was not in the repo). Logos also saved as SVG
  in `../artwork/` (`export_svg.py`).
