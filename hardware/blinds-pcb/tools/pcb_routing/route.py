#!/usr/bin/env python3
"""Route the panel PCB.

Run with KiCad's Python (system python3 with pcbnew), KiCad closed:

    python3 tools/pcb_routing/route.py

It deletes every track, via and copper zone, then routes the board again from
the plan at the bottom of this file and fills the GND zones. It overwrites
manual routing, so use it only until tracks are edited by hand.

How it works: a grid router (0.25 mm, two layers, 45-degree moves, A*).
Every pad, hole and finished track is drawn into "blocked" maps, grown by the
track half-width plus the clearance that applies, so a free cell is a legal
track centre. Power nets are routed first, in a fixed order with fixed
waypoints; the remaining signals follow, shortest first. KiCad's DRC is the
judge of the result: run it after every change.

Coordinates: KiCad board mm, front view, Y down.
"""
import heapq
import json
import math
import subprocess
import sys
import tempfile
import time
from array import array
from pathlib import Path

import pcbnew

ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "blinds-pcb.kicad_pcb"

X0, Y0, X1, Y1 = 90.0, 40.0, 210.0, 160.0
CX, CY = 150.0, 100.0
G = 0.25                       # grid pitch
W = int(round((X1 - X0) / G)) + 1
H = int(round((Y1 - Y0) / G)) + 1
NC = W * H
MARGIN = 0.05                  # added to every clearance (grid effects)
FRONT, BACK = 0, 1
LAYER = {FRONT: pcbnew.F_Cu, BACK: pcbnew.B_Cu}
INF = 1 << 60

# Router classes: track width, clearance, via pad, via drill.
# The first five are the net classes of the project file; the "n" classes are
# neck-downs for fine-pitch pads and for passing between header pins.
RC = {
    "sig":  (0.25, 0.20, 0.6, 0.3),
    "p3":   (0.50, 0.20, 0.6, 0.3),
    "p5":   (0.80, 0.20, 0.8, 0.4),
    "mot":  (0.80, 0.25, 0.8, 0.4),
    "motn": (0.50, 0.25, 0.8, 0.4),
    "p12":  (1.00, 0.30, 0.8, 0.4),
    "p12n": (0.50, 0.30, 0.8, 0.4),
}
# Clearance each net asks of its neighbours (net class clearance).
NET_CLR = {"+12V": 0.3, "/VIN_JACK": 0.3, "/VIN_FUSED": 0.3,
           "/MOT_A1": 0.25, "/MOT_A2": 0.25, "/MOT_B1": 0.25, "/MOT_B2": 0.25}

# No stitching vias under the front silkscreen drawings (tools/pcb_silk/silk.py):
# volcanoes along the top, flag on the right. x0, y0, x1, y1.
NO_STITCH = [(138.0, 48.0, 194.0, 65.0), (186.5, 86.0, 208.5, 107.5)]

VIA_COST = 250                 # in units of 1/10 grid step
STEP = {  # layer cost profiles: (straight, diagonal) for front, back
    "back":  ((16, 23), (10, 14)),      # default: front only for short jumps
    "front": ((10, 14), (12, 17)),      # LED ring nets
    "any":   ((10, 14), (10, 14)),
}
DIRS = [(1, 0, 1, 0), (-1, 0, -1, 0), (0, 1, W, 0), (0, -1, -W, 0),
        (1, 1, W + 1, 1), (-1, 1, W - 1, 1), (1, -1, -W + 1, 1), (-1, -1, -W - 1, 1)]


def mm(v):
    return pcbnew.FromMM(v)


def cell(x, y):
    return int(round((x - X0) / G)), int(round((y - Y0) / G))


def cxy(ix, iy):
    return X0 + ix * G, Y0 + iy * G


# --------------------------------------------------------------- primitives

class Prim:
    """A piece of copper (or a hole / keep-out) the router must stay away
    from. dist(x, y) is the distance from a point to its outline."""
    __slots__ = ("dist", "bbox", "layers", "net", "nclr", "absclr")

    def __init__(self, dist, bbox, layers, net, absclr=None):
        self.dist, self.bbox, self.layers, self.net = dist, bbox, layers, net
        self.nclr = NET_CLR.get(net, 0.2)
        self.absclr = absclr


def rect_prim(cx, cy, hx, hy, deg, layers, net, absclr=None):
    a = math.radians(deg)
    ca, sa = math.cos(a), math.sin(a)

    def dist(px, py):
        dx, dy = px - cx, py - cy
        lx = abs(dx * ca - dy * sa) - hx
        ly = abs(dx * sa + dy * ca) - hy
        if lx < 0:
            lx = 0.0
        if ly < 0:
            ly = 0.0
        return math.sqrt(lx * lx + ly * ly)
    ex = abs(hx * ca) + abs(hy * sa)
    ey = abs(hx * sa) + abs(hy * ca)
    return Prim(dist, (cx - ex, cy - ey, cx + ex, cy + ey), layers, net, absclr)


