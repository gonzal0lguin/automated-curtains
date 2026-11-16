#!/usr/bin/env python3
"""Silkscreen for the panel PCB: reference texts, labels and the front artwork.

Run with KiCad's Python, KiCad closed, after place.py and route.py:

    python3 tools/pcb_silk/silk.py            # everything below
    python3 tools/pcb_silk/silk.py --emblem   # only redraw the front emblem

What it does (it can be run again at any time, the result is the same):

1. removes every board-level drawing on the two silkscreen layers;
2. draws the front artwork (volcano emblem along the top, flag on the right);
3. adds the fixed labels (RJ45 warning, pin names, board title);
4. clips footprint silkscreen that reaches the board edge and removes the
   "1" pin marks of the LEDs (they sit on the neighbouring capacitor pad);
5. moves every reference text to the nearest free spot next to its part
   (test points show their net name instead), then the labels that belong
   to a part ("BOOT", "12V DC", ...).

"Free" means: not on a pad, not on other silkscreen, not under another part,
not on a via hole, inside the board edge.

Coordinates: KiCad board mm, front view, Y down.
"""
import json
import math
import sys
from pathlib import Path

import pcbnew

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "blinds-pcb.kicad_pcb"

X0, Y0, X1, Y1 = 90.0, 40.0, 210.0, 160.0
R_CORNER = 5.0
CX, CY = 150.0, 100.0
EDGE = 0.6                  # silkscreen to board edge
GAP = 0.15                  # silkscreen to pads / other silkscreen
REF_H, REF_T = 0.8, 0.15    # reference text height, stroke
F, B = 0, 1
SILK = {F: pcbnew.F_SilkS, B: pcbnew.B_SilkS}
TITLE = "BLINDS PANEL  REV A"
DATE = "2026-11"

# Parts whose courtyard does not block text (hollow encoder, under the knob).
OPEN_COURTYARD = ("SW1",)
# Test points show the value (net name) instead of the reference.
VALUE_AS_NAME = ("TP1", "TP2", "TP3", "TP4", "TP5")

# Front artwork boxes (x0, y0, x1, y1). route.py keeps stitching vias out of
# them (NO_STITCH there); keep the two lists in step.
ART_VOLCANO = (141.0, 48.2, 191.0, 64.8)
ART_FLAG = (187.9, 87.4, 207.1, 105.2)

# Volcano emblem ("Andes made"): Lonquimay, Llaima, Villarrica and Osorno
# rendered from real elevation data as a line engraving, with its frame and
# title. emblem.py makes it (own Python environment, see there) and writes
# the polygons to emblem.json; this script only draws them. Everything lies
# inside ART_VOLCANO.
EMBLEM = Path(__file__).resolve().parent / "emblem.json"

# Fixed labels: (text, x, y, height, angle, side[, stroke]).
LABELS = [
    # RJ45
    ("NOT ETHERNET", 109.5, 60.6, 1.2, 0, B, 0.2),
    ("DO NOT UNPLUG", 109.5, 62.6, 0.8, 0, B),
    ("WHILE POWERED", 109.5, 64.1, 0.8, 0, B),
    # spare I2C header, pin names under the pins
    ("3V3", 122.75, 49.3, 0.8, 0, B, 0.15), ("GND", 125.25, 49.3, 0.8, 0, B, 0.15),
    ("SDA", 127.75, 49.3, 0.8, 0, B, 0.15), ("SCL", 130.25, 49.3, 0.8, 0, B, 0.15),
    # LCD connector: wire order above the opening (pin 1 = BLK)
    ("BLK", 143.0, 123.4, 0.8, 90, B), ("CS", 145.0, 123.75, 0.8, 90, B),
    ("DC", 147.0, 123.75, 0.8, 90, B), ("RES", 149.0, 123.4, 0.8, 90, B),
    ("SDA", 151.0, 123.4, 0.8, 90, B), ("SCL", 153.0, 123.4, 0.8, 90, B),
    ("VIN", 155.0, 123.4, 0.8, 90, B), ("GND", 157.0, 123.4, 0.8, 90, B),
    # TMC2209 socket: corner pins, readable with the module out
    ("VM", 100.9, 69.61, 0.8, 0, B), ("EN", 108.1, 69.61, 0.8, 0, B),
    ("GND", 101.3, 87.6, 0.8, 0, B), ("DIR", 107.7, 87.6, 0.8, 0, B),
    ("TMC2209", 104.5, 78.5, 1.0, 90, B),
    # NodeMCU socket
    ("ANTENNA", 105.3, 106.3, 0.8, 0, B), ("USB", 105.3, 157.4, 1.0, 0, B),
    ("3V3", 114.6, 108.9, 0.8, 0, B), ("GND", 95.6, 108.9, 0.8, 0, B),
    # board title, date and the place for JLCPCB's order number
    (TITLE, 185.0, 70.0, 1.5, 0, B, 0.25),
    (DATE, 185.0, 73.0, 1.0, 0, B),
    ("JLCJLCJLCJLC", 185.0, 80.0, 1.0, 0, B),
]

