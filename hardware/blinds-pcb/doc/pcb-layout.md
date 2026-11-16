# PCB layout: placement and routing rules

State: **placed, routed, GND zones filled, silkscreen done** (2026-11-14),
**pre-order review fixes applied** (2026-11-15, see "Review fixes").
DRC: no errors; four intended warnings, see "DRC state". Fabrication files
are in `fab/` (2026-11-15, see `fab/README.md`).

> **Do not re-run `place.py` or `route.py`.** The board now holds a hand-moved
> `+12V` corner and the review fixes; both scripts would undo them. `silk.py`
> only writes silkscreen; after the review fixes it was run only as
> `silk.py --emblem`, which replaces the front emblem and nothing else. A full
> run moves reference texts again; if you do it, run DRC again: J1's outline
> was trimmed by hand.

The three steps were scripts, and all overwrite manual edits:

    python3 tools/pcb_placement/place.py    # positions (KiCad closed)
    python3 tools/pcb_routing/route.py      # tracks, vias, GND zones
    python3 tools/pcb_silk/silk.py          # reference texts, labels, artwork

Run them in this order: `route.py` after `place.py`, `silk.py` after
either. Once you move parts, tracks or texts by hand in KiCad, stop using
the script for that step.

Coordinates are KiCad board mm, front view (knob side), Y down. The knob axis
is at (150, 100).

## Board

- Outline 120 × 120 mm (x 90–210, y 40–160), corner radius 5 mm, 2 layers,
  1.6 mm.
- Front: encoder, centre switch, printed centre mechanics, LED ring and its
  capacitors. Everything else is on the back.
- M3 holes (Ø3.2): H1 (95, 45), H2 (205, 45), H3 (205, 155) in the corners,
  H4 at (124.5, 155). H4 is not in the corner because the NodeMCU sits there.

Why 120 mm: the NodeMCU socket pins come through to the front and must miss
the LED ring. With the USB connector at the bottom edge, the inner pin row
(x = 118) starts at y = 109 and only clears the ring on a 120 mm board with an
LED exactly at 9 o'clock.

## Where things are

| Area (front view) | Parts |
|---|---|
| Centre | SW1 encoder, SW4, MECH1 (locked) |
| Ring r 31 | D6–D15 with C22–C31, front side |
| Bottom-left, back | U2 NodeMCU, USB at the bottom edge, antenna pointing up; C8, C9, C10, D5, TP3 under the module |
| Top-left, back | J2 RJ45 on the top edge, A1 TMC2209 below it, C12 above it, C11 left of pins 15/16, C13 under the module, R5, R6 |
| Top edge, back | J3 spare I2C (R7, R8, C14), Q1 light sensor looking up (R14) |
| Bottom edge, back | SW3 BOOT, SW2 RESET, J1 DC jack |
| Below the knob axis, back | J4 LCD connector (SMD, side entry) with its opening 18 mm below the wire slot, C18, C19 below it; knob filters R9–R12, C15, C16 left and right of the encoder terminals |
| Bottom centre, back | U3 level shifter (R15, R16, C20) |
| Bottom-right, back | F1, D1, D2, buck U1 / L1 / C1–C6, C21, power LEDs, TP1, TP2, TP4 |
| Left of centre, back | R13, C17 (centre button), R3, R4, C7 (12 V sense) |

The top-right quarter of the back is empty.

### LCD connector (J4)

J4 is a surface-mount JST PH (S8B-PH-SM4-TB) at (150, 130) on the back. The
wire slot is at (150, 107.25); the connector opening is at y = 125 and faces
it, so the eight wires run 18 mm straight down the back of the PCB, over the
encoder terminals. Pin 1 (BLK) is at x = 143, pin 8 (GND) at x = 157.

It was a through-hole JST-XH at (138, 144.5), 37 mm from the slot. A
through-hole part cannot come closer than about 31 mm: its pins come out on
the front and must miss the knob (r 26) and the LED ring (to r 35).

### Changes made for routing

- F1 and D2 turned 180°, so jack → fuse → diode → TVS → buck is a straight
  line of pads with no crossing.
- C1, C2 turned so `+12V` is on the side where it arrives; C3 moved to sit
  across U1 pins 3 (VIN) and 4 (GND).
