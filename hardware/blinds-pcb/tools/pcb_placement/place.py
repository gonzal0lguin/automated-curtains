#!/usr/bin/env python3
"""Initial component placement for the panel PCB (no routing).

Run with KiCad's Python (system python3 with pcbnew), KiCad closed:

    python3 tools/pcb_placement/place.py

It draws the outline, adds the mounting holes, places every footprint, adds
the antenna keep-out and fixes the 3D models of the socketed modules. It
overwrites positions, so use it only until placement is edited by hand.

Coordinates: KiCad board mm, front view, Y down. Knob axis at (CX, CY).
"""
import math
import sys
from pathlib import Path

import pcbnew

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "blinds-pcb.kicad_pcb"
KLIB = "/usr/share/kicad/footprints"

CX, CY = 150.0, 100.0
W = H = 120.0
X0, Y0, X1, Y1 = CX - W / 2, CY - H / 2, CX + W / 2, CY + H / 2
R_CORNER = 5.0
LED_R = 31.0          # LED ring radius
LED_A0 = 198.0        # D6 angle, clockwise from 12 o'clock (front view)
CAP_DA = 9.6          # LED cap angular offset (downstream of its LED)
KNOB_R = 26.0         # nothing tall inside this radius on the front
SOCKET_H = 8.5        # height of the 2.54 mm female headers

# Mounting holes (M3). Three in the corners; the bottom-left one sits to the
# right of the NodeMCU, which needs that corner to reach the bottom edge.
HOLES = {"H1": (X0 + 5, Y0 + 5), "H2": (X1 - 5, Y0 + 5),
         "H3": (X1 - 5, Y1 - 5), "H4": (124.5, Y1 - 5)}

# ref: (courtyard centre x, y, rotation) — all on the back side.
BACK = {
    # ESP32 module: USB at the bottom edge, antenna pointing up
    "U2": (105.3, 131.9, 0),
    # motor: RJ45 on the top edge, driver below it
    "J2": (108.4, 49.2, 180), "A1": (104.5, 78.5, 0),
    "C12": (97.5, 63.3, 0), "C11": (105.5, 64.5, 0), "C13": (93.2, 84.8, 90),
    "R5": (115.8, 76.0, 0), "R6": (115.8, 73.0, 0),
    # I2C: spare header on the top edge, pull-ups, RJ45 3V3 cap
    "J3": (126.5, 45.0, 180), "R7": (123.0, 51.5, 0), "R8": (128.0, 51.5, 0),
    "C14": (119.5, 56.5, 90),
    # ambient light sensor looking out of the top edge
    "Q1": (160.0, 44.3, 180), "R14": (160.0, 51.5, 0),
    # bottom edge: BOOT, RESET, DC jack
    "SW3": (134.5, 156.35, 180), "SW2": (144.5, 156.35, 180),
    "J1": (157.0, 152.15, 270),
    # LCD connector and its caps (wires come from the slot under the knob)
    "J4": (138.0, 144.5, 0), "C18": (141.0, 138.5, 0), "C19": (146.2, 138.5, 0),
    # LED level shifter next to the first LED
    "U3": (127.5, 131.0, 0), "R15": (127.5, 134.6, 0), "C20": (127.5, 127.5, 0),
    "R16": (133.0, 131.0, 0),
    # knob A/B pull-ups and filters under the encoder terminals
    "R10": (143.5, 122.5, 0), "R9": (143.5, 125.5, 0), "C15": (143.5, 128.5, 0),
    "R12": (156.5, 122.5, 0), "R11": (156.5, 125.5, 0), "C16": (156.5, 128.5, 0),
    # centre button pull-up and filter
    "R13": (137.0, 107.0, 0), "C17": (137.0, 110.0, 0),
    # NodeMCU decoupling, 12 V sense divider at the ADC pin
    "C10": (114.8, 112.5, 90), "C9": (114.8, 153.0, 90), "C8": (111.0, 153.0, 90),
    "D5": (122.0, 147.5, 90),
    "R3": (124.5, 106.5, 0), "R4": (124.5, 109.5, 0), "C7": (124.5, 112.5, 0),
    # input protection (jack -> fuse -> diode -> TVS)
    "F1": (168.5, 155.5, 0), "D1": (177.5, 155.5, 0), "D2": (188.0, 155.5, 0),
    # 5 V buck
    "U1": (186.0, 146.0, 0), "C3": (191.0, 146.0, 90), "C1": (194.3, 146.0, 90),
    "C2": (197.5, 146.0, 90), "C4": (185.5, 141.5, 0), "L1": (178.5, 146.0, 0),
    "C5": (173.3, 146.0, 90), "C6": (170.8, 146.0, 90),
    "C21": (165.0, 137.5, 0),
    # power LEDs, test pads
    "R1": (190.0, 136.0, 0), "D3": (194.5, 136.0, 0),
    "R2": (190.0, 133.0, 0), "D4": (194.5, 133.0, 0),
    "TP1": (199.5, 139.5, 0), "TP2": (172.0, 140.5, 0), "TP3": (111.0, 116.0, 0),
    "TP4": (182.0, 136.0, 0), "TP5": (163.0, 128.5, 0),
}
LEDS = ["D%d" % n for n in range(6, 16)]
LED_CAPS = ["C%d" % n for n in range(22, 32)]
LOCKED = ("SW1", "SW4", "MECH1")