# Labels that belong to a part and are placed like its reference.
PART_LABELS = [
    ("J1", "12V DC\nCENTRE +", 0.8), ("SW2", "RESET", 0.8), ("SW3", "BOOT", 0.8),
    ("J3", "I2C", 0.8), ("J4", "LCD", 0.8), ("Q1", "LIGHT", 0.8),
    ("D3", "12V", 0.8), ("D4", "5V", 0.8),
]


def mm(v):
    return pcbnew.FromMM(v)


def pt(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


def xy(p):
    return pcbnew.ToMM(p.x), pcbnew.ToMM(p.y)


def box_mm(bb):
    return (pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop()),
            pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom()))


# ------------------------------------------------------------------ geometry

def in_board(x, y, d):
    """Point at least d inside the rounded outline."""
    if not (X0 + d <= x <= X1 - d and Y0 + d <= y <= Y1 - d):
        return False
    for cx in (X0 + R_CORNER, X1 - R_CORNER):
        for cy in (Y0 + R_CORNER, Y1 - R_CORNER):
            if (x - cx) * (cx - CX) > 0 and (y - cy) * (cy - CY) > 0:
                return math.hypot(x - cx, y - cy) <= R_CORNER - d
    return True


def pt_rect(px, py, r):
    dx = max(r[0] - px, 0, px - r[2])
    dy = max(r[1] - py, 0, py - r[3])
    return math.hypot(dx, dy)


def pt_seg(px, py, a, b):
    vx, vy = b[0] - a[0], b[1] - a[1]
    L = vx * vx + vy * vy
    t = 0 if L == 0 else max(0, min(1, ((px - a[0]) * vx + (py - a[1]) * vy) / L))
    return math.hypot(px - a[0] - t * vx, py - a[1] - t * vy)


def seg_rect(a, b, r):
    """Distance between segment a-b and rectangle r (0 if they cross)."""
    # Liang-Barsky: does the segment enter the rectangle?
    t0, t1 = 0.0, 1.0
    dx, dy = b[0] - a[0], b[1] - a[1]
    inside = True
    for p, q in ((-dx, a[0] - r[0]), (dx, r[2] - a[0]), (-dy, a[1] - r[1]), (dy, r[3] - a[1])):
        if p == 0:
            if q < 0:
                inside = False
                break
        else:
            t = q / p
            if p < 0:
                t0 = max(t0, t)
            else:
                t1 = min(t1, t)
    if inside and t0 <= t1:
        return 0.0
    d = min(pt_rect(a[0], a[1], r), pt_rect(b[0], b[1], r))
    for c in ((r[0], r[1]), (r[2], r[1]), (r[2], r[3]), (r[0], r[3])):
        d = min(d, pt_seg(c[0], c[1], a, b))
    return d


def rect_gap(a, b):
    dx = max(b[0] - a[2], 0, a[0] - b[2])
    dy = max(b[1] - a[3], 0, a[1] - b[3])
    return math.hypot(dx, dy)


class Obstacles:
    """Things text must stay clear of, one list per side.

    ("r", box, clr) rectangle, ("s", a, b, clr) segment, ("c", x, y, clr) dot.
    """

    def __init__(self):
        self.items = {F: [], B: []}

    def rect(self, side, box, clr=GAP):
        self.items[side].append(("r", box, clr, box))

    def seg(self, side, a, b, clr):
        bb = (min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1]))
        self.items[side].append(("s", (a, b), clr, bb))

    def dot(self, side, x, y, clr):
        self.items[side].append(("c", (x, y), clr, (x, y, x, y)))

    def near(self, side, region):
        return [o for o in self.items[side]
                if rect_gap(o[3], region) <= o[2] + 0.01]

    @staticmethod
    def hit(box, obs):
        for kind, g, clr, bb in obs:
            if rect_gap(bb, box) >= clr:
                continue
            if kind == "r":
                return True
            if kind == "c":
                if pt_rect(g[0], g[1], box) < clr:
                    return True
            elif seg_rect(g[0], g[1], box) < clr:
                return True
        return False


