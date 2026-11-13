# Centre mechanics: screen carrier and push button

Printed parts that hold the round LCD in the middle of the EC35 ring encoder
and turn "press the screen" into a click on the tact switch (SW4). Started from
the floating-riser idea in `doc/Curtain control panel hardware design.pdf`;
the guide moved from a block behind the PCB to a tower inside the encoder.

![section](out/section.png)

## How it holds together

The riser (plate + stem, carries the display) is the only moving part.

- **Guide:** the fixed tower stands on the PCB inside the encoder bore. Three
  M3×25 screws pass through 17 mm long bores in it, so the riser can only
  move along the axis and cannot turn.
- **Pull-out stop:** those screws go in from the back of the PCB and thread
  into nyloc nuts captured in the riser. Their heads (with washers) rest on
  the back of the PCB. Pulling on the screen pulls steel screws against the
  PCB; the load path has no glued, snapped or heat-set joint in it.
- **Rest position:** turning the three screws moves the riser in or out. Set
  them so the stem just touches the switch: no play, no shims. The nyloc
  nuts keep the setting.
- **Press stop:** after `travel` (0.4 mm) the hub lands on the tower top, so
  a hard press goes into the tower and PCB, not the switch.
- **Return force:** the tact switch only (about 1.6–2.5 N). There is no
  separate spring.

## Parts

| Part | File | What it does | Print |
|---|---|---|---|
| Riser | `out/riser.step` | Ø47 plate above the knob, Ø21.5 hub with three nut pockets, Ø5 stem down to SW4. The display is screwed to the plate. | Plate (display side) on the bed, no supports |
| Tower | `out/tower.step` | Fixed Ø21.5 × 17.1 guide inside the encoder bore: three guide bores, stem bore, switch pocket, wire channel. | PCB face on the bed |
| Bezel | `out/bezel.step` | Ø50.1 cap over the display, Ø33 window. Four ribs sit on the plate, four snap bumps hook under its edge. Cosmetic, removable from the front. | Front face on the bed |
| Knob | `out/knob.step` | Ø50.1 ring, flush with the bezel, Z 1.5–19.6. Hub Ø29.0 on the rotor sleeve, clamped by three radial set screws. The plate above keeps it from coming off. | Top face on the bed |

Hardware:

| Qty | Part | Use |
|---|---|---|
| 3 | M3×25 socket or button head + M3 washer | retention / guide, from the back |
| 3 | M3 nyloc nut (DIN 985, 4 mm high) | captured in the riser |
| 2 | M3×8 + M3 heat-set insert (OD 4.6, L 5.7) | tower to PCB, from the back |
| 4 | M3×6 countersunk | display to plate, from below |
| 3 | M3×6 set screw | knob to rotor (self-tapped in the ribs) |

Material: PETG.

The display keeps its brass standoffs and JST connector. Removing them would
save 6 mm of height but not diameter (the 40 mm long module PCB sets the
Ø50 bezel), and the standoffs give a screw mount plus a 6 mm cavity for the
cable loop.

## Stack (mm above the PCB front face, rest position)

| Level | Z |
|---|---|
| Screw heads behind the PCB | down to −5.1 (−3.8 with button heads) |
| Tact switch actuator / stem tip | 1.5 |
| Tower top (press stop, gap = `travel`) | 17.1 |
| Riser hub underside | 17.5 |
| Knob | 1.5 – 19.6 |
| Encoder rotor top | 20.0 |
| Plate | 21.0 – 23.5 |
| Display PCB back / glass top | 29.5 / 32.5 |
| Bezel top | 33.7 |

Radial: tower and hub Ø21.5 in the Ø23.7 rotating bore (1.1 mm clearance).

## PCB features

Footprint `blinds:Knob_Centre_Mechanics` (reference `MECH1`, board-only),
placed on the knob axis together with SW1 and SW4 at (150, 100):