def mm(v):
    return pcbnew.FromMM(v)


def pt(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


def find(board, ref):
    for fp in board.GetFootprints():
        if fp.GetReference() == ref:
            return fp
    return None


def courtyard_box(fp):
    fp.BuildCourtyardCaches()
    for layer in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
        poly = fp.GetCourtyard(layer)
        if poly.OutlineCount():
            return poly.BBox()
    return fp.GetBoundingBox(False)


def set_side(fp, back):
    if fp.IsFlipped() != back:
        fp.SetLayerAndFlip(pcbnew.B_Cu if back else pcbnew.F_Cu)


def put(board, ref, cx, cy, rot, back=True):
    """Place so that the courtyard centre lands on (cx, cy).

    KiCad flips top-to-bottom; the extra 180 degrees for back-side parts makes
    `rot` mean "as seen in the footprint editor, mirrored left-right".
    """
    fp = find(board, ref)
    if fp is None:
        sys.exit("missing footprint " + ref)
    set_side(fp, back)
    fp.SetOrientationDegrees(rot + 180 if back else rot)
    c = courtyard_box(fp).GetCenter()
    fp.Move(pcbnew.VECTOR2I(mm(cx) - c.x, mm(cy) - c.y))
    return fp


def polar(r, a_cw):
    a = math.radians(a_cw)
    return CX + r * math.sin(a), CY - r * math.cos(a)


def draw_outline(board):
    for d in [d for d in board.GetDrawings() if d.GetLayer() == pcbnew.Edge_Cuts]:
        board.Delete(d)
    r = R_CORNER

    def seg(a, b):
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pt(*a)); s.SetEnd(pt(*b))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1)); board.Add(s)

    def arc(cx, cy, a0):
        # quarter circle, a0 = start angle (deg, screen, clockwise from +X)
        p = [(cx + r * math.cos(math.radians(a0 + t)),
              cy + r * math.sin(math.radians(a0 + t))) for t in (0, 45, 90)]
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_ARC)
        s.SetArcGeometry(pt(*p[0]), pt(*p[1]), pt(*p[2]))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1)); board.Add(s)

    seg((X0 + r, Y0), (X1 - r, Y0)); seg((X1, Y0 + r), (X1, Y1 - r))
    seg((X1 - r, Y1), (X0 + r, Y1)); seg((X0, Y1 - r), (X0, Y0 + r))
    arc(X1 - r, Y0 + r, 270); arc(X1 - r, Y1 - r, 0)
    arc(X0 + r, Y1 - r, 90); arc(X0 + r, Y0 + r, 180)


def add_holes(board):
    for ref, (x, y) in HOLES.items():
        fp = find(board, ref)
        if fp is None:
            fp = pcbnew.FootprintLoad(KLIB + "/MountingHole.pretty",
                                      "MountingHole_3.2mm_M3")
            fp.SetReference(ref)
            fp.SetValue("M3")
            fp.SetAttributes(pcbnew.FP_BOARD_ONLY | pcbnew.FP_EXCLUDE_FROM_BOM
                             | pcbnew.FP_EXCLUDE_FROM_POS_FILES)
            fp.Reference().SetVisible(False)
            board.Add(fp)
        fp.SetPosition(pt(x, y))
        fp.SetLocked(True)


def place_ring(board):
    for i, (led, cap) in enumerate(zip(LEDS, LED_CAPS)):
        a = LED_A0 + 36.0 * i
        fp = find(board, led)
        set_side(fp, False)
        fp.SetPosition(pt(*polar(LED_R, a)))
        # local -X (DOUT side) points clockwise to the next LED,
        # local +Y (DOUT, GND) points outwards
        fp.SetOrientationDegrees(180.0 - a)
        ac = a + CAP_DA
        fp = find(board, cap)
        set_side(fp, False)
        fp.SetPosition(pt(*polar(LED_R, ac)))
        fp.SetOrientationDegrees(90.0 - ac)   # pad 1 (+5V) inwards


def add_antenna_keepout(board):
    name = "ESP32 antenna keep-out"
    for z in [z for z in board.Zones() if z.GetZoneName() == name]:
        board.Delete(z)
    z = pcbnew.ZONE(board)
    z.SetZoneName(name)
    z.SetIsRuleArea(True)
    ls = pcbnew.LSET()
    ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.B_Cu)
    z.SetLayerSet(ls)
    z.SetDoNotAllowTracks(True)
    z.SetDoNotAllowVias(True)
    z.SetDoNotAllowZoneFills(True)
    z.SetDoNotAllowPads(False)
    z.SetDoNotAllowFootprints(False)
    o = z.Outline()
    o.NewOutline()
    for x, y in ((94.5, 98.0), (115.8, 98.0), (115.8, 110.3), (94.5, 110.3)):
        o.Append(mm(x), mm(y))
    board.Add(z)


