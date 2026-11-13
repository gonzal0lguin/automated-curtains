# Bill of materials — control panel board

Everything is chosen to be **hand-solderable**: 0805 passives, SOT-23-class
ICs, through-hole connectors. Parts are listed per block; the reference
designators (R1, C1, …) are in the table at the end and in the schematic.

**LCSC column:** "✔" = number, package and stock checked on 2026-11-12
(jlcsearch index of the JLCPCB/LCSC catalogue). "?" = number from memory or
not found in the index — **check it on lcsc.com before ordering**.
Equivalent parts from any shop are fine as long as value, voltage rating and
package match.

## Power input and 5 V buck

| Part | Value / type | Package | Qty | LCSC | Notes |
|---|---|---|---|---|---|
| DC barrel jack | DC-005, 5.5 × 2.0/2.1 mm, centre positive | THT | 1 | C16214 ✔ | Check your adapter plug: 2.1 mm pin plugs need the 2.1 version |
| Resettable fuse (PTC) | 2 A hold, 33 V — 1812L200/33GR | 1812 | 1 | C18198343 ✔ | Must be rated above 12 V; the common 16 V ones are too close |
| Series Schottky | SS54, 40 V 5 A | SMA | 1 | C7420369 ✔ | Reverse-polarity protection |
| TVS diode | SMBJ15A, 15 V stand-off | SMB | 1 | C726824 ✔ | Clamps plug-in spikes |
| Buck regulator | AP63205WU-7, fixed 5 V 2 A | TSOT-26 | 1 | C2071056 | Number confirmed by the owner |
| Inductor | 4.7 µH shielded, Isat ≥ 3 A — FNR6045S4R7MT | 6 × 6 mm | 1 | C168067 ✔ | Low stock (862); any 6×6 shielded 4.7 µH ≥ 3 A works. Confirm Isat in its datasheet |
| Input capacitor | 10 µF 50 V X5R — CL31A106KBHNNNE | 1206 | 2 | C13585 ✔ | Two, because ceramics lose capacitance under DC bias |
| Output capacitor | 22 µF 16 V — CL21A226MOQNNNE | 0805 | 2 | C98190 ✔ | |
| Bootstrap + bypass | 100 nF 50 V X7R — 0805B104K500NT | 0805 | 2 | C38141 ✔ | One BST–SW, one on VIN |

## NodeMCU

| Part | Value / type | Package | Qty | LCSC | Notes |
|---|---|---|---|---|---|
| Female header | 1 × 19, 2.54 mm | THT | 2 | C2932678 ✔ | Socket for the 38-pin NodeMCU; check row spacing of your board first |
| Feed Schottky | B5819W, 40 V 1 A | SOD-123 | 1 | C8598 ✔ | Between +5V and the NodeMCU 5V pin |
| Decoupling | 10 µF (as above) + 100 nF | 1206 / 0805 | 1 + 2 | — | 5V pin and 3V3 pin |
| RESET, BOOT buttons | Right-angle 6 mm tact switch (C&K PTS645V type) | THT | 2 | ? | On LCSC but out of stock on 2026-11-13; any shop's PTS645V-compatible part works |

## Stepper driver

| Part | Value / type | Package | Qty | LCSC | Notes |
|---|---|---|---|---|---|
| TMC2209 module | BigTreeTech TMC2209 | StepStick | 1 | — | You already have it |
| Female header | 1 × 8, 2.54 mm | THT | 2 | ? | Socket for the module |
| Bulk capacitor | 100 µF 35 V low-ESR electrolytic | THT 6.3 × 11 mm | 1 | C109392 ✔ | Right next to VM/GND |
| Bypass | 100 nF | 0805 | 2 | C38141 ✔ | VM and VIO |
| UART resistor | 1 kΩ 1 % | 0805 | 1 | C17513 ✔ | ESP32 TX → PDN_UART |
| EN pull-up | 10 kΩ 1 % | 0805 | 1 | C17414 ? | |

## LED ring

| Part | Value / type | Package | Qty | LCSC | Notes |
|---|---|---|---|---|---|
| Addressable LED | WS2812B | 5050 | 10 | C114588 | Already in hand |
| LED bypass | 100 nF | 0805 | 10 | C38141 ✔ | One per LED |
| Level shifter | SN74AHCT1G125DBVR | SOT-23-5 | 1 | C7484 ✔ | Must be AHC**T** (TTL thresholds) |
| Shifter bypass | 100 nF | 0805 | 1 | C38141 ✔ | |
| Series resistor | 100 Ω | 0805 | 1 | C17408 ? | After the buffer |
| Pull-down | 10 kΩ | 0805 | 1 | C17414 ? | On the ESP32 side |
| Ring bulk cap | 470 µF 16 V electrolytic | THT 8 × 12 mm | 1 | C2943989 ✔ | |

## Knob, button, sensors

