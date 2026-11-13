# Smart Blinds — control panel PCB

KiCad 10 project for the wall control panel of the motorized blind: a round
board that carries the ESP32, the stepper driver, the ring knob, the LED ring
and the connectors for the round display and the motor.

**Status: schematic complete** (ERC clean, [`doc/schematic.pdf`](doc/schematic.pdf)).
PCB layout is the next phase; the board file still holds an old, partial placement.

## What is on the board

| Block | Part |
|---|---|
| Controller | ESP32 NodeMCU 38-pin, in sockets |
| Motor driver | BigTreeTech TMC2209 module, in sockets |
| Knob | ALPS EC35A1520402 ring encoder + centre tact switch |
| Lights | 10 × WS2812B with a 5 V level shifter |
| Power | 12 V barrel jack, fuse, reverse-polarity and surge protection, 5 V 2 A buck (AP63205) |
| Display | Connector for a GC9A01 1.28" round LCD module |
| Motor link | RJ45 to the NEMA 17 and its AS5600 encoder (**not Ethernet**) |
| Extras | Ambient light sensor, 12 V sense, RESET/BOOT buttons, spare I2C header |

## Documents

| File | Contents |
|---|---|
| [`doc/hardware-design.md`](doc/hardware-design.md) | Every circuit block explained: what it does, why each value, what to check |
| [`doc/pinmap.md`](doc/pinmap.md) | ESP32 pin map, connector pinouts, firmware edits for the PCB |
| [`doc/bom.md`](doc/bom.md) | Parts list with packages and LCSC numbers |
| [`doc/schematic.pdf`](doc/schematic.pdf) | Printable schematic |
| [`doc/PLAN.md`](doc/PLAN.md) | Working plan, decisions and log |
| [`tools/gen_schematic/`](tools/gen_schematic/) | Script that generated the schematic and checks its netlist. **Re-running it overwrites edits made in KiCad** |
| [`doc/Curtain control panel hardware design.pdf`](doc/Curtain%20control%20panel%20hardware%20design.pdf) | Earlier design study, including the press-the-screen mechanism |
| [`../../firmware/README.md`](../../firmware/README.md) | Firmware: build, flash, test firmwares, API |

## Opening the project

Open `blinds-pcb.kicad_pro` with KiCad 10 or newer. Project-specific symbols,
footprints and 3D models are in `lib/` (`blinds.kicad_sym`, `blinds.pretty/`,
`3d/`) and are found through `${KIPRJMOD}`, so no global library setup is needed.

## Things to know before powering it

- **Supply:** 12 V DC, 2 A, centre-positive, 5.5 mm barrel plug.
- **The RJ45 port carries 12 V motor current.** Never connect it to a
  computer, router or switch, and never unplug it while the board is powered.
- **Motor current** is set by the firmware over UART; keep it at or below
  about 1 A RMS because it flows through the RJ45 cable.
- **USB and 12 V together are fine:** a diode stops USB from powering the
  LED ring or back-feeding the regulator. The NodeMCU itself has no diode on
  its USB 5 V, so do not leave it plugged into a computer that is switched
  off while the 12 V adapter is on.
- **Flashing:** plug USB into the NodeMCU while it sits in the board. If
  automatic download mode fails, hold BOOT, tap RESET, release BOOT.
- **Firmware pins differ from the breadboard prototype.** Apply the edits
  listed in [`doc/pinmap.md`](doc/pinmap.md) before flashing a PCB build.

Assembly order and the bring-up checklist are at the end of
[`doc/hardware-design.md`](doc/hardware-design.md).