def set_models(fp, models):
    """models: list of (path, (x, y, z) offset mm, (rx, ry, rz) deg)."""
    fp.Models().clear()
    for path, off, rot in models:
        m = pcbnew.FP_3DMODEL()
        m.m_Filename = path
        m.m_Offset = pcbnew.VECTOR3D(*off)
        m.m_Rotation = pcbnew.VECTOR3D(*rot)
        m.m_Scale = pcbnew.VECTOR3D(1, 1, 1)
        fp.Models().append(m)


SOCK = "${KICAD10_3DMODEL_DIR}/Connector_PinSocket_2.54mm.3dshapes/PinSocket_1x%02d_P2.54mm_Vertical.step"
U2_MODELS = [
    (SOCK % 19, (12.7, 22.86, 0), (0, 0, 0)),
    (SOCK % 19, (-12.7, 22.86, 0), (0, 0, 0)),
    ("${KIPRJMOD}/lib/3d/ESP32_NODEMCU.step", (0, 0, SOCKET_H), (-90, 0, 0)),
]
A1_MODELS = [
    (SOCK % 8, (0, 0, 0), (0, 0, 0)),
    (SOCK % 8, (12.7, 0, 0), (0, 0, 0)),
    ("${KIPRJMOD}/lib/3d/TMC2209_BTT_V1.3.step", (0, 0, SOCKET_H), (0, 0, 0)),
]


EXTRA_MODELS = {   # KiCad ships no model for these footprints
    "J2": [("${KIPRJMOD}/lib/3d/RJ45_8P8C_block.step", (0, 0, 0), (0, 0, 0))],
    "F1": [("${KIPRJMOD}/lib/3d/Fuse_1812_block.step", (0, 0, 0), (0, 0, 0))],
    "SW4": [("${KIPRJMOD}/lib/3d/TS-1187A_block.step", (0, 0, 0), (0, 0, 0))],
}


def check(board):
    """Things DRC does not see: solder joints of back-side THT parts poking
    through under the knob or into a front-side part."""
    problems = []
    front = []
    for fp in board.GetFootprints():
        if not fp.IsFlipped() and fp.GetReference() not in LOCKED:
            fp.BuildCourtyardCaches()
            poly = fp.GetCourtyard(pcbnew.F_CrtYd)
            if poly.OutlineCount():
                front.append((fp.GetReference(), poly))
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        if ref in LOCKED or ref in HOLES:
            continue
        box = courtyard_box(fp)
        if (box.GetLeft() < mm(X0 - 0.3) or box.GetRight() > mm(X1 + 0.3)
                or box.GetTop() < mm(Y0 - 0.3) or box.GetBottom() > mm(Y1 + 0.3)):
            problems.append("%s courtyard outside the board" % ref)
        for pad in fp.Pads():
            if not pad.HasHole():
                continue
            p = pad.GetPosition()
            rad = max(pad.GetSize().x, pad.GetSize().y, pad.GetDrillSize().x) / 2
            d = math.hypot(pcbnew.ToMM(p.x) - CX, pcbnew.ToMM(p.y) - CY)
            if fp.IsFlipped() and d - pcbnew.ToMM(rad) < KNOB_R:
                problems.append("%s pad %s at r %.1f is under the knob"
                                % (ref, pad.GetNumber(), d))
            for fref, poly in front:
                if fref != ref and poly.Collide(p, int(rad + mm(0.3))):
                    problems.append("%s pad %s pokes into %s"
                                    % (ref, pad.GetNumber(), fref))
    return problems


def main():
    board = pcbnew.LoadBoard(str(BOARD))
    draw_outline(board)
    add_holes(board)
    place_ring(board)
    for ref, (x, y, rot) in BACK.items():
        put(board, ref, x, y, rot, back=True)
    add_antenna_keepout(board)
    set_models(find(board, "U2"), U2_MODELS)
    set_models(find(board, "A1"), A1_MODELS)
    for ref, models in EXTRA_MODELS.items():
        set_models(find(board, ref), models)

    placed = set(BACK) | set(LEDS) | set(LED_CAPS) | set(LOCKED) | set(HOLES)
    left = [f.GetReference() for f in board.GetFootprints()
            if f.GetReference() not in placed]
    if left:
        print("NOT PLACED:", left)
    for p in check(board):
        print("CHECK:", p)
    board.Save(str(BOARD))
    print("saved", BOARD.name, "-", len(board.GetFootprints()), "footprints")


if __name__ == "__main__":
    main()