| Feature | Size | Position from the axis (front view, 0° = right, 90° = up) |
|---|---|---|
| Retention screw holes | 3 × Ø3.4 NPTH | r 7.0 at 90°, 210°, 330° |
| Tower screw holes | 2 × Ø3.2 NPTH | r 7.5 at 30°, 150° |
| Wire slot | 7 × 3 oval NPTH | r 7.25 at 270° |
| SW4 | rotated 90° | centre (pads up/down, inside the tower pocket) |
| Back side | `B.Fab` circles | Ø7 washers and Ø5.5 screw heads |

Layout consequences:

- The knob is Ø50.1 and comes down to 1.5 mm above the PCB. Nothing taller
  than 1 mm inside r 26 (circle on `Cmts.User`); LED ring outside it.
- The tower sits flat on the PCB inside r 10.75: tracks are fine there, parts
  are not. On the back, keep the washer and head circles free of parts.
- The screw heads move 0.4 mm back on a press: leave 1 mm free behind them.
- DRC reports "NPTH inside courtyard" and one courtyard overlap against SW1:
  expected, because the encoder's courtyard is a full rectangle.

## Wires

PH 2.0 mm 8-pin plug at the display (the module's connector is a JST **PH**
side-entry, not XH). The wires leave the plug towards the top, loop round the
right-hand standoffs inside the bezel, go down the channel in the riser hub
and the tower, through the PCB slot, then to J4 on the back. Thread the
crimped wires through before fitting the housing at the J4 end: a housing does
not fit through the 7 × 3 slot.

## Assembly order

1. Solder the PCB (encoder, SW4). Press the two inserts into the tower base
   and screw the tower on from the back (2 × M3×8).
2. Fit the knob on the rotor and nip up the three set screws evenly. It
   cannot go on after the riser.
3. Drop the three nyloc nuts into the riser pockets (nylon ring up). Screw
   the display to the plate (4 × M3×6 from below), plug the cable.
4. Feed the wires down the tower channel and the PCB slot, lower the riser
   so the stem enters the tower.
5. From the back, run the three M3×25 (with washers) into the nuts. Tighten
   each until the switch clicks, then back off until it releases and the
   screen has no play. Keep the three equal.
6. Snap the bezel on.

To remove the screen: undo the three screws at the back.

## Changing the design

`panel_mech.py` is the source. All dimensions are in the `P` block at the top.

    uv venv .venv && uv pip install --python .venv/bin/python cadquery matplotlib
    .venv/bin/python mech/panel_mech.py

It rewrites the STEP files, `out/section.png`, the KiCad footprint, and prints
an interference check. After a change to hole positions, run "Update
footprints from library" on MECH1 in the PCB editor.

For direct edits, import the STEP files in Fusion 360 or FreeCAD (each part
is one solid). `out/panel_mech_assembly.step` (git-ignored, 19 MB) has all
parts in place with the encoder, display, screws and a PCB disc.

## To check on the real parts

- Display module against the vendor STEP: standoff pattern 27 × 12 mm,
  standoff length 6 mm, outline (tab corners at r 23.1).
- Display orientation: drawn with the flex tab down and the JST up. 180° is
  free in firmware; 90° needs `mod_standoffs` swapped.
- `travel` (0.4) and `sw_h` (1.5) against the real switch feel.
- Insert hole Ø4.0 against your inserts; guide bores Ø3.3 after printing
  (ream to a free sliding fit on the screws).
- Nut pockets (5.7 across flats): the nuts should press in and not turn.
- Edge presses: with the 17 mm guide the riser should not jam when pressed
  at the rim. If the return feels weak, a light spring round each screw in a
  counterbore in the tower top is the fix.
- Rotor sleeve: the STEP shows grooves on the Ø28.8 sleeve; check where the
  knob set screws land, and that light clamping does not drag the rotor.
- Snap bumps (`snap_h` 0.35) may need tuning for your printer.
