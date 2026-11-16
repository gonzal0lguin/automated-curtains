#!/usr/bin/env python3
"""One-shot fixes from the pre-order review (2026-11-15), applied on top of
the hand-edited board. Unlike route.py it keeps all existing routing and
changes only what is listed below. KiCad must be closed.

    python3 tools/pcb_routing/review_fixes.py

1. J1 (DC jack): the three plated slots grow from 3.0 to 3.6 mm and pad 2
   moves 0.15 mm, so both the DC-005 (C16214) and the DC005-T20 (C111567,
   3 A, blades about 2.9 mm wide, datasheet asks 3.5 mm slots) fit.
2. Four front-side keep-out rule areas for the encoder's mounting lugs
   (ALPS EC35A drawing, "wiring prohibited area": squares 4.4 mm wide
   between r 14.3 and r 18.2 on the diagonals), plus 0.3 mm margin.
   No tracks or vias there; the GND fill is allowed (the lugs are GND).
3. KNOB_SW: the front piece that ran along the bottom-left lug is removed
   and the net is joined again around the keep-out.
4. NodeMCU 5 V: the branch that took +5V from the far end of the LED-ring
   arc is removed; D5 gets its own 0.8 mm track straight from C21 (the
   buck side), so ESP32 current no longer flows round the ring.
5. D6 -> D7 LED data: the 65 mm detour is removed and routed again.
6. Buck GND pads (U1.4, C1-C3, C5, C6, D2) connect solidly to the zones
   instead of through thermal spokes (shorter hot loop, better cooling).

It refuses to run twice (the keep-outs mark the board as done).
"""
import math
import sys

import pcbnew

import route as R
from route import BOARD, FRONT, BACK, capsule_prim, cell, W, NC

LUG_NAME = "EC35 lug keep-out"
CX, CY = R.CX, R.CY


