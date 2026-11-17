#!/usr/bin/env python3
"""Builds motor-pcb.kicad_pcb from the schematic: footprints, outline, holes,
placement, routing and GND zones.

Run with KiCad's Python (system python3 with pcbnew), KiCad closed:

    python3 tools/gen_schematic.py     # only if the schematic changed
    python3 tools/build_pcb.py
    python3 tools/silk.py

WARNING: it rebuilds the whole board, so it overwrites any hand edits.

Sides: F (front) = outer face, away from the motor. B (back) = motor side,
facing the magnet: only U1, C1, C2 and JP1 sit there.
Coordinates: KiCad board mm, seen from the outer face, Y down. The shaft axis
is at (CX, CY).
"""
import math
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import pcbnew

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / "blinds-pcb" / "tools" / "gen_schematic"))
from sch import parse, child, children  # noqa: E402

SCH = ROOT / "motor-pcb.kicad_sch"
BOARD = ROOT / "motor-pcb.kicad_pcb"
KLIB = "/usr/share/kicad/footprints"

CX, CY = 150.0, 100.0
SIZE = 42.3                    # NEMA 17 body
X0, Y0, X1, Y1 = CX - SIZE / 2, CY - SIZE / 2, CX + SIZE / 2, CY + SIZE / 2
R_CORNER = 3.0
HOLE_D = 15.5                  # half of the 31 mm motor hole pattern
HOLES = {"H1": (CX - HOLE_D, CY - HOLE_D), "H2": (CX + HOLE_D, CY - HOLE_D),
         "H3": (CX + HOLE_D, CY + HOLE_D), "H4": (CX - HOLE_D, CY + HOLE_D)}
SCREW_KEEP = 3.6               # radius kept free of copper round the holes (washer / spacer)

# ref: (x, y, rotation, side). Position = footprint origin.
# Rotation as KiCad shows it in the board editor.
PLACE = {
    # motor side: sensor exactly on the shaft axis, its caps and DIR jumper
    "U1": (CX, CY, 0, "B"),
    "C1": (145.3, 100.0, 90, "B"),
    "C2": (142.9, 100.0, 90, "B"),
    "JP1": (156.6, 101.905, 270, "B"),
    # outer face: RJ45 mouth on the bottom edge, motor header on the left
    "J1": (145.555, 106.93, 0, "F"),
    "J2": (133.0, 100.0, 90, "F"),
    "R1": (148.0, 98.6, 270, "F"),
    "D2": (151.6, 98.4, 90, "F"),
    "R2": (155.0, 98.6, 270, "F"),
    "C3": (157.4, 98.6, 270, "F"),
    "R3": (161.5, 99.0, 270, "F"),
    "D1": (161.5, 103.0, 90, "F"),
    "TP1": (154.5, 93.0, 0, "F"),
    "TP2": (158.0, 93.0, 0, "F"),
    "TP3": (148.9, 92.0, 0, "F"),
}


def mm(v):
    return pcbnew.FromMM(v)


