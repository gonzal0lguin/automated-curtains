# Hardware design notes — control panel board

What each part of the circuit does and why its values were chosen, so every
decision can be checked. Companion files: [`pinmap.md`](pinmap.md) (pins and
connectors), [`bom.md`](bom.md) (parts), [`PLAN.md`](PLAN.md) (work log).

Status: **design defined, schematic not wired yet.** Items marked *to check*
are open.

## Overview

```
12 V jack ─ fuse ─ diode ─ TVS ─┬─────────────── +12V ──► TMC2209 VM ──► motor (RJ45)
                                 │                  └────► 12 V sense divider ─► GPIO36
                                 └─ AP63205 buck ─ +5V ─┬─► WS2812 ring + level shifter
                                                         └─ diode ─► NodeMCU 5V pin
                                                                       └─ on-board regulator ─ +3V3 ─► knob, LCD,
                                                                          TMC2209 logic, AS5600 (RJ45), sensors
```

Three rails:

| Rail | Source | Loads | Worst case |
|---|---|---|---|
| +12V | Wall adapter, 12 V 2 A | Motor driver, buck | Motor ≈ 1 A + buck input ≈ 0.6 A |
| +5V | AP63205 buck (2 A) | NodeMCU, 10 LEDs, level shifter, | ≈ 1.2 A |
| +3V3 | NodeMCU's own regulator | ESP32, TMC2209 logic, AS5600, pull-ups, LCD | Small external loads only |

## 1. Power input

**Chain:** barrel jack → PTC fuse → series Schottky → TVS → `+12V`.

- **PTC fuse, 2 A hold, 33 V.** Opens on a short and resets when it cools.
  The voltage rating must exceed the supply; 16 V parts are too marginal for
  a 12 V adapter that may idle at 13 V or more.
- **SS54 series Schottky.** If a centre-negative adapter is plugged in,
  nothing conducts. Cost: about 0.4–0.5 V drop, so `+12V` is really ≈ 11.5 V.
  The motor does not care. Power lost at 1.5 A ≈ 0.7 W, fine for an SMA part
  with some copper around it.
- **SMBJ15A TVS.** Plugging a live adapter into ceramic capacitors can ring
  to nearly twice the input voltage. The TVS ignores anything up to 15 V and
  clamps spikes at roughly 24 V, below the TMC2209 (28 V) and AP63205 (35 V
  absolute maximum) limits.
- **Order matters:** the TVS sits after the diode so a reversed supply cannot
  forward-bias it.

## 2. 5 V buck (AP63205WU)

A buck converter instead of a linear regulator because dropping 12 V to 5 V
at 1 A linearly would burn 7 W; the buck loses about 0.5 W.

Values come straight from the datasheet (Diodes DS41326, Table 3, fixed 5 V):

| Part | Value | Why |
|---|---|---|
| Inductor | 4.7 µH, shielded, Isat ≥ 3 A | Datasheet value. Saturation current must exceed the 2 A load plus ripple. Shielded keeps noise away from the knob and I2C. |
| Input cap | 2 × 10 µF 50 V (1206) + 100 nF | Datasheet asks 10 µF. A 10 µF ceramic has roughly half its capacitance left at 12 V bias, hence two, and 50 V rated. |
| Output cap | 2 × 22 µF 16 V | Datasheet value. |
| Bootstrap cap | 100 nF, BST to SW | Datasheet value; powers the internal high-side switch driver. |
| FB (pin 1) | Wired directly to the 5 V output | Fixed-output part: no feedback divider. |
| EN (pin 2) | Tied to VIN | Datasheet: "attach to VIN or leave open for automatic startup". Rated to 35 V. |

Pinout (TSOT-26): 1 FB, 2 EN, 3 VIN, 4 GND, 5 SW, 6 BST.

**This is the one block where layout matters a lot** (see PCB notes): input
capacitor tight against VIN/GND, short SW node, ground plane underneath.

## 3. NodeMCU supply

- `+5V` reaches the NodeMCU 5V pin through a **B5819W Schottky**. Without it,
  plugging in USB while the 12 V adapter is off would power the LED ring and
  back-feed the buck output from your computer's USB port. With it, USB only
  powers the NodeMCU. The pin then sees about 4.6 V, enough for the on-board
  3.3 V regulator.