- D5 moved under the NodeMCU, out of the gap used by the `+12V` trunk.
- C11 moved to the left of A1 pins 16 (VM) and 15 (GND); C13 moved under the
  TMC2209 module next to pin 10 (VIO).

### Review fixes (2026-11-15)

Applied by `tools/pcb_routing/review_fixes.py` on top of the existing
routing (it keeps every other track; it refuses to run twice):

- **J1 slots 3.0 → 3.6 mm**, pad copper 4.2 mm along the slot, pad 2 moved
  0.15 mm (sleeve pin at 6.15 mm from the centre pin, between the 6.0 mm of
  the DC-005 and the 6.2 mm of the DC005-T20). Both jacks fit. J1's
  silkscreen lines were shortened to clear the larger pads.
- **Encoder lug keep-outs:** four rule areas "EC35 lug keep-out 1–4" on F.Cu
  (see Keep-outs).
- **`KNOB_SW`:** the front piece that ran 0.2 mm from the bottom-left lug
  square was removed; R13 now reaches C17 through a short back/front/back hop
  at x = 134.75.
- **NodeMCU 5 V:** the branch that left the ring arc near D6 (its far end)
  was removed. D5 has its own 0.5 mm track from C21: front along y = 139 to
  x ≈ 130, then short back/front hops past the LED data and NodeMCU pins to
  D5 (5 vias, about 62 mm, about 0.06 Ω). An 0.8 mm track does not fit
  there. U3 and C20 still take their 5 V from the arc (a few mA).
- **D6 → D7 data** routed again: 56 mm (was 65 mm). A shorter path is
  blocked by the parts and the `+5V` spoke near D6; at 800 kHz this does not
  matter.
- **Buck GND pads** U1.4, C1.2, C2.2, C3.2, C5.2, C6.2 and D2.2: zone
  connection "solid" (pad setting), no thermal spokes.

### LED ring

Ten LEDs every 36°, clockwise seen from the front. D6 is at 198° from
12 o'clock (just left of 6 o'clock); D8 is at 9 o'clock, D13 at 3 o'clock.
There is no LED at 12 or 6 o'clock; the ring is symmetric about both axes.
+5 V pads face inwards, GND pads outwards. Firmware needs an index offset to
make "LED 0" point up.

### Keep-outs

- Nothing taller than 1 mm on the front inside r 26 (knob).
- No through-hole part of the back may have pins inside r 26 or under a front
  part; `place.py` checks this.
- Rule area "ESP32 antenna keep-out" (x 94.5–115.8, y 98–110.3): no tracks,
  vias or copper fill on either layer.
- Back side at the centre: washer and screw-head circles of MECH1 (`B.Fab`).
- Rule areas "EC35 lug keep-out 1–4" (F.Cu): the ALPS "wiring prohibited"
  squares where the encoder's metal lugs sit (r 14.3–18.2 on the diagonals,
  4.4 mm wide), plus 0.3 mm. No tracks or vias; GND fill allowed (the lugs
  are GND).

## Heights behind the PCB (from the 3D models)

| Part | Height |
|---|---|
| Female headers (2.54 mm sockets) | 8.5 mm |
| NodeMCU on its sockets | about 15.6 mm |
| TMC2209 on its sockets, with heatsink | about 17.6 mm (heatsink 5 mm assumed) |
| RJ45 (Amphenol 54602-908LF) | about 13 mm (assumed envelope; check the datasheet) |
| DC jack | 11 mm |

Models added by hand: sockets under U2 and A1, `lib/3d/TMC2209_BTT_V1.3.step`,
`RJ45_8P8C_block.step`, `Fuse_1812_block.step`, `TS-1187A_block.step`
(simple shapes from `tools/pcb_placement/make_models.py`). They are set on
the board footprints, not in the libraries: "Update footprint from library"
on A1, U2, J2, F1 or SW4 removes them, and `place.py` puts them back.

## Routing

### How it was done

`tools/pcb_routing/route.py` is a small grid router (0.25 mm grid, 45° moves,
two layers). The plan at the bottom of the file fixes the order and the
important paths; KiCad's DRC checks the result.

1. Fixed pieces: the `+5V` arc inside the LED ring, the exits of RJ45 pins 3
   and 7 between their neighbours.