def shape_segments(g):
    """Outline of a footprint graphic as segments [(a, b)]."""
    t = g.GetShape()
    if t == pcbnew.SHAPE_T_SEGMENT:
        return [(xy(g.GetStart()), xy(g.GetEnd()))]
    if t == pcbnew.SHAPE_T_RECT:
        c = [xy(p) for p in g.GetRectCorners()]
        return [(c[i], c[(i + 1) % 4]) for i in range(4)]
    if t == pcbnew.SHAPE_T_CIRCLE:
        cx, cy = xy(g.GetCenter())
        r = pcbnew.ToMM(g.GetRadius())
        p = [(cx + r * math.cos(i * math.pi / 12), cy + r * math.sin(i * math.pi / 12))
             for i in range(25)]
        return list(zip(p, p[1:]))
    if t == pcbnew.SHAPE_T_ARC:
        cx, cy = xy(g.GetCenter())
        r = pcbnew.ToMM(g.GetRadius())
        s, m, e = xy(g.GetStart()), xy(g.GetArcMid()), xy(g.GetEnd())
        a0 = math.atan2(s[1] - cy, s[0] - cx)
        am = math.atan2(m[1] - cy, m[0] - cx)
        a1 = math.atan2(e[1] - cy, e[0] - cx)
        d1 = (a1 - a0) % (2 * math.pi)
        if (am - a0) % (2 * math.pi) > d1:        # the arc runs the other way
            d1 -= 2 * math.pi
        n = max(2, int(abs(d1) * r / 0.5))
        p = [(cx + r * math.cos(a0 + d1 * i / n), cy + r * math.sin(a0 + d1 * i / n))
             for i in range(n + 1)]
        return list(zip(p, p[1:]))
    if t == pcbnew.SHAPE_T_POLY:
        out = []
        ps = g.GetPolyShape()
        for i in range(ps.OutlineCount()):
            o = ps.Outline(i)
            p = [xy(o.CPoint(j)) for j in range(o.PointCount())]
            out += list(zip(p, p[1:] + p[:1]))
        return out
    b = box_mm(g.GetBoundingBox())
    c = [(b[0], b[1]), (b[2], b[1]), (b[2], b[3]), (b[0], b[3])]
    return [(c[i], c[(i + 1) % 4]) for i in range(4)]


# ------------------------------------------------------------------- drawing

class Pen:
    def __init__(self, board, side):
        self.board, self.layer, self.side = board, SILK[side], side

    def line(self, a, b, w=0.2):
        s = pcbnew.PCB_SHAPE(self.board, pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pt(*a))
        s.SetEnd(pt(*b))
        s.SetWidth(mm(w))
        s.SetLayer(self.layer)
        self.board.Add(s)

    def path(self, pts, w=0.2, close=False):
        for a, b in zip(pts, pts[1:] + (pts[:1] if close else [])):
            self.line(a, b, w)

    def fill(self, pts):
        s = pcbnew.PCB_SHAPE(self.board, pcbnew.SHAPE_T_POLY)
        s.SetPolyPoints(pcbnew.VECTOR_VECTOR2I([pt(*p) for p in pts]))
        s.SetFilled(True)
        s.SetWidth(0)
        s.SetLayer(self.layer)
        self.board.Add(s)

    def fill_set(self, ps):
        """Filled polygons from a SHAPE_POLY_SET (holes allowed), one shape
        per outline."""
        ps = pcbnew.SHAPE_POLY_SET(ps)
        ps.Fracture()
        for i in range(ps.OutlineCount()):
            o = ps.Outline(i)
            pts = [o.CPoint(j) for j in range(o.PointCount())]
            if len(pts) < 3 or abs(o.Area()) < mm(0.15) * mm(0.15):   # specks
                continue
            s = pcbnew.PCB_SHAPE(self.board, pcbnew.SHAPE_T_POLY)
            s.SetPolyPoints(pcbnew.VECTOR_VECTOR2I(pts))
            s.SetFilled(True)
            s.SetWidth(0)
            s.SetLayer(self.layer)
            self.board.Add(s)

    def circle(self, x, y, r, w=0.15):
        s = pcbnew.PCB_SHAPE(self.board, pcbnew.SHAPE_T_CIRCLE)
        s.SetCenter(pt(x, y))
        s.SetEnd(pt(x + r, y))
        s.SetWidth(mm(w))
        s.SetLayer(self.layer)
        self.board.Add(s)

    def text(self, txt, x, y, h=1.0, t=None, angle=0):
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
        return s