def capsule_prim(x1, y1, x2, y2, r, layers, net, absclr=None):
    vx, vy = x2 - x1, y2 - y1
    l2 = vx * vx + vy * vy

    def dist(px, py):
        if l2 == 0:
            t = 0.0
        else:
            t = ((px - x1) * vx + (py - y1) * vy) / l2
            t = 0.0 if t < 0 else 1.0 if t > 1 else t
        dx, dy = px - x1 - t * vx, py - y1 - t * vy
        d = math.sqrt(dx * dx + dy * dy) - r
        return d if d > 0 else 0.0
    return Prim(dist, (min(x1, x2) - r, min(y1, y2) - r, max(x1, x2) + r,
                       max(y1, y2) + r), layers, net, absclr)


def oval_prim(cx, cy, sx, sy, deg, layers, net, absclr=None):
    """Oval / circle of size sx x sy, rotated like a KiCad pad."""
    a = math.radians(deg)
    r = min(sx, sy) / 2
    h = (max(sx, sy) - min(sx, sy)) / 2
    ux, uy = (math.cos(a), -math.sin(a)) if sx >= sy else (math.sin(a), math.cos(a))
    return capsule_prim(cx - ux * h, cy - uy * h, cx + ux * h, cy + uy * h, r,
                        layers, net, absclr)


def poly_prim(pts, layers, net, absclr=None):
    edges = list(zip(pts, pts[1:] + pts[:1]))

    def dist(px, py):
        inside = False
        best = 1e9
        for (x1, y1), (x2, y2) in edges:
            if (y1 > py) != (y2 > py) and px < x1 + (py - y1) * (x2 - x1) / (y2 - y1):
                inside = not inside
            vx, vy = x2 - x1, y2 - y1
            l2 = vx * vx + vy * vy
            t = ((px - x1) * vx + (py - y1) * vy) / l2 if l2 else 0.0
            t = 0.0 if t < 0 else 1.0 if t > 1 else t
            d = math.hypot(px - x1 - t * vx, py - y1 - t * vy)
            if d < best:
                best = d
        return 0.0 if inside else best
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    return Prim(dist, (min(xs), min(ys), max(xs), max(ys)), layers, net, absclr)