- 10 µF + 100 nF at the 5V pin, 100 nF at the 3V3 pin.
- This NodeMCU has **no diode** between its USB 5 V and the 5V pin (confirmed
  by the owner). So with both supplies connected, whichever is higher feeds
  the module, and D5 keeps USB away from the buck and LEDs. One case to
  avoid: 12 V on and the USB cable in a switched-off computer — about 4.6 V
  would then reach that computer's USB port.

## 4. Stepper driver (BigTreeTech TMC2209 module)

| Module pin | Connects to | Why |
|---|---|---|
| VM | `+12V`, with 100 µF / 35 V + 100 nF at the pins | The driver draws current in fast pulses; without a bulk capacitor the supply spikes can destroy it. Trinamic and the firmware README both call for 100 µF. |
| GND (both) | GND | |
| VCC_IO | `+3V3` + 100 nF | Sets the logic level to match the ESP32. |
| EN | GPIO26, 10k pull-up to 3V3 | EN is active low. The pull-up keeps the motor off while the ESP32 boots or is being flashed. |
| STEP / DIR | GPIO27 / GPIO14 | |
| MS1, MS2 | GND | In UART mode they set the address: both low = address 0 (`TMC_ADDRESS 0`). |
| PDN_UART | GPIO35 directly, GPIO17 through 1 kΩ | Single-wire UART: TX and RX share the pin, the resistor stops them fighting. |
| CLK | GND | Selects the internal clock. |
| A1, A2, B1, B2 | RJ45 pins 1, 2, 3, 6 | One coil per twisted pair. |
| PDN_UART_TX (pin 5, printed "TX") | Not connected | The board has its own 1 kΩ (R5), so only pin 4 is used. |
| DIAG, INDEX | Not connected | No free GPIO, and no socket hole: the pins hang free above the board. |

The module goes in two 1 × 8 female headers, so a dead driver is a swap.

The module is a **V1.3**; pin order was taken from its silkscreen
(`doc/images.jpeg`): EN, MS1, MS2, RX, TX, CLK, STEP, DIR on one row and VM,
GND, A2, A1, B1, B2, VIO, GND on the other. The footprint is the standard
StepStick outline (two rows of 8, 12.7 mm apart) — measure the row spacing
once with calipers before ordering the PCB.

**Motor current.** The module ships at about 0.9 A RMS (VREF 1.2 V). The
firmware sets current over UART; keep it at or below about 0.8–1 A RMS
because the current also flows through the RJ45 contacts and 24AWG wire.

## 5. LED ring (10 × WS2812B)

- **Why a level shifter.** A WS2812B powered from 5 V wants at least 3.5 V
  for a logic high. The ESP32 gives 3.3 V: it often works on the bench and
  then flickers. The **74AHCT1G125** runs from 5 V, accepts 3.3 V as high
  (TTL thresholds — that is the "T" in AHCT) and outputs a clean 5 V. Its
  enable pin is tied to GND (always on).
- **10k pull-down** on the buffer input keeps the LEDs dark while the ESP32
  pin floats during boot.
- **100 Ω series resistor** after the buffer damps ringing on the trace.
- **100 nF at every LED**, plus one **470 µF** capacitor where 5 V enters the
  ring.
- **Current.** Up to 60 mA per LED at full white → 0.6 A for ten. Limit it in
  firmware (for example FastLED's `setMaxPowerInVoltsAndMilliamps`).
- The last LED's DOUT goes to a test pad, handy for debugging or extending.

## 6. Knob (ALPS EC35A1520402) and centre button

- Encoder outputs A and B each get the filter commonly used with mechanical
  encoders: **10k pull-up**, then **10k in series with 10 nF to GND**. The
  RC (0.1 ms) removes the fastest contact noise without slowing fast turns.
  The EC35A datasheet allows up to 5 ms of chatter, so the firmware's
  quadrature decoding still does the real debouncing, as it does today.
- Common pin C and the metal mounting tabs go to GND.
- 15 pulses / 30 detents → 2 transitions per detent, which matches
  `KNOB_TRANSITIONS_PER_DETENT 2`.
- **Centre button:** a tact switch on the knob axis, pressed by the floating
  screen riser described in the PDF. 10k pull-up + 100 nF. The pull-up is
  mandatory here because GPIO34 has none inside.

## 7. Display connector (GC9A01)

8-pin JST-XH in the module's own order (BLK, CS, DC, RES, SDA, SCL, VIN,
GND). BLK is driven directly by PWM from GPIO25, as it is today. VIN is
fed from 3V3, as on the breadboard; 10 µF + 100 nF sit at the connector.

## 8. Motor and encoder connector (RJ45)