def pt(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


# ------------------------------------------------------------------ netlist

def netlist():
    out = os.path.join(tempfile.mkdtemp(), "motor.net")
    subprocess.run(["kicad-cli", "sch", "export", "netlist", "--format", "kicadsexpr",
                    "-o", out, str(SCH)], check=True, capture_output=True)
    tree = parse(open(out).read())
    comps = {}
    for c in children(child(tree, "components"), "comp"):
        ref = str(child(c, "ref")[1])
        fields = {}
        for f in children(child(c, "fields"), "field") if child(c, "fields") else []:
            vals = [v for v in f[2:] if not isinstance(v, list)]
            fields[str(child(f, "name")[1])] = str(vals[0]) if vals else ""
        comps[ref] = {
            "value": str(child(c, "value")[1]),
            "footprint": str(child(c, "footprint")[1]),
            "path": str(child(c, "sheetpath")[2][1]) + str(child(c, "tstamps")[1]),
            "fields": fields,
        }
    pins = {}
    for n in children(child(tree, "nets"), "net"):
        name = str(child(n, "name")[1])
        for node in children(n, "node"):
            pins[(str(child(node, "ref")[1]), str(child(node, "pin")[1]))] = name
    return comps, pins


def field_value(f):
    return f


# ------------------------------------------------------------------ board

def new_board():
    board = pcbnew.LoadBoard(str(BOARD)) if BOARD.exists() else pcbnew.CreateEmptyBoard()
    for item in list(board.GetFootprints()) + list(board.GetTracks()) + list(board.Zones()) \
            + list(board.GetDrawings()):
        board.Delete(item)
    return board


def add_footprints(board, comps, pins):
    nets = {}
    for name in sorted(set(pins.values())):
        ni = pcbnew.NETINFO_ITEM(board, name)
        board.Add(ni)
        nets[name] = ni
    for ref, c in sorted(comps.items()):
        lib, name = c["footprint"].split(":")
        fp = pcbnew.FootprintLoad(f"{KLIB}/{lib}.pretty", name)
        assert fp, c["footprint"]
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        fp.SetReference(ref)
        fp.SetValue(c["value"])
        fp.SetPath(pcbnew.KIID_PATH(c["path"]))
        for k, v in c["fields"].items():
            if k in ("Reference", "Value", "Footprint"):
                continue
            fp.SetField(k, v)
            fp.GetField(k).SetVisible(False)
        fp.SetExcludedFromBOM(False)        # the schematic decides (all in BOM)
        for pad in fp.Pads():
            n = pins.get((ref, pad.GetNumber()))
            if n:
                pad.SetNet(nets[n])
        board.Add(fp)
    return nets


def find(board, ref):
    for fp in board.GetFootprints():
        if fp.GetReference() == ref:
            return fp
    raise KeyError(ref)


def place(board):
    for ref, (x, y, rot, side) in PLACE.items():
        fp = find(board, ref)
        if side == "B" and not fp.IsFlipped():
            fp.SetLayerAndFlip(pcbnew.B_Cu)
        fp.SetOrientationDegrees(rot)
        fp.SetPosition(pt(x, y))
    for ref, (x, y) in HOLES.items():
        fp = pcbnew.FootprintLoad(KLIB + "/MountingHole.pretty", "MountingHole_3.2mm_M3")
        fp.SetReference(ref)
        fp.SetValue("M3")
        fp.SetAttributes(pcbnew.FP_BOARD_ONLY | pcbnew.FP_EXCLUDE_FROM_BOM
                         | pcbnew.FP_EXCLUDE_FROM_POS_FILES)
        fp.Reference().SetVisible(False)
        fp.SetPosition(pt(x, y))
        fp.SetLocked(True)
        board.Add(fp)
    fp = find(board, "U1")
    fp.SetLocked(True)                 # on the shaft axis: do not move
    m = pcbnew.FP_3DMODEL()
    m.m_Filename = "${KIPRJMOD}/lib/3d/RJ45_8P8C_block.step"
    find(board, "J1").Models().clear()
    find(board, "J1").Models().append(m)


def draw_outline(board):
    r = R_CORNER

    def add(s):
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(mm(0.1))
        board.Add(s)

    for a, b in (((X0 + r, Y0), (X1 - r, Y0)), ((X1, Y0 + r), (X1, Y1 - r)),
                 ((X1 - r, Y1), (X0 + r, Y1)), ((X0, Y1 - r), (X0, Y0 + r))):
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pt(*a)); s.SetEnd(pt(*b)); add(s)
    for cx, cy, a0 in ((X1 - r, Y0 + r, 270), (X1 - r, Y1 - r, 0),
                       (X0 + r, Y1 - r, 90), (X0 + r, Y0 + r, 180)):
        p = [(cx + r * math.cos(math.radians(a0 + t)), cy + r * math.sin(math.radians(a0 + t)))
             for t in (0, 45, 90)]
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_ARC)
        s.SetArcGeometry(pt(*p[0]), pt(*p[1]), pt(*p[2])); add(s)