class Router:
    def __init__(self, board):
        self.board = board
        self.T = {k: [array("H", bytes(2 * NC)), array("H", bytes(2 * NC))] for k in RC}
        self.V = {k: array("H", bytes(2 * NC)) for k in RC}
        self.padmask = array("H", bytes(2 * NC))   # no via here (SMD pads)
        self.own = {}        # net -> [Prim]
        self.pads = {}       # "REF.PAD" -> (net, x, y, layers)
        self.padobj = {}     # "REF.PAD" -> pcbnew pad
        self.padprim = {}    # "REF.PAD" -> Prim
        self.written = [0, 0]
        self.netpads = {}    # net -> [pad names]
        self.tree = {}       # net -> set of nodes (layer * NC + cell)
        self.anchor = {}     # net -> {node: (x, y)} exact pad centres
        self.thru = {}       # net -> set of cells where both layers join (THT)
        self.done = {}       # net -> set of pad names in the tree
        self.tracks = []     # (net, x1, y1, x2, y2, layer, width)
        self.vias = []       # (net, x, y, pad, drill)
        self.stubs = set()
        self.failed = []
        self._collect()

    # ------------------------------------------------------------ obstacles

    def _raster(self, prim, r, arr, sign):
        bx0, by0, bx1, by1 = prim.bbox
        ix0 = max(0, int(math.ceil((bx0 - r - X0) / G)))
        ix1 = min(W - 1, int(math.floor((bx1 + r - X0) / G)))
        iy0 = max(0, int(math.ceil((by0 - r - Y0) / G)))
        iy1 = min(H - 1, int(math.floor((by1 + r - Y0) / G)))
        dist = prim.dist
        for iy in range(iy0, iy1 + 1):
            py = Y0 + iy * G
            row = iy * W
            for ix in range(ix0, ix1 + 1):
                if dist(X0 + ix * G, py) < r:
                    arr[row + ix] += sign

    def stamp(self, prim, sign=1, only=None):
        for k in (only or RC):
            width, clr, via, _ = RC[k]
            base = prim.absclr if prim.absclr is not None else max(clr, prim.nclr)
            for layer in (FRONT, BACK):
                if prim.layers & (1 << layer):
                    self._raster(prim, width / 2 + base + MARGIN, self.T[k][layer], sign)
            self._raster(prim, via / 2 + base + MARGIN, self.V[k], sign)

    def add_prim(self, prim):
        if prim.net:
            self.own.setdefault(prim.net, []).append(prim)
        self.stamp(prim)

    def _collect(self):
        board = self.board
        both = 3
        # board edge (0.5 mm copper to edge) as a chain of zero-width capsules
        r = 5.0
        pts = []
        for cx, cy, a0 in ((X1 - r, Y0 + r, 270), (X1 - r, Y1 - r, 0),
                           (X0 + r, Y1 - r, 90), (X0 + r, Y0 + r, 180)):
            for t in range(0, 91, 15):
                a = math.radians(a0 + t)
                pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
        for a, b in zip(pts, pts[1:] + pts[:1]):
            self.add_prim(capsule_prim(a[0], a[1], b[0], b[1], 0, both, None, 0.55))
        # rule areas (board level and inside footprints)
        zones = list(board.Zones())
        for fp in board.GetFootprints():
            zones.extend(fp.Zones())
        for z in zones:
            if z.GetIsRuleArea() and (z.GetDoNotAllowTracks() or z.GetDoNotAllowVias()):
                layers = (1 if z.IsOnLayer(pcbnew.F_Cu) else 0) | (2 if z.IsOnLayer(pcbnew.B_Cu) else 0)
                o = z.Outline()
                for i in range(o.OutlineCount()):
                    line = o.Outline(i)
                    pts = [(pcbnew.ToMM(line.CPoint(j).x), pcbnew.ToMM(line.CPoint(j).y))
                           for j in range(line.PointCount())]
                    self.add_prim(poly_prim(pts, layers, None, 0.05))
        for fp in board.GetFootprints():
            ref = fp.GetReference()
            for pad in fp.Pads():
                p = pad.GetPosition()
                x, y = pcbnew.ToMM(p.x), pcbnew.ToMM(p.y)
                deg = pad.GetOrientationDegrees()
                if pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                    d = pad.GetDrillSize()
                    dx, dy = pcbnew.ToMM(d.x), pcbnew.ToMM(d.y)
                    # M3 screw heads and washers: keep tracks from under them
                    # (KiCad treats NPTH edges like the board edge: 0.5 mm)
                    head = 1.5 if ref.startswith("H") else 0.55
                    self.add_prim(oval_prim(x, y, dx, dy, deg, 1, None, head))
                    back = 1.9 if ref == "MECH1" and dx == dy else head
                    self.add_prim(oval_prim(x, y, dx, dy, deg, 2, None, back))
                    continue
                net = pad.GetNetname()
                tht = pad.HasHole()
                layers = both if tht else (1 if pad.IsOnLayer(pcbnew.F_Cu) else 2)
                sx, sy = pcbnew.ToMM(pad.GetSize().x), pcbnew.ToMM(pad.GetSize().y)
                shape = pad.GetShape(pcbnew.F_Cu if layers & 1 else pcbnew.B_Cu)
                sp = pad.ShapePos(pcbnew.F_Cu if layers & 1 else pcbnew.B_Cu)
                px, py = pcbnew.ToMM(sp.x), pcbnew.ToMM(sp.y)   # copper may be offset from the hole
                if shape in (pcbnew.PAD_SHAPE_CIRCLE, pcbnew.PAD_SHAPE_OVAL):
                    prim = oval_prim(px, py, sx, sy, deg, layers, net or None)
                else:
                    prim = rect_prim(px, py, sx / 2, sy / 2, deg, layers, net or None)
                bb = pad.GetBoundingBox()
                # the model may be a little larger (rounded corners), never smaller
                dl = pcbnew.ToMM(bb.GetLeft()) - prim.bbox[0]
                db = prim.bbox[3] - pcbnew.ToMM(bb.GetBottom())
                if not (-0.02 < dl < 0.3 and -0.02 < db < 0.3):
                    sys.exit("pad model wrong for %s.%s" % (ref, pad.GetNumber()))
                self.add_prim(prim)
                if not tht:
                    self._raster(prim, 0.55, self.padmask, 1)
                if net and not net.startswith("unconnected-"):
                    name = "%s.%s" % (ref, pad.GetNumber())
                    while name in self.pads:          # SW4, SW1 MP: two pads, one number
                        name += "'"
                    self.pads[name] = (net, x, y, layers)
                    self.padobj[name] = pad
                    self.padprim[name] = prim
                    if net == "GND":
                        pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_INHERITED)
                    self.netpads.setdefault(net, []).append(name)

    # ---------------------------------------------------------------- tree

    def _pad_nodes(self, name):
        net, x, y, layers = self.pads[name]
        ix, iy = cell(x, y)
        c = iy * W + ix
        nodes = [c + layer * NC for layer in (FRONT, BACK) if layers & (1 << layer)]
        for n in nodes:
            self.anchor.setdefault(net, {})[n] = (x, y)
        if layers == 3:
            self.thru.setdefault(net, set()).add(c)
        return nodes

    def _join(self, net, name):
        self.tree.setdefault(net, set()).update(self._pad_nodes(name))
        self.done.setdefault(net, set()).add(name)

    def _target(self, net, item):
        """item: pad name or ("wp", x, y, layer)."""
        if isinstance(item, str):
            if self.pads[item][0] != net:
                sys.exit("%s is not on %s" % (item, net))
            return self._pad_nodes(item)
        _, x, y, layer = item
        ix, iy = cell(x, y)
        return [iy * W + ix + layer * NC]

    # ----------------------------------------------------------------- A*

    def _astar(self, k, net, sources, targets, profile, via_ok=True):
        Tk, Vk, padmask = self.T[k], self.V[k], self.padmask
        thru = self.thru.get(net, ())
        costs = STEP[profile]
        tset = set(targets)
        t0 = targets[0] % NC
        ty, tx = divmod(t0, W)
        gs = {}
        prev = {}
        heap = []
        for s in sources:
            c = s % NC
            iy, ix = divmod(c, W)
            dx, dy = abs(ix - tx), abs(iy - ty)
            gs[s] = 0
            heap.append((10 * max(dx, dy) + 4 * min(dx, dy), 0, s))
        heapq.heapify(heap)
        pop, push = heapq.heappop, heapq.heappush
        while heap:
            f, g, idx = pop(heap)
            if g > gs[idx]:
                continue
            if idx in tset:
                path = [idx]
                while idx in prev:
                    idx = prev[idx]
                    path.append(idx)
                return path[::-1]
            layer = 1 if idx >= NC else 0
            c = idx - NC if layer else idx
            iy, ix = divmod(c, W)
            Tl = Tk[layer]
            base = NC if layer else 0
            cs, cd = costs[layer]
            for dx, dy, off, diag in DIRS:
                nx, ny = ix + dx, iy + dy
                if nx < 0 or ny < 0 or nx >= W or ny >= H:
                    continue
                nc = c + off
                if Tl[nc]:
                    continue
                ng = g + (cd if diag else cs)
                ni = nc + base
                if ng < gs.get(ni, INF):
                    gs[ni] = ng
                    prev[ni] = idx
                    ax, ay = abs(nx - tx), abs(ny - ty)
                    push(heap, (ng + 10 * max(ax, ay) + 4 * min(ax, ay), ng, ni))
            other = 1 - layer
            if not Tk[other][c]:
                if c in thru:
                    ng = g + 1
                elif via_ok and not Vk[c] and not padmask[c]:
                    ng = g + VIA_COST
                else:
                    continue
                ni = c + (NC if other else 0)
                if ng < gs.get(ni, INF):
                    gs[ni] = ng
                    prev[ni] = idx
                    ax, ay = abs(ix - tx), abs(iy - ty)
                    push(heap, (ng + 10 * max(ax, ay) + 4 * min(ax, ay), ng, ni))
        return None

    # ------------------------------------------------------------ smoothing

    @staticmethod
    def _line(a, b):
        """Cells on a straight octilinear line a -> b, or None."""
        dx, dy = b[0] - a[0], b[1] - a[1]
        n = max(abs(dx), abs(dy))
        if n == 0:
            return [a]
        if dx and dy and abs(dx) != abs(dy):
            return None
        sx, sy = (dx > 0) - (dx < 0), (dy > 0) - (dy < 0)
        return [(a[0] + sx * i, a[1] + sy * i) for i in range(n + 1)]

    def _free(self, Tl, a, b):
        for ix, iy in self._line(a, b):
            if Tl[iy * W + ix]:
                return False
        return True

    def _shortcut(self, Tl, a, b):
        """One or two octilinear segments from a to b through free cells.
        Returns the list of corner points (possibly empty), or None."""
        dx, dy = b[0] - a[0], b[1] - a[1]
        if dx == 0 or dy == 0 or abs(dx) == abs(dy):
            return [] if self._free(Tl, a, b) else None
        d = min(abs(dx), abs(dy))
        sx, sy = (dx > 0) - (dx < 0), (dy > 0) - (dy < 0)
        for corner in ((b[0] - sx * d, b[1] - sy * d), (a[0] + sx * d, a[1] + sy * d)):
            if self._free(Tl, a, corner) and self._free(Tl, corner, b):
                return [corner]
        return None

    def _smooth(self, Tl, pts, look=160):
        out = [pts[0]]
        i, n = 0, len(pts)
        while i < n - 1:
            j = min(n - 1, i + look)
            corner = []
            while j > i + 1:
                corner = self._shortcut(Tl, pts[i], pts[j])
                if corner is not None:
                    break
                j -= 1
            else:
                corner = []
            out.extend(corner)
            out.append(pts[j])
            i = j
        res = [out[0]]                       # drop points on a straight line
        for p in out[1:]:
            if len(res) >= 2:
                a, b = res[-2], res[-1]
                if (b[0] - a[0]) * (p[1] - b[1]) == (b[1] - a[1]) * (p[0] - b[0]) \
                        and (b[0] - a[0]) * (p[0] - b[0]) >= 0 and (b[1] - a[1]) * (p[1] - b[1]) >= 0:
                    res[-1] = p
                    continue
            if p != res[-1]:
                res.append(p)
        return res

    # ------------------------------------------------------------- routing

    def _add_track(self, net, a, b, layer, width):
        if abs(a[0] - b[0]) < 1e-6 and abs(a[1] - b[1]) < 1e-6:
            return
        prim = capsule_prim(a[0], a[1], b[0], b[1], width / 2, 1 << layer, net)
        self.tracks.append((net, a[0], a[1], b[0], b[1], layer, width, prim))
        self.add_prim(prim)

    def _add_via(self, net, x, y, k):
        _, _, pad, drill = RC[k]
        ix, iy = cell(x, y)
        if iy * W + ix in self.thru.setdefault(net, set()):
            return                                   # already a via or THT pad here
        self.thru[net].add(iy * W + ix)              # the net may change layer here
        prim = capsule_prim(x, y, x, y, pad / 2, 3, net)
        self.vias.append([net, x, y, pad, drill, prim, None])
        self.add_prim(prim)
        self._raster(prim, 1.0, self.padmask, 1)     # hole to hole, any net

    def _stub(self, net, node, width):
        """Short track from an off-grid pad centre to its grid node."""
        xy = self.anchor.get(net, {}).get(node)
        if xy is None:
            return
        layer = 1 if node >= NC else 0
        iy, ix = divmod(node % NC, W)
        g = cxy(ix, iy)
        key = (node, )
        if key in self.stubs or math.hypot(g[0] - xy[0], g[1] - xy[1]) < 0.002:
            return
        self.stubs.add(key)
        self._add_track(net, xy, g, layer, width)

    def _route(self, net, k, targets, profile, via_ok=True, label="", sources=None):
        own = self.own.get(net, [])
        for p in own:
            self.stamp(p, -1, only=(k,))
        path = self._astar(k, net, sources or self.tree[net], targets, profile, via_ok)
        for p in own:
            self.stamp(p, 1, only=(k,))
        if path is None:
            self.failed.append("%s %s" % (net, label))
            print("  FAILED", net, label)
            return False
        width = RC[k][0]
        self._stub(net, path[0], width)
        self._stub(net, path[-1], width)
        # split into runs on one layer
        runs, cur = [], [path[0]]
        for n in path[1:]:
            if (n >= NC) != (cur[-1] >= NC):
                runs.append(cur)
                cur = [n]
            else:
                cur.append(n)
        runs.append(cur)
        thru = self.thru.get(net, ())
        tree = self.tree.setdefault(net, set())
        # own copper must not block the smoothing either
        for p in own:
            self.stamp(p, -1, only=(k,))
        geo = []
        for run in runs:
            layer = 1 if run[0] >= NC else 0
            pts = [(n % NC % W, n % NC // W) for n in run]
            geo.append((layer, self._smooth(self.T[k][layer], pts) if len(pts) > 1 else pts))
        for p in own:
            self.stamp(p, 1, only=(k,))
        for i, (layer, pts) in enumerate(geo):
            base = layer * NC
            for a, b in zip(pts, pts[1:]):
                self._add_track(net, cxy(*a), cxy(*b), layer, width)
                for ix, iy in self._line(a, b):
                    tree.add(base + iy * W + ix)
            tree.add(base + pts[0][1] * W + pts[0][0])
            if i:                                   # layer change before this run
                c = pts[0][1] * W + pts[0][0]
                if c not in thru:
                    self._add_via(net, *cxy(*pts[0]), k)
        return True

    def pre(self, net, pts, layer, width, join=True):
        """Fixed polyline with exact coordinates. Its vertices that lie on the
        grid become places the router can connect to (at once, or with
        join=False when the caller adds the returned nodes to the tree)."""
        nodes = set()
        for a, b in zip(pts, pts[1:]):
            self._add_track(net, a, b, layer, width)
        for x, y in pts:
            ix, iy = cell(x, y)
            g = cxy(ix, iy)
            if math.hypot(g[0] - x, g[1] - y) < 0.002:
                nodes.add(layer * NC + iy * W + ix)
        if join:
            self.tree.setdefault(net, set()).update(nodes)
        return nodes

    def chain(self, net, k, items, profile="back", via_ok=True):
        """Connect the items to the net's tree in the given order. After a
        waypoint the next piece starts at that waypoint."""
        last_wp = None
        for item in items:
            if net not in self.tree or not self.tree[net]:
                self._join(net, item)
                continue
            if isinstance(item, str) and item in self.done.get(net, ()):
                continue
            target = self._target(net, item)
            ok = self._route(net, k, target, profile, via_ok, str(item), sources=last_wp)
            last_wp = None
            if isinstance(item, str):
                if ok:
                    self._join(net, item)
            elif ok:
                last_wp = target

    def prune(self):
        """Remove track ends that lead nowhere (unused ends of fixed pieces)."""
        removed = 0
        while True:
            dead = []
            for t in self.tracks:
                net, x1, y1, x2, y2, layer, width, prim = t
                bit = 1 << layer
                others = [p for p in self.own[net] if p is not prim and p.layers & bit]
                for ex, ey in ((x1, y1), (x2, y2)):
                    if not any(p.dist(ex, ey) < 1e-6 for p in others):
                        dead.append(t)
                        break
            if not dead:
                return removed
            for t in dead:
                self.tracks.remove(t)
                self.own[t[0]].remove(t[7])
                self.stamp(t[7], -1)
                removed += 1

    def auto(self, net, k, over=None, profile="back", via_ok=True, first=None):
        """Connect every remaining pad of the net, nearest to the tree first."""
        over = over or {}
        todo = [p for p in self.netpads[net] if p not in self.done.get(net, ())]
        if not todo:
            return
        if net not in self.tree or not self.tree[net]:
            start = first or todo[0]
            self._join(net, start)
            todo.remove(start)
        while todo:
            tree_pts = [(n % NC % W, n % NC // W) for n in self.tree[net]]
            if len(tree_pts) > 400:
                tree_pts = tree_pts[::len(tree_pts) // 400]
            best = None
            for name in todo:
                ix, iy = cell(self.pads[name][1], self.pads[name][2])
                d = min((ix - a) ** 2 + (iy - b) ** 2 for a, b in tree_pts)
                if best is None or d < best[0]:
                    best = (d, name)
            name = best[1]
            todo.remove(name)
            if self._route(net, over.get(name, k), self._target(net, name), profile,
                           via_ok, name):
                self._join(net, name)

    # ----------------------------------------------------------- GND vias

    def gnd_via(self, name, reach=4.0, k="p3"):
        """Short GND track from a pad to a via next to it (both zones)."""
        net, x, y, layers = self.pads[name]
        if layers == 3:
            return True
        layer = FRONT if layers & 1 else BACK
        own = self.own.get(net, [])
        for p in own:
            self.stamp(p, -1, only=(k,))
        ix0, iy0 = cell(x, y)
        n = int(reach / G)
        cands = []
        for dy in range(-n, n + 1):
            for dx in range(-n, n + 1):
                ix, iy = ix0 + dx, iy0 + dy
                if not (0 <= ix < W and 0 <= iy < H):
                    continue
                c = iy * W + ix
                if (self.V[k][c] or self.padmask[c] or self.T[k][0][c]
                        or self.T[k][1][c]):
                    continue
                cands.append((dx * dx + dy * dy, c))
        cands.sort()
        path = None
        start = self._pad_nodes(name)
        for _, c in cands[:40]:
            path = self._astar(k, net, start, [c + layer * NC], "any", via_ok=False)
            if path and len(path) < 3 * n:
                break
            path = None
        for p in own:
            self.stamp(p, 1, only=(k,))
        if path is None:
            print("  no GND via for", name)
            return False
        width = RC[k][0]
        self._stub(net, path[0], width)
        pts = [(q % NC % W, q % NC // W) for q in path]
        for p in own:
            self.stamp(p, -1, only=(k,))
        pts = self._smooth(self.T[k][layer], pts) if len(pts) > 1 else pts
        for p in own:
            self.stamp(p, 1, only=(k,))
        for a, b in zip(pts, pts[1:]):
            self._add_track(net, cxy(*a), cxy(*b), layer, width)
        self._add_via(net, *cxy(*pts[-1]), k)
        return True

    def stitch(self, pitch=8.0, k="sig"):
        """GND vias on a lattice wherever both layers are free."""
        net = "GND"
        own = self.own.get(net, [])
        for p in own:
            self.stamp(p, -1, only=(k,))
        spots = []
        step = int(pitch / G)
        for iy in range(step // 2, H, step):
            for ix in range(step // 2, W, step):
                c = iy * W + ix
                if not (self.V[k][c] or self.padmask[c] or self.T[k][0][c]
                        or self.T[k][1][c]):
                    spots.append(cxy(ix, iy))
        for p in own:
            self.stamp(p, 1, only=(k,))
        for x, y in spots:
            if math.hypot(x - CX, y - CY) < 12.0:      # tower, screws, switch
                continue
            if any(a <= x <= c and b <= y <= d for a, b, c, d in NO_STITCH):
                continue
            self._add_via(net, x, y, k)
            self.vias[-1].append("stitch")
        return sum(1 for v in self.vias if v[-1] == "stitch")

    def gnd_link(self, name, others):
        """Track from a GND pad to the nearest of `others` [(x, y, layers)]."""
        _, x, y, layers = self.pads[name]
        for _, ox, oy, ol in sorted((math.hypot(ox - x, oy - y), ox, oy, ol)
                                    for ox, oy, ol in others)[:3]:
            ix, iy = cell(ox, oy)
            targets = [iy * W + ix + l * NC for l in (FRONT, BACK) if ol & (1 << l)]
            for k in ("p3", "sig"):
                n = len(self.failed)
                if self._route("GND", k, targets, "any", True, name,
                               sources=self._pad_nodes(name)):
                    return True
                del self.failed[n:]
        self.failed.append("GND " + name)
        return False

    # ------------------------------------------------------------- output

    def write(self):
        """Add the tracks and vias made since the last call to the board."""
        board = self.board
        nt, nv = self.written
        self.written = [len(self.tracks), len(self.vias)]
        for net, x1, y1, x2, y2, layer, width, _ in self.tracks[nt:]:
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(pcbnew.VECTOR2I(mm(x1), mm(y1)))
            t.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(y2)))
            t.SetWidth(mm(width))
            t.SetLayer(LAYER[layer])
            t.SetNet(board.FindNet(net))
            board.Add(t)
        for via in self.vias[nv:]:
            net, x, y, pad, drill = via[:5]
            v = pcbnew.PCB_VIA(board)
            via[6] = v
            v.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
            v.SetDrill(mm(drill))
            v.SetWidth(pcbnew.F_Cu, mm(pad))
            v.SetNet(board.FindNet(net))
            board.Add(v)


# ------------------------------------------------------------------- zones

def add_gnd_zones(board):
    net = board.FindNet("GND")
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(net)
        z.SetZoneName("GND front" if layer == pcbnew.F_Cu else "GND back")
        z.SetLocalClearance(mm(0.3))
        z.SetMinThickness(mm(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetThermalReliefGap(mm(0.4))
        z.SetThermalReliefSpokeWidth(mm(0.5))
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        o = z.Outline()
        o.NewOutline()
        for x, y in ((X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)):
            o.Append(mm(x), mm(y))
        board.Add(z)


def fill(board):
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


def gnd_groups(board, r):
    """Which GND pads and vias the filled zones really join together.
    Returns a list of groups, largest first: (pads, vias)."""
    islands = []
    for z in board.Zones():
        if z.GetIsRuleArea():
            continue
        for lid, layer in ((pcbnew.F_Cu, FRONT), (pcbnew.B_Cu, BACK)):
            if z.IsOnLayer(lid):
                ps = z.GetFilledPolysList(lid)
                islands.extend((layer, ps, i) for i in range(ps.OutlineCount()))
    parent = {}

    def find(a):
        parent.setdefault(a, a)
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def touch(item, x, y, layers, acc=0.05):
        find(item)
        p = pcbnew.VECTOR2I(mm(x), mm(y))
        for n, (layer, ps, i) in enumerate(islands):
            if layers & (1 << layer) and ps.Contains(p, i, mm(acc)):
                parent[find(("isl", n))] = find(item)

    def ring(prim, cx, cy, off=0.2, n=48):
        """Points 0.2 mm outside a pad: inside the thermal gap, where only
        spokes (or a solid connection) put copper."""
        for i in range(n):
            ux, uy = math.cos(2 * math.pi * i / n), math.sin(2 * math.pi * i / n)
            lo, hi = 0.0, 8.0
            for _ in range(14):
                mid = (lo + hi) / 2
                if prim.dist(cx + ux * mid, cy + uy * mid) < off:
                    lo = mid
                else:
                    hi = mid
            yield cx + ux * hi, cy + uy * hi

    pads = [n for n in r.netpads["GND"]]
    for name in pads:
        _, x, y, layers = r.pads[name]
        find(("pad", name))
        if r.padobj[name].GetLocalZoneConnection() != pcbnew.ZONE_CONNECTION_NONE:
            for px, py in ring(r.padprim[name], x, y):
                touch(("pad", name), px, py, layers, 0.0)
    vias = [v for v in r.vias if v[0] == "GND"]
    for n, v in enumerate(vias):
        touch(("via", n), v[1], v[2], 3)
    for t in r.tracks:
        if t[0] != "GND":
            continue
        bit = 1 << t[5]
        item = ("trk", id(t))
        for ex, ey in ((t[1], t[2]), (t[3], t[4])):
            touch(item, ex, ey, bit)
            end = ("end", round(ex, 3), round(ey, 3), t[5])     # next piece of the track
            parent[find(end)] = find(item)
            for name in pads:
                _, x, y, layers = r.pads[name]
                if layers & bit and abs(x - ex) < 0.2 and abs(y - ey) < 0.2:
                    parent[find(("pad", name))] = find(item)
            for n, v in enumerate(vias):
                if abs(v[1] - ex) < 0.01 and abs(v[2] - ey) < 0.01:
                    parent[find(("via", n))] = find(item)
    groups = {}
    for item in list(parent):
        if item[0] in ("pad", "via"):
            g = groups.setdefault(find(item), ([], []))
            g[0].append(item[1]) if item[0] == "pad" else g[1].append(vias[item[1]])
    return sorted(groups.values(), key=lambda g: -len(g[0]))


def starved_pads(board, r):
    """GND pads KiCad reports with too few thermal spokes."""
    board.Save(str(BOARD))
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "drc.json"
        subprocess.run(["kicad-cli", "pcb", "drc", "--format", "json", "--severity-all",
                        "-o", str(out), str(BOARD)], capture_output=True, check=True)
        report = json.loads(out.read_text())
    found = set()
    for v in report["violations"]:
        if v["type"] != "starved_thermal":
            continue
        for item in v["items"]:
            for name in r.netpads["GND"]:
                _, x, y, _ = r.pads[name]
                if abs(x - item["pos"]["x"]) < 0.05 and abs(y - item["pos"]["y"]) < 0.05:
                    found.add(name)
    return sorted(found)


def finish_gnd(board, r):
    """Fill the zones, then repair what the fill left out:
    - stitching vias on copper islands that reach no pad are removed,
    - a pad the zones do not reach gets a track to the nearest connected one,
    - a pad with too few thermal spokes is taken out of the zones and
      connected by a track instead (still easy to solder by hand)."""
    add_gnd_zones(board)
    for _ in range(6):
        r.write()
        fill(board)
        groups = gnd_groups(board, r)
        main = groups[0]
        good = [(r.pads[n][1], r.pads[n][2], r.pads[n][3]) for n in main[0]]
        good += [(v[1], v[2], 3) for v in main[1]]
        changed = False
        for pads, vias in groups[1:]:
            if not pads:
                for v in vias:
                    if v[-1] == "stitch":
                        board.Delete(v[6])
                        r.vias.remove(v)
                        r.own["GND"].remove(v[5])
                        r.stamp(v[5], -1)
                        r._raster(v[5], 1.0, r.padmask, -1)
                        r.thru["GND"].discard(cell(v[1], v[2])[1] * W + cell(v[1], v[2])[0])
                        r.written[1] -= 1
                        changed = True
                continue
            name = min(pads, key=lambda n: min(math.hypot(r.pads[n][1] - x, r.pads[n][2] - y)
                                               for x, y, _ in good))
            print("  GND track for", name, "(group of %d pads)" % len(pads))
            changed |= r.gnd_link(name, good)
        if changed:
            continue
        starved = starved_pads(board, r)
        if not starved:
            return
        for name in starved:
            print("  GND by track instead of thermal spokes:", name)
            r.padobj[name].SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_NONE)
    print("GND repair did not settle")


def clear_routing(board):
    for t in list(board.GetTracks()):
        board.Delete(t)
    for z in [z for z in board.Zones() if not z.GetIsRuleArea()]:
        board.Delete(z)


# -------------------------------------------------------------------- plan

def polar(r, a_cw):
    a = math.radians(a_cw)
    return CX + r * math.sin(a), CY - r * math.cos(a)


def snap(x, y):
    return cxy(*cell(x, y))


def plan(r):
    B, F = BACK, FRONT

    # ---- fixed pieces -----------------------------------------------------
    # +5 V arc inside the LED ring (front). Open at 6 o'clock, so the GND
    # plane inside the ring stays joined to the rest.
    arc = [snap(*polar(28.2, a)) for a in range(193, 528, 3)]
    arc_nodes = r.pre("+5V", arc, F, 0.8, join=False)
    # RJ45 pin 3 sits in the back row; its track leaves between pins 2 and 4
    # (1.04 mm gap), exactly centred.
    r.pre("/MOT_B1", [(106.495, 54.3), (106.495, 58.0), (106.5, 58.25)], B, 0.5)
    r.pre("/SCL", [(111.575, 54.3), (111.575, 58.0), (111.5, 58.25)], B, 0.25)
    # buck: EN tied to VIN at the pins
    r.pre("+12V", [(187.71, 146.0), (187.71, 146.95)], B, 0.5)

    # ---- input and buck (back) -------------------------------------------
    r.chain("/VIN_JACK", "p12", ["J1.1", "F1.1"])
    r.chain("/VIN_FUSED", "p12", ["F1.2", "D1.2"])
    r.chain("/SW", "p3", ["U1.5", "L1.1", "C4.2"])
    r.chain("/BST", "sig", ["U1.6", "C4.1"])
    r.chain("+12V", "p12", ["D1.1", "D2.1"])
    r.chain("+12V", "p12n", ["C3.1", "U1.3"])
    r.chain("+12V", "p12", ["C1.1", "C2.1"])
    # +12V trunk: along the bottom, up the gap beside the NodeMCU pins,
    # under the TMC2209 and up its left side to VM.
    r.chain("+12V", "p12", [("wp", 120.75, 152.25, B), ("wp", 120.75, 95.0, B),
                            ("wp", 93.5, 91.5, B), "C11.1", "A1.16", "C12.1"],
            via_ok=False)
    # buck output, then the feedback line round the top of the inductor
    # (not under it) from the output capacitors to FB
    r.chain("+5V", "p5", ["L1.2", "C5.1", "C6.1", "TP2.1", "C21.1"])
    r.chain("+5V", "sig", [("wp", 176.0, 139.5, B), ("wp", 188.25, 142.5, B), "U1.1"])
    r.auto("+12V", "p12n")

    # ---- motor phases: THT at both ends, so either layer, no vias --------
    r.done["/MOT_B1"] = {"J2.3"}          # already left their pads above
    r.done["/SCL"] = {"J2.7"}
    for net in ("/MOT_A1", "/MOT_B1", "/MOT_B2", "/MOT_A2"):
        r.auto(net, "mot", via_ok=False)

    # ---- +5 V: ring spokes, feed from C21, buck output, loads ------------
    r.chain("+5V", "p5", [("wp", arc[-1][0], arc[-1][1], F)], profile="any")
    r.tree["+5V"].update(arc_nodes)
    thin = {p: "p3" for p in r.netpads["+5V"] if p[0] in "DCRU"}
    r.auto("+5V", "p5", over=thin, profile="any")
    r.auto("/5V_MCU", "p3", first="U2.J5_1")

    # ---- LED data chain (front) ------------------------------------------
    for net in sorted(n for n in r.netpads if n.startswith("Net-(D") and "DO" in n
                      or n == "Net-(D10-DIN)"):
        r.auto(net, "sig", profile="front", via_ok=False)

    # ---- +3.3 V -----------------------------------------------------------
    r.auto("+3.3V", "p3", first="U2.J5_19")

    # ---- everything else, shortest first ---------------------------------
    rest = [n for n in r.netpads if n != "GND" and len(r.netpads[n]) > 1
            and len(r.done.get(n, ())) < len(r.netpads[n])]

    def span(net):
        xs = [r.pads[p][1] for p in r.netpads[net]]
        ys = [r.pads[p][2] for p in r.netpads[net]]
        return max(xs) - min(xs) + max(ys) - min(ys)
    for net in sorted(rest, key=span):
        r.auto(net, "sig")

    # ---- GND: vias at the buck and bulk caps, then a stitching lattice ---
    for name in ("C1.2", "C2.2", "C3.2", "U1.4", "C5.2", "C6.2", "D2.2",
                 "C11.2", "C13.2", "C18.2", "C19.2", "C20.2", "U3.1", "U3.3",
                 "C8.2", "C9.2", "C10.2", "C14.2"):
        r.gnd_via(name)


def main():
    t0 = time.time()
    board = pcbnew.LoadBoard(str(BOARD))
    clear_routing(board)
    r = Router(board)
    print("obstacles in %.0f s" % (time.time() - t0))
    plan(r)
    n = r.stitch()
    print("pruned %d dead-end tracks" % r.prune())
    finish_gnd(board, r)
    board.Save(str(BOARD))
    if "--no-zones" in sys.argv:           # extra copy for looking at the tracks alone
        for z in [z for z in board.Zones() if not z.GetIsRuleArea()]:
            board.Delete(z)
        out = ROOT / "output" / "routed-no-zones.kicad_pcb"
        out.parent.mkdir(exist_ok=True)
        board.Save(str(out))
    print("%d tracks, %d vias (%d stitching), %.0f s"
          % (len(r.tracks), len(r.vias), n, time.time() - t0))
    if r.failed:
        print("NOT ROUTED:")
        for f in r.failed:
            print("  ", f)
        sys.exit(1)


if __name__ == "__main__":
    main()