2. Input chain and buck, back layer.
3. `+12V` trunk with waypoints, then its thin branches (R1, R3, TP1).
4. Motor phases, `+5V`, LED data, `+3.3V`, then all signals, shortest first.
5. GND: vias at the buck and decoupling capacitors, stitching vias on an
   8 mm lattice, zones on both layers. The script then checks which pads
   the fill really reaches and adds a track where it does not.

Freerouting was not used (the session was not allowed to run a downloaded
program); the script has no dependency besides KiCad's Python.

### What is where

- **`+12V` trunk** (1.0 mm, back, no vias): D1 → along the bottom edge under
  the jack → up x = 120.75 beside the NodeMCU pin row → left at y ≈ 95,
  below the TMC2209 → up x = 93.5 → VM pin, C11, C12.
- **Buck:** C3 directly across VIN/GND, `SW` is 3.5 mm long, the feedback
  line goes from the output capacitors round the top of L1 (not under it).
- **Motor:** A1, B1, B2 on the back, A2 on the front. All four pins are
  through-hole at both ends, so there are no vias. `MOT_B1` is 0.5 mm wide
  for its first 4 mm, between RJ45 pins 2 and 4.
- **LED ring (front):** `+5V` arc at r 28.2, 0.8 mm, open at 6 o'clock. It is
  fed from C21's pin at its D15 end (165°) and runs through 3, 12 and
  9 o'clock to its D6 end (193°). 0.5 mm spokes to each LED and capacitor;
  data from LED to LED outside the arc; GND through the front zone.
- **NodeMCU 5 V:** own 0.5 mm track from C21 to D5 (see "Review fixes"),
  so the LED current does not flow through it.
- **Signals:** mostly on the back. Signals that cross the `+12V` trunk jump
  on the front, usually starting at a NodeMCU pin.
- **Totals:** about 1.9 m of track on the back, 1.1 m on the front, 231
  vias (147 GND, 120 of them stitching; none under the front drawings).

Narrower than the net class, on purpose: `/SW`, `/5V_MCU`, LED spokes,
the `+5V` feed from C21 to D5 and the `+12V` branches to R1, R3, TP1 and U1
(0.5 mm); the feedback line (0.25 mm). R2 (5 V power LED, 3 mA) hangs on
the feedback line; harmless.

### GND

Zones on both layers, thermal reliefs on all pads except the buck's GND
pads (U1, C1–C3, C5, C6, D2), which are solid. Two pads cannot get two
thermal spokes and are joined by a 0.5 mm track instead (zone connection
"none" on the pad): U2 GND pin J4_1 (beside the antenna keep-out) and J3
pin 2. `route.py` sets and resets this itself.

### DRC state

No errors: no unconnected items, no clearance, width, hole or courtyard
errors, no silkscreen warnings, schematic parity clean.

How the earlier courtyard errors were removed (`place.py`,
`fix_courtyards`): a courtyard now covers only what touches the PCB.

- U2 and A1 stand on 8.5 mm sockets, so their courtyards are the two socket
  strips. C8, C9, C10, D5, TP3 and C13 sit under the modules.
- The encoder's courtyard has its bore (r 11.6) cut out. SW4 and the MECH1
  holes are inside the bore.

Left, 4 warnings "footprint does not match the library copy", intended:

- A1: courtyard changed as above (the footprint is KiCad's Pololu one).
- U2, J1, J2: their silkscreen outline is cut back 0.6 mm from the board
  edge. J1 also has the 3.6 mm slots of the review fixes.

"Update footprint from library" on these undoes the change. Do not do it on
J1 (the slots would shrink back to 3.0 mm, too tight for the DC005-T20).

## Silkscreen

`tools/pcb_silk/silk.py` writes everything on the two silkscreen layers.

- **Reference texts:** 0.8 mm high, 0.15 mm stroke, moved to the nearest
  spot beside the part that is free of pads, other silkscreen, other parts
  and via holes. Test points show their net name, not "TPn".
- **Labels, back:** "NOT ETHERNET / DO NOT UNPLUG WHILE POWERED" under the
  RJ45; pin names at J3 (3V3 GND SDA SCL) and at J4 (BLK … GND, above the
  opening); VM, EN, GND, DIR at the corners of the TMC2209 socket; ANTENNA,
  USB, 3V3, GND at the NodeMCU socket; BOOT, RESET, "12V DC CENTRE +",
  LIGHT, 12V / 5V at the power LEDs; board title, date and `JLCJLCJLCJLC`
  (where JLCPCB prints its order number) in the empty top-right quarter.
- **Front artwork:** flag of Chile, 18 × 12 mm at (188.5–206.5, 88–100),
  with "Designed in Chile / by Gonz" below it. White is solid silkscreen;
  blue is drawn as horizontal lines and red as vertical lines (heraldic
  hatching). Volcano emblem along the top (frame x 142.5–189.5,
  y 48.4–59.8, title and names below it to y 64.5), redrawn on 2026-11-15
  from real terrain: Lonquimay, Llaima, Villarrica and Osorno, north to
  south, from the Copernicus 90 m elevation model, lit from the north-west
  and drawn as a line engraving. Lit snow is solid; the sky is horizontal
  lines, heavier towards the horizon, brighter where Villarrica's plume
  drifts off; the forest below the snow is bare board; a lake in front
  carries ripples. The cones stand closer together than in reality and are
  drawn 3.3 times taller (true scale would be a flat strip at this size).
  Below: "ANDES MADE" in Lato Bold (1.8 mm caps, wide spacing) between two
  rules, and the four names (0.95 mm caps). Every line and gap is at least
  0.16 mm. The frame top clears the pads of Q1 (y 48.0).
- The pin-1 text "1" of the ten LEDs is removed: it sat on the pad of the
  neighbouring capacitor. Check LED orientation against the footprint outline
  and the pads when soldering (+5 V pads face inwards).

Text sizes and positions are tables at the top of `silk.py`. The emblem is
made by `tools/pcb_silk/emblem.py` (its own Python environment: numpy,
scipy, opencv, fonttools; it downloads three elevation tiles once) and
saved as polygons in `tools/pcb_silk/emblem.json`; `silk.py` only draws
that file. To change the emblem: edit the constants in `emblem.py`, run it
with `--preview some.png`, then `python3 tools/pcb_silk/silk.py --emblem`
(KiCad closed), DRC, and export the gerbers again.

## Routing rules

Stored in `blinds-pcb.kicad_pro` (Board Setup → Net Classes / Constraints).

| Net class | Nets | Track | Clearance | Via (pad / drill) |
|---|---|---|---|---|
| Default | signals | 0.25 | 0.2 | 0.6 / 0.3 |
| Power12 | `+12V`, `VIN_JACK`, `VIN_FUSED` | 1.0 | 0.3 | 0.8 / 0.4 |
| Motor | `MOT_A1/A2/B1/B2` | 0.8 | 0.25 | 0.8 / 0.4 |
| Power5 | `+5V`, `5V_MCU`, `SW` | 0.8 | 0.2 | 0.8 / 0.4 |
| Power3V3 | `+3.3V` | 0.5 | 0.2 | 0.6 / 0.3 |
| GND | `GND` (where not in a zone) | 0.5 | 0.2 | 0.6 / 0.3 |

Board minimums: track 0.2, clearance 0.2, via 0.6 / 0.3, annular ring 0.15,
copper to edge 0.5, hole to hole 0.5 (all inside JLCPCB 2-layer limits).

Guidelines the routing follows:

- GND zones on both layers. The front is the main plane, because the parts
  and most tracks are on the back; keep front tracks to the LED ring and
  short jumps.
- Thermal reliefs on GND pads (hand soldering), except the buck's GND pads.
- Wide tracks may neck down to 0.5 mm to pass between 2.54 mm header pads.
- `+12V` runs from the bottom-right up the gap at x ≈ 120.75 (beside the
  NodeMCU pin row, back layer) to the TMC. D5 was moved out of that gap.
- Buck: input caps and the `SW` node as short as possible, GND vias at C1–C3
  and C5–C6, nothing sensitive under L1.
- Motor tracks short and direct between A1 and J2; keep `VSENSE`, `AMBIENT`,
  I2C and knob lines away from them.
- LED ring: +5 V on the inner arc, GND on the outer arc, C21 feeds the ring
  at its D15 end. The NodeMCU gets its 5 V from C21 directly, not from the
  ring.