| Part | Value / type | Package | Qty | LCSC | Notes |
|---|---|---|---|---|---|
| Ring encoder | ALPS EC35A1520402 | THT | 1 | — | You already have it |
| Centre tact switch | TS-1187A (5.1 × 5.1 mm) | SMD 4-pad | 1 | C318889 ✔ | Pick the actuation force you like (160–260 gf) |
| Pull-ups | 10 kΩ | 0805 | 3 | C17414 ? | Knob A, B, switch |
| Filter resistors | 10 kΩ | 0805 | 2 | C17414 ? | Knob A, B series |
| Filter caps | 10 nF 50 V X7R — CL21B103KBANNNC | 0805 | 2 | C1710 ✔ | Knob A, B |
| Switch debounce cap | 100 nF | 0805 | 1 | C38141 ✔ | |
| Ambient light sensor | Vishay TEPT4400, 3 mm ambient-light phototransistor | THT, lying flat | 1 | C132178 ✔ | Side-looking: placed at the board edge, facing the side opening of the enclosure |
| Light sensor load | 4.7 kΩ | 0805 | 2 | C17673 ✔ | Starting value, not checked against the datasheet: raise it if readings are too low, lower it if they saturate in bright light |
| 12 V sense divider | 100 kΩ + 22 kΩ | 0805 | 1 + 1 | C149504 ✔ / C17560 ? | |
| 12 V sense filter | 100 nF | 0805 | 1 | C38141 ✔ | |

## Connectors and indicators

| Part | Value / type | Package | Qty | LCSC | Notes |
|---|---|---|---|---|---|
| RJ45 jack | 8P8C, **no magnetics** — KH-RJ45-56-8P8C | THT right-angle | 1 | C2683359 ✔ | Confirm contact current (≥ 1.5 A). Schematic uses the Amphenol 54602 footprint as a placeholder: compare with this part's drawing before layout |
| LCD connector | JST-XH 8-pin header (XH-8A) | THT | 1 | C10385 ✔ | |
| I2C header | JST-XH 4-pin (B4B-XH-A) | THT | 1 | C594232 ✔ | |
| I2C pull-ups | 10 kΩ | 0805 | 2 | C17414 ? | In parallel with the ones on the AS5600 module |
| Outgoing 3V3 bypass | 100 nF | 0805 | 1 | C38141 ✔ | At the RJ45 |
| LCD bypass | 10 µF + 100 nF | 1206 / 0805 | 1 + 1 | — | |
| Power LEDs | Red (12 V), green (5 V) | 0805 | 2 | ? | |
| LED resistors | 4.7 kΩ (12 V), 1 kΩ (5 V) | 0805 | 1 + 1 | C17673 ✔ / C17513 ✔ | About 2 mA each |

## Totals of the common passives

| Value | Package | Qty |
|---|---|---|
| 100 nF 50 V X7R | 0805 | 21 |
| 10 kΩ | 0805 | 9 |
| 10 µF 50 V | 1206 | 4 |
| 4.7 kΩ | 0805 | 2 |
| 1 kΩ | 0805 | 2 |
| 22 µF 16 V | 0805 | 2 |
| 10 nF | 0805 | 2 |
| 100 Ω, 22 kΩ, 100 kΩ | 0805 | 1 each |

Buy spares of everything small: 0805 parts are cheap and easy to lose.

## Reference designators

| Refs | Part | Block |
|---|---|---|
| J1, F1, D1, D2 | DC jack, PTC fuse, SS54, SMBJ15A | 1 Power input |
| U1, C1–C2 (10 µF), C3 (100 nF), C4 (100 nF bootstrap), L1, C5–C6 (22 µF) | Buck | 2 5 V buck |
| R1 + D3 (12 V LED), R2 + D4 (5 V LED), R3 (100k), R4 (22k), C7 | Indicators, 12 V sense | 3 |
| TP1 +12V, TP2 +5V, TP3 +3V3, TP4 GND, TP5 LED_DOUT | Test pads | 4 |
| U2, SW2 (RESET), SW3 (BOOT), D5 (B5819W), C8 (10 µF), C9, C10 | NodeMCU | 5 |
| A1, C11 (100 nF), C12 (100 µF), C13, R5 (1k UART), R6 (10k EN pull-up) | TMC2209 | 6 |
| J2 (RJ45), R7–R8 (10k I2C pull-ups), C14, J3 (spare I2C) | Motor + encoder link | 7 |
| SW1 (EC35), R9 + R11 (10k series), R10 + R12 (10k pull-ups), C15–C16 (10 nF), SW4, R13, C17 | Knob + centre button | 8 |
| J4, C18 (10 µF), C19 | LCD | 9 |
| Q1, R14 | Ambient light | 10 |
| U3, R15 (10k pull-down), R16 (100 Ω), C20, C21 (470 µF), D6–D15, C22–C31 | LED ring | 11 |