def P(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def mmv(v):
    return pcbnew.ToMM(v)


def near(a, b, tol=0.02):
    return abs(a[0] - b[0]) < tol and abs(a[1] - b[1]) < tol


def track_ends(t):
    return ((mmv(t.GetStart().x), mmv(t.GetStart().y)),
            (mmv(t.GetEnd().x), mmv(t.GetEnd().y)))


def remove_tracks(board, net, segs=None, vias=None):
    """Delete tracks of `net` whose two ends match one of `segs` (any order),
    or all tracks of the net when segs is None; and vias at `vias`."""
    n = 0
    for t in list(board.GetTracks()):
        if t.GetNetname() != net:
            continue
        if t.Type() == pcbnew.PCB_VIA_T:
            p = (mmv(t.GetPosition().x), mmv(t.GetPosition().y))
            if vias and any(near(p, v) for v in vias):
                board.Delete(t)
                n += 1
            continue
        a, b = track_ends(t)
        if segs is None or any((near(a, s) and near(b, e)) or (near(a, e) and near(b, s))
                               for s, e in segs):
            board.Delete(t)
            n += 1
    return n


def set_fields(board):
    for ref, value, lcsc in (("J1", "DC005-T20 5.5x2.0 3A", "C111567"),
                             ("J2", "RJ45 54602-908LF (no magnetics)", "C2847314"),
                             ("D5", "DSK34", "C727076")):
        fp = board.FindFootprintByReference(ref)
        fp.SetValue(value)
        fp.SetField("LCSC", lcsc)


def widen_jack(board):
    fp = board.FindFootprintByReference("J1")
    for pad in fp.Pads():
        num = pad.GetNumber()
        if num in ("1", "2"):
            size, drill = (3.5 if num == "1" else 3.0, 4.2), (1.0, 3.6)
        else:
            size, drill = (4.2, 3.5), (3.6, 1.0)
        pad.SetSize(pcbnew.F_Cu, P(*size))
        pad.SetDrillSize(P(*drill))
        if num == "2":
            pad.SetFPRelativePosition(P(-6.15, 0))


def add_lug_keepouts(board):
    for k, a in enumerate((45, 135, 225, 315)):
        ux, uy = math.cos(math.radians(a)), math.sin(math.radians(a))
        vx, vy = -uy, ux
        r0, r1, hw = 14.3 - 0.3, 18.2 + 0.3, 2.2 + 0.3
        z = pcbnew.ZONE(board)
        z.SetIsRuleArea(True)
        z.SetLayer(pcbnew.F_Cu)
        z.SetZoneName("%s %d" % (LUG_NAME, k + 1))
        z.SetDoNotAllowTracks(True)
        z.SetDoNotAllowVias(True)
        z.SetDoNotAllowZoneFills(False)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
        o = z.Outline()
        o.NewOutline()
        for r, s in ((r0, -hw), (r1, -hw), (r1, hw), (r0, hw)):
            o.Append(pcbnew.FromMM(CX + ux * r + vx * s), pcbnew.FromMM(CY + uy * r + vy * s))
        board.Add(z)


def add_existing(r, board):
    """Existing tracks and vias become obstacles (and own copper of their net)."""
    for t in board.GetTracks():
        net = t.GetNetname()
        if t.Type() == pcbnew.PCB_VIA_T:
            x, y = mmv(t.GetPosition().x), mmv(t.GetPosition().y)
            prim = capsule_prim(x, y, x, y, mmv(t.GetWidth(pcbnew.F_Cu)) / 2, 3, net)
            r.add_prim(prim)
            r._raster(prim, 1.0, r.padmask, 1)
            ix, iy = cell(x, y)
            r.thru.setdefault(net, set()).add(iy * W + ix)
            continue
        (x1, y1), (x2, y2) = track_ends(t)
        layer = FRONT if t.GetLayer() == pcbnew.F_Cu else BACK
        r.add_prim(capsule_prim(x1, y1, x2, y2, mmv(t.GetWidth()) / 2, 1 << layer, net))


def connect(r, net, k, src, dst, profile, via_ok=True):
    ok = r._route(net, k, r._pad_nodes(dst), profile, via_ok, "%s-%s" % (src, dst),
                  sources=r._pad_nodes(src))
    print("  %-14s %-8s -> %-8s %s %s" % (net, src, dst, k, "ok" if ok else "FAILED"))
    if not ok:
        r.failed.pop()
    return ok


def pad_name(r, ref, net):
    names = [n for n in r.netpads[net] if n.split(".")[0] == ref]
    if len(names) != 1:
        sys.exit("expected one %s pad on %s: %s" % (ref, net, names))
    return names[0]


def main():
    board = pcbnew.LoadBoard(str(BOARD))
    if any(z.GetIsRuleArea() and z.GetZoneName().startswith(LUG_NAME) for z in board.Zones()):
        sys.exit("review fixes already applied")

    set_fields(board)
    widen_jack(board)
    add_lug_keepouts(board)

    # old routing to replace
    n = remove_tracks(board, "+5V", segs=[
        ((129.75, 126.25), (129.75, 127.25)), ((129.75, 127.25), (118.75, 138.25)),
        ((118.75, 138.25), (117.25, 138.25)), ((117.25, 138.25), (115.5, 140.0)),
        ((115.5, 140.0), (115.5, 147.5)), ((115.5, 147.5), (114.5, 148.5)),
        ((114.45, 148.4), (114.5, 148.5))], vias=[(115.5, 147.5)])
    print("removed %d pieces of the NodeMCU 5 V branch" % n)
    n = remove_tracks(board, "Net-(D6-DOUT)")
    print("removed %d pieces of the D6 -> D7 data track" % n)
    n = remove_tracks(board, "/KNOB_SW", segs=[
        ((141.75, 110.25), (141.75, 111.75)), ((141.75, 111.75), (138.75, 114.75)),
        ((138.75, 114.75), (138.25, 114.75))], vias=[(138.25, 114.75)])
    print("removed %d pieces of KNOB_SW at the encoder lug" % n)

    # Router() resets GND pads to "inherited"; keep the hand-set ones
    zc = {(p.GetParentFootprint().GetReference(), p.GetNumber(), p.GetPosition().x,
           p.GetPosition().y): p.GetLocalZoneConnection()
          for fp in board.GetFootprints() for p in fp.Pads()}
    r = R.Router(board)
    add_existing(r, board)

    ok = True
    ok &= connect(r, "Net-(D6-DOUT)", "sig", "D6.2", "D7.4", "front", via_ok=False)
    # KNOB_SW: the SW4 / R13 side joins C17 (which leads on to GPIO34)
    ok &= connect(r, "/KNOB_SW", "sig", pad_name(r, "R13", "/KNOB_SW"),
                  pad_name(r, "C17", "/KNOB_SW"), "back")
    if not connect(r, "+5V", "p5", "C21.1", "D5.2", "any"):
        ok &= connect(r, "+5V", "p3", "C21.1", "D5.2", "any")
    if not ok:
        sys.exit("routing failed, board not saved")
    r.write()

    for fp in board.GetFootprints():
        for p in fp.Pads():
            key = (fp.GetReference(), p.GetNumber(), p.GetPosition().x, p.GetPosition().y)
            if key in zc:
                p.SetLocalZoneConnection(zc[key])
    for ref, num in (("U1", "4"), ("C1", "2"), ("C2", "2"), ("C3", "2"),
                     ("C5", "2"), ("C6", "2"), ("D2", "2")):
        pad = board.FindFootprintByReference(ref).FindPadByNumber(num)
        if pad.GetNetname() != "GND":
            sys.exit("%s.%s is not GND" % (ref, num))
        pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

    R.fill(board)
    board.Save(str(BOARD))
    print("saved; run DRC")


if __name__ == "__main__":
    main()