Pinout in [`pinmap.md`](pinmap.md). Design rules for this port:

1. **Jack without magnetics.** Ethernet "MagJacks" contain transformers that
   block DC and I2C.
2. **Current.** Each coil wire carries the full phase current. 24AWG copper
   and jack contacts are comfortable to roughly 1 A; that is the reason for
   the current cap above. Use solid-copper 24AWG cable, not copper-clad
   aluminium, and keep it short (1 m).
3. **I2C next to motor wires.** Run at 100 kHz, 100 nF on the outgoing 3V3.
   The AS5600 module has its own pull-ups; the panel adds 10k ones (R7, R8)
   so the bus is still defined when the cable is unplugged and only the
   spare I2C header is used.
4. **Never unplug while powered.** Breaking a coil circuit under current
   makes an inductive spike that can kill the driver. This goes on the
   silkscreen, together with "NOT ETHERNET".
5. *To check before ordering the PCB:* run the real motor and AS5600 through
   the actual 1 m cable on the breadboard and watch for I2C read errors.

The PDF's alternative (driver on a small board at the motor, only DC and
logic in the cable) removes points 2–4. It stays as the upgrade path if this
port gives trouble.

## 9. Sensing and extras

- **12 V sense.** 100k over 22k divides 12 V to 2.16 V (15 V → 2.70 V), inside
  the ESP32 ADC range. 100 nF across the lower resistor filters noise. Use:
  the firmware can notice power dropping and save the blind position before
  the rails collapse, and can report supply voltage.
- **Ambient light.** TEPT4400 3 mm phototransistor (Q1) from 3V3, with 4.7k
  (R14) to GND; the voltage on GPIO39 rises with light. It is a through-hole
  part mounted lying flat at the board edge, so it looks sideways through the
  opening on the side of the enclosure, which is not visible from the front.
  R14 sets the sensitivity: tune it on the finished unit.
- **RESET and BOOT buttons**, side-mounted: RESET pulls the NodeMCU EN pin to
  GND, BOOT pulls GPIO0 to GND.
- **Spare I2C header** (3V3, GND, SDA, SCL) for future sensors.
- **Power LEDs** on 12 V (4.7k → ≈ 2 mA) and 5 V (1k → ≈ 3 mA).
- **Test points** on +12V, +5V, +3V3, GND and LED DOUT.

## PCB notes (for the layout phase)

| Topic | Rule |
|---|---|
| Stack | 2 layers, 1.6 mm, 1 oz copper; bottom layer mostly solid GND |
| Track widths | +12V and motor phases 1.0 mm; +5V 0.6–0.8 mm; signals 0.25 mm |
| Clearance / vias | 0.2 mm minimum; vias 0.6 mm pad / 0.3 mm drill |
| Buck | Copy the datasheet layout: input caps touching VIN/GND, tiny SW copper, inductor next to SW, GND vias at the caps, no signals under the inductor |
| Driver | Bulk capacitor within a few mm of VM/GND |
| Antenna | NodeMCU antenna end hangs over the board edge or over a copper-free zone; keep it away from the encoder's metal body and the LCD |
| Noisy vs quiet | Motor traces away from I2C, knob and ADC traces |
| Hand soldering | `_HandSolder` footprints, thermal reliefs on GND pads, no parts closer than ~1 mm |
| Mechanics | Encoder bore features from the PDF (centre switch, guide-pin holes, wire slot); 10 LEDs every 36° |

## Bring-up order

1. Without NodeMCU, driver or LCD: apply 12 V from a current-limited supply
   (≈ 100 mA limit). Check +12V ≈ 11.5 V and +5V = 5.0 V.
2. Plug in the NodeMCU; check 3V3. Flash over USB with it socketed.
3. Run the test firmwares in order: `knob`, `display`, then the LED ring.
4. Plug in the driver (motor disconnected), then the motor cable; run
   `encoder`, `motor`, `closed_loop`.

## Sources

- Diodes Inc. AP63200/1/3/5 datasheet (DS41326): pinout, Table 3, EN, layout.
- BigTreeTech TMC2209 wiki (global.bttwiki.com/TMC2209.html): header pinout,
  sense resistor, VREF.
- Watterott SilentStepStick TMC2209 pin configuration: EN, MS1/MS2, SPREAD.
- ALPS EC35A datasheet: `doc/datasheets/EC35A1520402_ALPS_ELECTRIC.pdf`.
- Earlier design conversation: `Curtain control panel hardware design.pdf`.