def draw_flag(pen):
    """Flag of Chile, 3:2. White is solid silkscreen; blue and red use the
    heraldic hatching (blue = horizontal lines, red = vertical lines)."""
    x0, y0, w, h = 188.5, 88.0, 18.0, 12.0
    x1, y1, ym = x0 + w, y0 + h, y0 + h / 2
    xc = x0 + h / 2                               # canton is a square, h/2
    pen.path([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], 0.2, close=True)
    pen.fill([(xc, y0), (x1, y0), (x1, ym), (xc, ym)])            # white band
    pen.line((x0, ym), (xc, ym), 0.15)
    # star: diameter = half the canton side
    sx, sy, ro = x0 + h / 4, y0 + h / 4, h / 8
    ri = ro * 0.382
    star = []
    for i in range(10):
        r = ro if i % 2 == 0 else ri
        a = -math.pi / 2 + i * math.pi / 5
        star.append((sx + r * math.cos(a), sy + r * math.sin(a)))
    pen.fill(star)
    # blue canton: horizontal lines, stopping short of the star
    keep = ro + 0.45
    n = int(round((h / 2) / 0.6))
    for i in range(1, n):
        y = y0 + i * (h / 2) / n
        dy = abs(y - sy)
        if dy < keep:
            dx = math.sqrt(keep * keep - dy * dy)
            pen.line((x0, y), (sx - dx, y), 0.15)
            pen.line((sx + dx, y), (xc, y), 0.15)
        else:
            pen.line((x0, y), (xc, y), 0.15)
    # red band: vertical lines
    n = int(round(w / 0.6))
    for i in range(1, n):
        x = x0 + i * w / n
        pen.line((x, ym), (x, y1), 0.15)
    pen.text("Designed in Chile", x0 + w / 2, y1 + 1.6, 1.0)
    pen.text("by Gonz", x0 + w / 2, y1 + 3.6, 1.0)


def draw_emblem(pen):
    """Filled polygons (outline + holes) from emblem.json."""
    polys = json.loads(EMBLEM.read_text())["polys"]
    for poly in polys:
        ps = pcbnew.SHAPE_POLY_SET()
        ps.NewOutline()
        for x, y in poly[0]:
            ps.Append(mm(x), mm(y))
        for hole in poly[1:]:
            h = ps.NewHole()
            for x, y in hole:
                ps.Append(mm(x), mm(y), -1, h)
        ps.Simplify()                       # normalise winding, merge touching
        pen.fill_set(ps)
    return len(polys)


def remove_emblem(board):
    """Delete the front silkscreen drawings that lie inside ART_VOLCANO."""
    x0, y0, x1, y1 = ART_VOLCANO
    n = 0
    for d in list(board.GetDrawings()):
        if d.GetLayer() != SILK[F]:
            continue
        bx0, by0, bx1, by1 = box_mm(d.GetBoundingBox())
        if bx0 >= x0 and by0 >= y0 and bx1 <= x1 and by1 <= y1:
            board.Delete(d)
            n += 1
    return n


# ------------------------------------------------------- footprint clean-up

def clip_edge_silk(board):
    """Shorten or drop footprint silkscreen lines that reach the board edge."""
    n = 0
    for fp in board.GetFootprints():
        for g in list(fp.GraphicalItems()):
            if g.GetLayer() not in SILK.values() or g.GetClass() != "PCB_SHAPE":
                continue
            if g.GetShape() != pcbnew.SHAPE_T_SEGMENT:
                continue
            d = EDGE + pcbnew.ToMM(g.GetWidth()) / 2
            a, b = xy(g.GetStart()), xy(g.GetEnd())
            ia, ib = in_board(*a, d), in_board(*b, d)
            if ia and ib:
                continue
            n += 1
            if not ia and not ib:
                fp.Remove(g)
                continue
            lo, hi = (a, b) if ia else (b, a)          # lo inside, hi outside
            for _ in range(30):
                m = ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2)
                if in_board(*m, d):
                    lo = m
                else:
                    hi = m
            (g.SetEnd if ia else g.SetStart)(pt(*lo))
    return n


