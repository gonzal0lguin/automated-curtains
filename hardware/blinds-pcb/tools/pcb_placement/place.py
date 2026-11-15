#!/usr/bin/env python3
"""Initial component placement for the panel PCB (no routing).

Run with KiCad's Python (system python3 with pcbnew), KiCad closed:

    python3 tools/pcb_placement/place.py

It draws the outline, adds the mounting holes, places every footprint, adds
the antenna keep-out and fixes the 3D models of the socketed modules. It
overwrites positions, so use it only until placement is edited by hand.
It does not move tracks or texts: run tools/pcb_routing/route.py and then
tools/pcb_silk/silk.py afterwards.

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
SOCKETED = ("U2", "A1")
ENC_BORE_R = 11.85    # clear bore of the EC35A rotor

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
    "C12": (97.5, 63.3, 0),
    # 100 nF across VM / GND (pins 16, 15), outside the module; VIO cap inside
    "C11": (95.5, 70.9, 90), "C13": (101.5, 84.8, 180),
    "R5": (115.8, 76.0, 0), "R6": (115.8, 73.0, 0),
    # I2C: spare header on the top edge, pull-ups, RJ45 3V3 cap
    "J3": (126.5, 45.0, 180), "R7": (123.0, 51.5, 0), "R8": (128.0, 51.5, 0),
    "C14": (119.5, 56.5, 90),
    # ambient light sensor looking out of the top edge
    "Q1": (160.0, 44.3, 180), "R14": (160.0, 51.5, 0),
    # bottom edge: BOOT, RESET, DC jack
    "SW3": (134.5, 156.35, 180), "SW2": (144.5, 156.35, 180),
    "J1": (157.0, 152.15, 270),
    # LCD connector (SMD side-entry, mouth facing the wire slot 18 mm above)
    # and its caps
    "J4": (150.0, 129.85, 180), "C18": (149.5, 137.5, 0), "C19": (155.0, 137.5, 0),
    # LED level shifter next to the first LED
    "U3": (127.5, 131.0, 0), "R15": (127.5, 134.6, 0), "C20": (127.5, 127.5, 0),
    "R16": (133.0, 131.0, 0),
    # knob A/B pull-ups and filters either side of the encoder terminals
    "R10": (140.5, 115.0, 0), "R9": (140.5, 118.0, 0), "C15": (140.5, 121.0, 0),
    "R12": (159.5, 115.0, 0), "R11": (159.5, 118.0, 0), "C16": (159.5, 121.0, 0),
    # centre button pull-up and filter
    "R13": (137.0, 107.0, 0), "C17": (137.0, 110.0, 0),
    # NodeMCU decoupling, 12 V sense divider at the ADC pin
    "C10": (114.8, 112.5, 90), "C9": (114.8, 153.0, 90), "C8": (111.0, 153.0, 90),
    # 5 V feed diode under the module (keeps the x = 120.75 gap free for +12V)
    "D5": (112.8, 148.4, 180),
    "R3": (124.5, 106.5, 0), "R4": (124.5, 109.5, 0), "C7": (124.5, 112.5, 0),
    # input protection (jack -> fuse -> diode -> TVS), pads in current order
    "F1": (168.5, 155.5, 180), "D1": (177.5, 155.5, 0), "D2": (188.0, 155.5, 180),
    # 5 V buck: C3 straight across VIN / GND (pins 3, 4), bulk caps with
    # +12V at the bottom, where it arrives from D2
    "U1": (186.0, 146.0, 0), "C3": (186.0, 148.9, 0), "C1": (191.5, 146.5, 270),
    "C2": (195.0, 146.5, 270), "C4": (185.5, 141.5, 0), "L1": (178.5, 146.0, 0),
    "C5": (173.3, 146.0, 90), "C6": (170.8, 146.0, 90),
    "C21": (165.0, 137.5, 0),
    # power LEDs, test pads
    "R1": (190.0, 136.0, 0), "D3": (194.5, 136.0, 0),
    "R2": (190.0, 133.0, 0), "D4": (194.5, 133.0, 0),
    "TP1": (199.5, 139.5, 0), "TP2": (172.0, 140.5, 0), "TP3": (111.0, 116.0, 0),
    "TP4": (182.0, 136.0, 0), "TP5": (164.5, 128.5, 0),
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
    if ref in SOCKETED:
        # their courtyard is redrawn by fix_courtyards(): use the pin field
        xs = [p.GetPosition().x for p in fp.Pads() if p.GetNumber()]
        ys = [p.GetPosition().y for p in fp.Pads() if p.GetNumber()]
        c = pcbnew.VECTOR2I((min(xs) + max(xs)) // 2, (min(ys) + max(ys)) // 2)
    else:
        c = courtyard_box(fp).GetCenter()
    fp.Move(pcbnew.VECTOR2I(mm(cx) - c.x, mm(cy) - c.y))
    return fp


# Footprints that differ from what the board was first synced with:
# ref: (library, footprint). Same pad numbers, so the nets carry over.
SWAP = {
    "J4": ("Connector_JST", "JST_PH_S8B-PH-SM4-TB_1x08-1MP_P2.00mm_Horizontal"),
}


def swap_footprints(board):
    for ref, (lib, name) in SWAP.items():
        old = find(board, ref)
        if str(old.GetFPID().GetLibItemName()) == name:
            continue
        new = pcbnew.FootprintLoad("%s/%s.pretty" % (KLIB, lib), name)
        new.SetFPID(pcbnew.LIB_ID(lib, name))
        new.SetReference(ref)
        new.SetValue(old.GetValue())
        new.SetPath(old.GetPath())
        new.SetSheetname(old.GetSheetname())
        new.SetSheetfile(old.GetSheetfile())
        new.SetPosition(old.GetPosition())
        for f in old.GetFields():      # carry the schematic fields over
            name = f.GetName()
            if name in ("Reference", "Value", "Footprint"):
                continue
            new.SetField(name, "" if name == "LCSC" else f.GetText())
            new.GetField(name).SetVisible(False)
        nets = {p.GetNumber(): p.GetNet() for p in old.Pads()}
        for pad in new.Pads():
            if pad.GetNumber() in nets:
                pad.SetNet(nets[pad.GetNumber()])
        board.Delete(old)
        board.Add(new)
        print("swapped footprint of", ref)


def set_courtyard(fp, shapes):
    """Replace the courtyard of a board footprint. shapes: ("rect", x0, y0,
    x1, y1) or ("circle", cx, cy, r) in board mm."""
    back = fp.IsFlipped()
    layer = pcbnew.B_CrtYd if back else pcbnew.F_CrtYd
    for g in list(fp.GraphicalItems()):
        if g.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
            fp.Remove(g)
    for kind, *v in shapes:
        s = pcbnew.PCB_SHAPE(fp, pcbnew.SHAPE_T_RECT if kind == "rect"
                             else pcbnew.SHAPE_T_CIRCLE)
        if kind == "rect":
            s.SetStart(pt(v[0], v[1]))
            s.SetEnd(pt(v[2], v[3]))
        else:
            s.SetCenter(pt(v[0], v[1]))
            s.SetEnd(pt(v[0] + v[2], v[1]))
        s.SetLayer(layer)
        s.SetWidth(mm(0.05))
        fp.Add(s)
    fp.BuildCourtyardCaches()


def fix_courtyards(board):
    """Courtyards that describe what really touches the PCB.

    U2 and A1 sit in 8.5 mm sockets, so only the two socket strips occupy the
    board and small parts may sit under the module. The encoder is hollow:
    its bore (r 11.85) holds SW4 and the MECH1 holes.
    """
    for ref in SOCKETED:
        fp = find(board, ref)
        cols = {}
        for p in fp.Pads():
            if p.GetNumber():
                x, y = pcbnew.ToMM(p.GetPosition().x), pcbnew.ToMM(p.GetPosition().y)
                cols.setdefault(round(x, 1), []).append((x, y))
        assert len(cols) == 2, ref
        set_courtyard(fp, [("rect", c[0][0] - 1.4, min(y for _, y in c) - 1.52,
                            c[0][0] + 1.4, max(y for _, y in c) + 1.52)
                           for c in cols.values()])
    fp = find(board, "SW1")                 # as in lib/blinds.pretty
    if not any(g.GetLayer() == pcbnew.F_CrtYd and g.GetShape() == pcbnew.SHAPE_T_CIRCLE
               for g in fp.GraphicalItems()):
        s = pcbnew.PCB_SHAPE(fp, pcbnew.SHAPE_T_CIRCLE)
        s.SetCenter(pt(CX, CY))
        s.SetEnd(pt(CX + ENC_BORE_R - 0.25, CY))
        s.SetLayer(pcbnew.F_CrtYd)
        s.SetWidth(mm(0.05))
        fp.Add(s)
        fp.BuildCourtyardCaches()


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
    swap_footprints(board)
    draw_outline(board)
    add_holes(board)
    place_ring(board)
    for ref, (x, y, rot) in BACK.items():
        put(board, ref, x, y, rot, back=True)
    add_antenna_keepout(board)
    fix_courtyards(board)
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
