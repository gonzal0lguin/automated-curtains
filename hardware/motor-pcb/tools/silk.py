#!/usr/bin/env python3
"""Silkscreen for the motor board: reference texts, labels and the artwork.

Run with KiCad's Python, KiCad closed, after build_pcb.py:

    python3 tools/silk.py

It can be run again at any time: it first removes every board-level drawing
and text on the two silkscreen layers.

Outer face (F): small volcano emblem (tools/emblem_small.json, made by
emblem_small.py) between the upper screws, flag of Chile on the right, RJ45
warning, motor pin names, test pad names.
Motor side (B): magnet note, DIR jumper labels, title, date, order-number
placeholder.
Coordinates: KiCad board mm, seen from the outer face, Y down.
"""
import json
from pathlib import Path

import pcbnew

from artwork import draw_flag

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "motor-pcb.kicad_pcb"
EMBLEM = Path(__file__).resolve().parent / "emblem_small.json"
F, B = 0, 1
SILK = {F: pcbnew.F_SilkS, B: pcbnew.B_SilkS}
TITLE = "BLINDS MOTOR  REV A"
DATE = "2026-11"

# Fixed labels: (text, x, y, height, angle, side[, stroke])
LABELS = [
    # motor header, pin names to the right of the pins
    ("MOTOR", 133.0, 88.6, 0.8, 0, F),
    ("A1", 138.0, 100.0, 0.8, 0, F), ("A2", 138.0, 97.5, 0.8, 0, F),
    ("B1", 138.0, 95.0, 0.8, 0, F), ("B2", 138.0, 92.5, 0.8, 0, F),
    # test pads
    ("3V3", 154.5, 91.2, 0.8, 0, F), ("GND", 158.0, 91.2, 0.8, 0, F),
    ("OUT", 148.9, 90.2, 0.8, 0, F),
    # RJ45 warning, right of the jack
    ("NOT ETHERNET", 164.6, 106.2, 1.0, 0, F, 0.2),
    ("DO NOT UNPLUG", 164.6, 107.9, 0.8, 0, F),
    ("WHILE POWERED", 164.6, 109.2, 0.8, 0, F),
    # motor side
    ("MOTOR SIDE", 150.0, 90.6, 1.2, 0, B, 0.2),
    ("MAGNET D6 ON THE AXIS", 150.0, 92.5, 0.8, 0, B),
    ("GAP 0.5-3 mm", 150.0, 93.9, 0.8, 0, B),
    ("DIR", 160.0, 101.9, 0.8, 0, B), ("CW", 160.0, 100.6, 0.8, 0, B),
    ("CCW", 160.0, 103.2, 0.8, 0, B),
    (TITLE, 150.0, 110.6, 1.0, 0, B, 0.15),
    (DATE, 150.0, 116.3, 0.8, 0, B),
    ("JLCJLCJLCJLC", 150.0, 118.4, 0.8, 0, B),
]

# Reference texts: ref: (x, y, angle) or None to hide.
REFS = {
    "R1": (146.65, 98.6, 90), "D2": (151.6, 100.75, 0), "R2": (155.0, 101.0, 0),
    "C3": (157.4, 101.6, 0), "R3": (163.5, 99.0, 90), "D1": (163.6, 103.0, 90),
    "J1": (150.0, 118.8, 0), "J2": (131.0, 104.2, 0),
    "U1": (150.0, 96.6, 0), "C1": (145.3, 96.9, 0), "C2": (142.9, 96.9, 0),
    "JP1": (156.6, 98.9, 0),
    "TP1": None, "TP2": None, "TP3": None,
    "H1": None, "H2": None, "H3": None, "H4": None,
}
REF_H, REF_T = 0.8, 0.15

FLAG = (163.3, 88.5, 7.2)        # x0, y0, width (3:2)


def mm(v):
    return pcbnew.FromMM(v)


def pt(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


class Pen:
    def __init__(self, board, side):
        self.board, self.layer, self.side = board, SILK[side], side

    def line(self, a, b, w=0.15):
        s = pcbnew.PCB_SHAPE(self.board, pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pt(*a)); s.SetEnd(pt(*b))
        s.SetWidth(mm(w)); s.SetLayer(self.layer)
        self.board.Add(s)

    def fill(self, pts):
        s = pcbnew.PCB_SHAPE(self.board, pcbnew.SHAPE_T_POLY)
        s.SetPolyPoints(pcbnew.VECTOR_VECTOR2I([pt(*p) for p in pts]))
        s.SetFilled(True); s.SetWidth(0); s.SetLayer(self.layer)
        self.board.Add(s)

    def fill_set(self, ps):
        ps = pcbnew.SHAPE_POLY_SET(ps)
        ps.Fracture()
        for i in range(ps.OutlineCount()):
            o = ps.Outline(i)
            pts = [o.CPoint(j) for j in range(o.PointCount())]
            if len(pts) < 3 or abs(o.Area()) < mm(0.15) * mm(0.15):
                continue
            s = pcbnew.PCB_SHAPE(self.board, pcbnew.SHAPE_T_POLY)
            s.SetPolyPoints(pcbnew.VECTOR_VECTOR2I(pts))
            s.SetFilled(True); s.SetWidth(0); s.SetLayer(self.layer)
            self.board.Add(s)

    def text(self, txt, x, y, h=0.8, t=None, angle=0):
        s = pcbnew.PCB_TEXT(self.board)
        s.SetText(txt)
        s.SetLayer(self.layer)
        s.SetTextSize(pcbnew.VECTOR2I(mm(h), mm(h)))
        s.SetTextThickness(mm(t or max(0.15, round(h * 0.15, 2))))
        s.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER)
        s.SetVertJustify(pcbnew.GR_TEXT_V_ALIGN_CENTER)
        s.SetMirrored(self.side == B)
        s.SetTextAngleDegrees(angle)
        s.SetPosition(pt(x, y))
        self.board.Add(s)


def draw_emblem(pen):
    polys = json.loads(EMBLEM.read_text())["polys"]
    for poly in polys:
        ps = pcbnew.SHAPE_POLY_SET()
        ps.NewOutline()
        for x, y in poly[0]:
            ps.Append(mm(x), mm(y))
        for hole in poly[1:]:
            k = ps.NewHole()
            for x, y in hole:
                ps.Append(mm(x), mm(y), -1, k)
        ps.Simplify()
        pen.fill_set(ps)
    return len(polys)


def main():
    board = pcbnew.LoadBoard(str(BOARD))
    for d in list(board.GetDrawings()):
        if d.GetLayer() in SILK.values():
            board.Delete(d)
    front, back = Pen(board, F), Pen(board, B)
    n = draw_emblem(front)
    draw_flag(front, *FLAG)
    for lab in LABELS:
        txt, x, y, h, ang, side = lab[:6]
        (front if side == F else back).text(txt, x, y, h, lab[6] if len(lab) > 6 else None, ang)
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        r = fp.Reference()
        spot = REFS.get(ref, "keep")
        if spot == "keep":
            print("no reference spot for", ref)
            continue
        if spot is None:
            r.SetVisible(False)
            continue
        r.SetVisible(True)
        r.SetTextSize(pcbnew.VECTOR2I(mm(REF_H), mm(REF_H)))
        r.SetTextThickness(mm(REF_T))
        r.SetPosition(pt(spot[0], spot[1]))
        r.SetTextAngleDegrees(spot[2])
        r.SetKeepUpright(False)
    board.Save(str(BOARD))
    print("emblem: %d polygons; %d labels; saved" % (n, len(LABELS)))


if __name__ == "__main__":
    main()