def drop_led_pin_marks(board):
    n = 0
    for fp in board.GetFootprints():
        for g in list(fp.GraphicalItems()):
            if (g.GetClass() == "PCB_TEXT" and g.GetLayer() in SILK.values()
                    and g.GetText() == "1"):
                fp.Remove(g)
                n += 1
    return n


# ------------------------------------------------------------ text placement

def collect(board):
    obs = Obstacles()
    for fp in board.GetFootprints():
        side = B if fp.IsFlipped() else F
        for p in fp.Pads():
            bb = box_mm(p.GetBoundingBox())
            for s in (F, B):
                if p.IsOnLayer(pcbnew.F_Mask if s == F else pcbnew.B_Mask) \
                        or p.GetDrillSize().x > 0:
                    obs.rect(s, bb, GAP + 0.05)
        for g in fp.GraphicalItems():
            if g.GetLayer() not in SILK.values():
                continue
            s = F if g.GetLayer() == pcbnew.F_SilkS else B
            if g.GetClass() == "PCB_SHAPE":
                for a, b in shape_segments(g):
                    obs.seg(s, a, b, pcbnew.ToMM(g.GetWidth()) / 2 + GAP)
            else:
                obs.rect(s, box_mm(g.GetBoundingBox()))
        if fp.GetReference() not in OPEN_COURTYARD:
            poly = fp.GetCourtyard(pcbnew.B_CrtYd if side == B else pcbnew.F_CrtYd)
            for i in range(poly.OutlineCount()):
                obs.rect(side, box_mm(poly.Outline(i).BBox()), 0.0)
    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            x, y = xy(t.GetPosition())
            for s in (F, B):
                obs.dot(s, x, y, pcbnew.ToMM(t.GetDrill()) / 2 + 0.1)
    for d in board.GetDrawings():
        if d.GetLayer() in SILK.values():
            obs.rect(F if d.GetLayer() == pcbnew.F_SilkS else B,
                     box_mm(d.GetBoundingBox()), 0.03)
    obs.rect(F, ART_VOLCANO, 0.0)
    obs.rect(F, ART_FLAG, 0.0)
    return obs


def body_box(fp):
    """Box the text has to stay outside of: courtyard and pads."""
    side_layer = pcbnew.B_CrtYd if fp.IsFlipped() else pcbnew.F_CrtYd
    boxes = [box_mm(p.GetBoundingBox()) for p in fp.Pads()]
    poly = fp.GetCourtyard(side_layer)
    if poly.OutlineCount():
        boxes.append(box_mm(poly.BBox()))
    if not boxes:
        boxes = [box_mm(fp.GetBoundingBox(False))]
    return (min(b[0] for b in boxes), min(b[1] for b in boxes),
            max(b[2] for b in boxes), max(b[3] for b in boxes))


def text_size(item, angle):
    """Width and height of a text item's box at `angle`."""
    item.SetTextAngleDegrees(angle)
    b = box_mm(item.GetBoundingBox())
    return b[2] - b[0], b[3] - b[1]


def find_spot(body, sizes, obs, side, reach=4.0):
    """Nearest free box next to `body`. sizes: {angle: (w, h)}.
    Returns (cx, cy, angle) or None."""
    region = (body[0] - reach - 8, body[1] - reach - 8,
              body[2] + reach + 8, body[3] + reach + 8)
    near = obs.near(side, region)
    bx, by = (body[0] + body[2]) / 2, (body[1] + body[3]) / 2
    cands = []
    for angle, (w, h) in sizes.items():
        pen = 0.4 if angle else 0.0
        g = 0.2
        while g <= reach:
            # above / below: slide in x
            x = body[0] - w / 2
            while x <= body[2] + w / 2:
                for y in (body[1] - g - h / 2, body[3] + g + h / 2):
                    cands.append((g + 0.2 * abs(x - bx) + pen, x, y, angle, w, h))
                x += 0.25
            # left / right: slide in y
            y = body[1] - h / 2
            while y <= body[3] + h / 2:
                for x in (body[0] - g - w / 2, body[2] + g + w / 2):
                    cands.append((g + 0.2 * abs(y - by) + pen, x, y, angle, w, h))
                y += 0.25
            g += 0.25
    cands.sort()
    for _, x, y, angle, w, h in cands:
        box = (x - w / 2, y - h / 2, x + w / 2, y + h / 2)
        if not all(in_board(px, py, EDGE) for px in (box[0], box[2])
                   for py in (box[1], box[3])):
            continue
        if not Obstacles.hit(box, near):
            return x, y, angle
    return None