def keepouts(board):
    """No copper round the screws: washers, spacers and the motor body."""
    for ref, (x, y) in HOLES.items():
        z = pcbnew.ZONE(board)
        z.SetZoneName("screw keep-out " + ref)
        z.SetIsRuleArea(True)
        ls = pcbnew.LSET(); ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.B_Cu)
        z.SetLayerSet(ls)
        z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True)
        z.SetDoNotAllowZoneFills(True); z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
        o = z.Outline(); o.NewOutline()
        for i in range(24):
            a = 2 * math.pi * i / 24
            o.Append(mm(x + SCREW_KEEP * math.cos(a)), mm(y + SCREW_KEEP * math.sin(a)))
        board.Add(z)


def dump_pads(board):
    for fp in sorted(board.GetFootprints(), key=lambda f: f.GetReference()):
        for p in fp.Pads():
            q = p.GetPosition()
            print("%-4s %-2s %-9s %7.3f %7.3f  %s" % (fp.GetReference(), p.GetNumber(),
                  p.GetNetname()[:9], pcbnew.ToMM(q.x), pcbnew.ToMM(q.y),
                  "F" if not fp.IsFlipped() else "B"))


# ------------------------------------------------------------------ routing
F, B = pcbnew.F_Cu, pcbnew.B_Cu
# (net, layer, width, [points]). Phases: one coil pair per route corridor,
# all of them at least 7 mm from U1. A1/A2 and B2 on the outer face, B1 on
# the motor side under the RJ45.
TRACKS = [
    ("/MOT_A1", F, 0.8, [(145.555, 106.93), (133.0, 106.93), (133.0, 100.0)]),
    ("/MOT_A2", F, 0.8, [(146.825, 104.39), (136.5, 104.39), (136.5, 97.5), (133.0, 97.5)]),
    ("/MOT_B2", F, 0.6, [(151.905, 104.39), (151.905, 103.0), (139.5, 103.0), (139.5, 92.5),
                         (133.0, 92.5)]),
    ("/MOT_B1", B, 0.8, [(148.095, 106.93), (148.095, 109.5), (137.0, 109.5), (137.0, 95.0),
                         (133.0, 95.0)]),
    # I2C: SDA under the chip body on the motor side, SCL between RJ45 pins 6 and 8
    ("/SDA", B, 0.25, [(149.365, 104.39), (149.365, 103.0), (150.0, 102.365), (150.0, 99.365),
                       (152.475, 99.365)]),
    ("/SDA", F, 0.25, [(148.0, 99.6), (148.0, 100.3), (148.7, 101.0), (150.0, 101.0),
                       (150.65, 100.35), (150.65, 99.338)]),
    ("/SCL", F, 0.25, [(153.175, 106.93), (153.175, 100.2), (152.55, 99.575), (152.55, 99.338)]),
    ("/SCL", F, 0.25, [(153.175, 100.2), (154.4, 100.2), (155.0, 99.6)]),
    ("/SCL", B, 0.25, [(152.475, 100.635), (153.75, 100.635), (154.2, 100.2)]),
    ("/DIR", B, 0.25, [(152.475, 101.905), (156.6, 101.905)]),
    ("/ENC_OUT", B, 0.25, [(147.525, 99.365), (148.9, 99.365), (148.9, 94.4)]),
    ("/ENC_OUT", F, 0.25, [(148.9, 94.4), (148.9, 92.0)]),
    # 3V3: RJ45 pin 8 -> ring round the pull-ups -> via to the chip side
    ("+3.3V", F, 0.5, [(154.445, 104.39), (155.5, 103.3), (159.0, 103.3), (159.0, 96.2),
                       (146.0, 96.2), (146.0, 101.9)]),
    ("+3.3V", F, 0.4, [(148.0, 97.6), (148.0, 96.2)]),
    ("+3.3V", F, 0.4, [(155.0, 97.6), (155.0, 96.2)]),
    ("+3.3V", F, 0.4, [(157.4, 97.562), (157.4, 96.2)]),
    ("+3.3V", F, 0.4, [(154.5, 93.0), (154.5, 96.2)]),
    ("+3.3V", F, 0.4, [(161.5, 98.0), (159.0, 98.0)]),
    ("+3.3V", B, 0.4, [(146.0, 101.9), (147.525, 101.905)]),
    ("+3.3V", B, 0.4, [(146.0, 101.9), (145.3, 101.9), (145.3, 101.037)]),
    ("+3.3V", B, 0.4, [(147.525, 100.635), (146.2, 100.635), (145.3, 101.037)]),
    ("+3.3V", B, 0.4, [(145.3, 101.037), (142.9, 101.037)]),
    ("+3.3V", B, 0.4, [(158.0, 103.3), (156.6, 103.205)]),
    ("Net-(D1-A)", F, 0.25, [(161.5, 100.0), (161.5, 101.975)]),
    # GND: chip and caps on the motor side, ESD diode on the outer face
    ("GND", B, 0.4, [(147.525, 98.095), (146.2, 98.095), (145.3, 98.963)]),
    ("GND", B, 0.4, [(145.3, 98.963), (142.9, 98.963), (141.6, 98.963)]),
    ("GND", F, 0.4, [(151.6, 97.463), (150.5, 97.4)]),
    ("GND", F, 0.4, [(157.4, 99.638), (158.2, 100.6)]),
]
VIAS = [("/SDA", 150.0, 101.0), ("/SCL", 154.2, 100.2), ("/ENC_OUT", 148.9, 94.4),
        ("+3.3V", 146.0, 101.9), ("+3.3V", 158.0, 103.3),
        ("GND", 141.6, 98.963), ("GND", 150.5, 97.4), ("GND", 158.2, 100.6),
        # stitching between the two GND planes
        ("GND", 142.0, 90.0), ("GND", 160.5, 94.5), ("GND", 160.0, 111.5),
        ("GND", 142.0, 112.0), ("GND", 159.5, 110.0), ("GND", 130.8, 108.0),
        ("GND", 168.5, 100.0), ("GND", 131.5, 89.0)]


