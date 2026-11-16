# Fabrication files and how to order (rev A)

Made on 2026-11-15 from `blinds-pcb.kicad_pcb` with `kicad-cli` 10.0.8, and
exported again the same day after the pre-order review fixes (J1 slots,
encoder lug keep-outs, NodeMCU 5 V feed; see `doc/pcb-layout.md`) and after
the new volcano emblem (front silkscreen only).
DRC at export time: 0 errors, 0 unconnected, schematic parity clean, ERC 0.

| File | Use |
|---|---|
| `blinds-pcb-revA-gerbers.zip` | Upload this to JLCPCB (9 gerber layers + 2 drill files) |
| `gerbers/` | The same files, unzipped |
| `lcsc-order.csv` | Parts list with LCSC numbers and suggested quantities |
| `print-1to1-front.pdf`, `print-1to1-back.pdf` | Paper fit check: print at 100 % ("actual size") |

Regenerate after any board change (KiCad closed):

    kicad-cli pcb export gerbers -o fab/gerbers/ \
      -l F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts \
      --subtract-soldermask --no-x2 --no-netlist --check-zones blinds-pcb.kicad_pcb
    kicad-cli pcb export drill -o fab/gerbers/ --excellon-units mm \
      --excellon-zeros-format decimal --excellon-oval-format alternate \
      --excellon-separate-th blinds-pcb.kicad_pcb

Then delete `fab/gerbers/*.gbrjob` and zip the folder again. The paper prints:

    kicad-cli pcb export pdf -o fab/print-1to1-front.pdf --mode-single --black-and-white \
      --layers F.Mask,F.Silkscreen,F.Fab,Edge.Cuts blinds-pcb.kicad_pcb
    kicad-cli pcb export pdf -o fab/print-1to1-back.pdf --mode-single --black-and-white \
      --mirror --layers B.Mask,B.Silkscreen,B.Fab,Edge.Cuts blinds-pcb.kicad_pcb

## Before you pay: three checks

1. **Paper fit check.** Print both PDFs at 100 %, measure the outline
   (120.0 mm), and lay the real parts on the paper: the two modules in their
   sockets, the encoder, the DC jack, the RJ45 jack. Checked against the
   drawings in the review: the encoder (ALPS), the DC jack (both DC-005
   types fit the widened slots), and the RJ45 footprint, which is KiCad's
   copy of the **Amphenol 54602** and fits only that jack (the Kinghelm
   KH-RJ45-56 listed before does not).
2. **Routing review in KiCad**, the one you postponed.
3. **JLCPCB's own preview** after the upload (step 2 below).

## Board against JLCPCB's limits

| Item | Board | JLCPCB 2-layer minimum |
|---|---|---|
| Track width / spacing | 0.25 mm / 0.2 mm | 0.127 mm / 0.127 mm |
| Via | 0.6 mm pad, 0.3 mm hole | 0.5 mm / 0.3 mm |
| Smallest plated hole ring | 0.26 mm (NodeMCU sockets) | 0.13 mm |
| Plated slots (DC jack, encoder) | 1.0 mm wide and up | 0.5 mm |
| Unplated slots (wire slot, encoder) | 1.5 mm wide and up | 1.0 mm |
| Copper to board edge | 0.5 mm | 0.3 mm |
| Silkscreen line / text height | 0.15 mm / 0.8 mm | 0.15 mm / 0.8–1.0 mm |

The limits are from memory of JLCPCB's capability page; their upload check
is the authority. The 0.8 mm reference texts are at the lower limit: they
will print, some may be a little soft.

## Ordering the PCB at jlcpcb.com

1. "Order now" → upload `blinds-pcb-revA-gerbers.zip`.
2. Look at the preview, both sides: outline with round corners, four M3 holes,
   the centre holes and slots, flag and volcanoes on the front,
   texts readable (not mirrored) on the back.
3. Options:

| Option | Choose |
|---|---|
| Base material | FR-4 |
| Layers | 2 |
| Dimensions | 120 × 120 mm (filled in from the file) |
| PCB qty | 5 (the minimum) |
| Product type | Industrial/consumer electronics |
| Different design | 1 |
| Delivery format | Single PCB |
| PCB thickness | 1.6 mm |
| PCB colour | Your choice. Green is the cheapest and fastest; the white artwork shows best on black, blue or green |
| Silkscreen | White |
| Surface finish | HASL (with lead) is the cheapest and easiest to hand-solder; lead-free HASL if you prefer |
| Outer copper weight | 1 oz |
| Via covering | Tented |
| Mark on PCB (order number) | "Order number (specify position)": the back has the text `JLCJLCJLCJLC` for it |
| Electrical test | Flying probe, fully tested (default) |
| Gold fingers, castellated holes, edge plating | No |
| PCB assembly, stencil | No (hand soldering) |

Everything else stays at its default. The board is larger than
100 × 100 mm, so the lowest promotional price does not apply.

## Ordering the parts at lcsc.com

LCSC is a separate shop from JLCPCB, with its own cart and shipment.

1. Open `lcsc-order.csv` in a spreadsheet and delete the rows you already
   have. You said you own: TMC2209 module, encoder, WS2812B, NodeMCU,
   display.
2. On lcsc.com use "BOM Tool": upload the file, map the columns "LCSC Part
   Number" and "Suggested order qty", add all to the cart.
3. LCSC raises quantities to each part's minimum and multiple (resistors
   and small capacitors often come in tens or hundreds). Accept that: they
   cost cents.
4. If a part is out of stock, any equivalent with the same value, voltage
   and package works; `doc/bom.md` has the requirements per part.

Not on LCSC list, needed for assembly: the 8-wire JST-PH cable of the
display (check whether it is wired 1:1 or reversed, see `doc/PLAN.md`),
3 × M3×25 screws and nyloc nuts for the centre mechanics, M3 mounting
hardware.

## Open points on parts

- **RJ45 54602-908LF (C2847314):** the LCSC index did not show its stock.
  If LCSC has none, buy it at Digi-Key, RS or element14: no other jack fits
  this footprint. Its contact current (1.5 A or more) is still to confirm
  in the Amphenol datasheet.
- **DC jack:** now DC005-T20 (C111567, 3 A). The old C16214 (1 A) also fits.
- **Side switches C285519** (PTS645VK392LFS): in stock, actuator 3.85 mm
  instead of 3.15 mm. Pin pattern is the PTS645V one used by the
  footprint; confirm in the datasheet.

## Checks on parts you already own (before soldering)

- **NodeMCU:** the metal can must say ESP32-**WROOM**-32 (any suffix). On a
  WROVER module GPIO16/17 (LED data, TMC TX) are used by the PSRAM.
- **TMC2209 module EN:** measure EN to GND with an ohmmeter. If it reads
  about 10k, fit 2.2k for R6 instead of 10k.
- **TEPT4400 (Q1):** pad 1 (square) is the collector. Compare with the part's
  C/E marking before bending the leads; if it is the other way, turn the
  part over (it still looks sideways).