def style(item, h, t):
    item.SetTextSize(pcbnew.VECTOR2I(mm(h), mm(h)))
    item.SetTextThickness(mm(t))
    item.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER)
    item.SetVertJustify(pcbnew.GR_TEXT_V_ALIGN_CENTER)
    item.SetKeepUpright(False)


def settle(item, body, obs, side, what, reach=4.0):
    sizes = {a: text_size(item, a) for a in (0, 90)}
    spot = find_spot(body, sizes, obs, side, reach) \
        or find_spot(body, sizes, obs, side, reach * 3)
    if spot is None:
        print("NO ROOM:", what)
        return False
    x, y, angle = spot
    item.SetTextAngleDegrees(angle)
    item.SetPosition(pt(x, y))
    # the box KiCad reports may sit a little off the anchor: correct once
    b = box_mm(item.GetBoundingBox())
    ex, ey = x - (b[0] + b[2]) / 2, y - (b[1] + b[3]) / 2
    if abs(ex) > 0.01 or abs(ey) > 0.01:
        item.SetPosition(pt(x + ex, y + ey))
        b = box_mm(item.GetBoundingBox())
    obs.rect(side, b, GAP)
    return True


def place_texts(board, obs):
    fps = sorted(board.GetFootprints(), key=lambda f: f.GetReference())
    done = 0
    for fp in fps:
        ref = fp.GetReference()
        side = B if fp.IsFlipped() else F
        field = fp.Reference()
        if ref in VALUE_AS_NAME:
            field.SetVisible(False)
            field = fp.Value()
            field.SetLayer(SILK[side])
            field.SetVisible(True)
        if not field.IsVisible():
            continue
        field.SetLayer(SILK[side])
        style(field, REF_H, REF_T)
        done += settle(field, body_box(fp), obs, side, ref)
    for ref, txt, h in PART_LABELS:
        fp = board.FindFootprintByReference(ref)
        side = B if fp.IsFlipped() else F
        item = Pen(board, side).text(txt, 0, 0, h)
        settle(item, body_box(fp), obs, side, "%s label %r" % (ref, txt), 6.0)
    return done


def check_labels(board, obs):
    """Fixed labels must be free too: report the ones that are not."""
    for d in board.GetDrawings():
        if d.GetClass() != "PCB_TEXT" or d.GetLayer() not in SILK.values():
            continue
        side = F if d.GetLayer() == pcbnew.F_SilkS else B
        if side == F:
            continue                                # artwork captions
        box = box_mm(d.GetBoundingBox())
        # other labels are 0.03 away at most by design: test real obstacles
        others = [o for o in obs.near(side, box) if o[2] != 0.03]
        ink = (box[0] + 0.1, box[1] + 0.1, box[2] - 0.1, box[3] - 0.1)   # box has a margin
        if Obstacles.hit(ink, others):
            print("LABEL COLLIDES:", d.GetText().replace("\n", " / "),
                  "at %.1f, %.1f" % ((box[0] + box[2]) / 2, (box[1] + box[3]) / 2))


def main():
    board = pcbnew.LoadBoard(str(BOARD))
    if "--emblem" in sys.argv[1:]:
        # only the front emblem: everything else on the board stays as it is
        n = remove_emblem(board)
        m = draw_emblem(Pen(board, F))
        board.Save(str(BOARD))
        print("emblem: %d old drawings removed, %d polygons drawn" % (n, m))
        return
    for d in list(board.GetDrawings()):
        if d.GetLayer() in SILK.values():
            board.Delete(d)
    for fp in board.GetFootprints():
        fp.BuildCourtyardCaches()

    front = Pen(board, F)
    draw_emblem(front)
    draw_flag(front)
    for text, x, y, h, angle, side, *t in LABELS:
        Pen(board, side).text(text, x, y, h, t[0] if t else None, angle)

    clipped = clip_edge_silk(board)
    marks = drop_led_pin_marks(board)
    obs = collect(board)
    check_labels(board, obs)
    n = place_texts(board, obs)
    board.Save(str(BOARD))
    print("silkscreen: %d texts placed, %d edge lines clipped, %d LED pin marks removed"
          % (n, clipped, marks))


if __name__ == "__main__":
    main()