def route(board, nets):
    for net, layer, w, pts in TRACKS:
        for a, b in zip(pts, pts[1:]):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(pt(*a)); t.SetEnd(pt(*b))
            t.SetWidth(mm(w)); t.SetLayer(layer); t.SetNet(nets[net])
            board.Add(t)
    for net, x, y in VIAS:
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(pt(x, y)); v.SetDrill(mm(0.3)); v.SetWidth(mm(0.6))
        v.SetNet(nets[net])
        board.Add(v)


def gnd_zones(board, nets):
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(nets["GND"])
        z.SetLocalClearance(mm(0.3))
        z.SetMinThickness(mm(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetThermalReliefGap(mm(0.3))
        z.SetThermalReliefSpokeWidth(mm(0.4))
        o = z.Outline(); o.NewOutline()
        for x, y in ((X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)):
            o.Append(mm(x), mm(y))
        board.Add(z)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


def main():
    comps, pins = netlist()
    board = new_board()
    nets = add_footprints(board, comps, pins)
    draw_outline(board)
    place(board)
    keepouts(board)
    if "--pads" in sys.argv:
        dump_pads(board)
    route(board, nets)
    gnd_zones(board, nets)
    board.Save(str(BOARD))
    print("saved", BOARD.name, len(board.GetFootprints()), "footprints,",
          len(board.GetTracks()), "tracks/vias")


if __name__ == "__main__":
    main()
