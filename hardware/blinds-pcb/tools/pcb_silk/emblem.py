#!/usr/bin/env python3
"""Front emblem: Lonquimay, Llaima, Villarrica and Osorno rendered from real
elevation data, as a line engraving for the silkscreen, plus the title.

Writes tools/pcb_silk/emblem.json (filled polygons in board mm), which
silk.py draws. Run this only to change the artwork; the JSON is committed.

It needs numpy, scipy, tifffile, imagecodecs, opencv-python-headless,
fonttools and pillow, so it runs in its own environment, not KiCad's Python:

    uv venv /tmp/emblem-env
    uv pip install -p /tmp/emblem-env numpy scipy tifffile imagecodecs \\
        opencv-python-headless fonttools pillow
    /tmp/emblem-env/bin/python tools/pcb_silk/emblem.py --dem ~/.cache/dem \\
        --preview /tmp/emblem.png
    python3 tools/pcb_silk/silk.py --emblem      # KiCad closed

How the picture is made:

1. Elevation: Copernicus DEM GLO-90 (90 m, free), downloaded once into --dem.
   A 15 km patch round each summit is placed in one made-up landscape, close
   together, in their real order north to south (left to right when looking
   east from the Pacific side). Heights are true, then drawn 3.3x taller,
   because 25 km wide cones would be flat strips at this size.
2. Snow lies above a snow line per volcano, reaches lower in gullies and
   leaves rock ribs bare; steep faces stay bare.
3. A perspective render from the west, lit from the north-west (afternoon
   in the southern hemisphere).
4. Engraving: lit snow is solid white; the sky is horizontal lines, heavier
   towards the horizon, with Villarrica's plume as brighter lines; the
   forest below the snow stays dark; a lake in front carries ripples and a
   broken reflection.
5. Silkscreen limits: every line and every gap is at least 0.16 mm; dots
   smaller than that are removed (JLCPCB asks 0.153 mm).
"""
import argparse
import json
import math
import urllib.request
from pathlib import Path

import cv2
import numpy as np
import tifffile
from fontTools.pens.basePen import BasePen
from fontTools.ttLib import TTFont
from PIL import Image, ImageChops, ImageDraw
from scipy import ndimage as ndi

HERE = Path(__file__).resolve().parent
OUT = HERE / "emblem.json"

# ----------------------------------------------------------------- layout (board mm)
FRAME = (142.5, 48.4, 189.5, 59.8)     # outer edge of the frame line; top clears Q1
FRAME_R = 1.4
FRAME_W = 0.18
FRAME_GAP = 0.3                       # frame line to picture
XC = (FRAME[0] + FRAME[2]) / 2
TITLE = "ANDES MADE"
TITLE_FONT = "/usr/share/fonts/truetype/lato/Lato-Bold.ttf"
TITLE_CAP, TITLE_TRACK, TITLE_BASE = 1.8, 0.42, 62.55
NAMES = "LONQUIMAY · LLAIMA · VILLARRICA · OSORNO"
NAMES_FONT = "/usr/share/fonts/truetype/lato/Lato-Bold.ttf"
NAMES_CAP, NAMES_TRACK, NAMES_BASE = 0.95, 0.16, 64.5
RULE_W, RULE_GAP = 0.18, 1.0          # rules either side of the title

# ----------------------------------------------------------------- picture
PX = 40.0                              # raster pixels per mm
MIN = 0.16                             # minimum line and gap (JLCPCB: 0.153)
PITCH = 0.38                           # engraving line pitch
SX0, SY0 = FRAME[0] + FRAME_W + FRAME_GAP, FRAME[1] + FRAME_W + FRAME_GAP
SX1, SY1 = FRAME[2] - FRAME_W - FRAME_GAP, FRAME[3] - FRAME_W - FRAME_GAP
W, H = int(round((SX1 - SX0) * PX)), int(round((SY1 - SY0) * PX))
LAKE = 2.5                             # lake height at the bottom, mm

TILE_URL = ("https://copernicus-dem-90m.s3.amazonaws.com/"
            "Copernicus_DSM_COG_30_{0}_DEM/Copernicus_DSM_COG_30_{0}_DEM.tif")
# name, summit lat, lon (approx., refined to the highest point), DEM tile,
# snow line (m)
VOLC = [("Lonquimay", -38.377, -71.586, "S39_00_W072_00", 1750),
        ("Llaima", -38.692, -71.729, "S39_00_W072_00", 1700),
        ("Villarrica", -39.420, -71.939, "S40_00_W072_00", 1400),
        ("Osorno", -41.100, -72.493, "S42_00_W073_00", 1200)]
# placement in the made-up landscape: (north km, east km); camera is west
PLACE = {"Lonquimay": (15.5, 6.0), "Llaima": (5.0, 2.0),
         "Villarrica": (-5.2, 4.0), "Osorno": (-14.6, 1.0)}
PATCH_R = {"Lonquimay": 10.0}         # km, default 15 (keeps Tolhuaca out)
G = 0.075                              # landscape grid, km
EXAG = 3.3                             # vertical exaggeration
CAM = (-60.0, 0.5, 0.6)                # camera east, north (km), height (km x EXAG)
SPAN = 43.0                            # km across the picture at 60 km
SUN = (-0.70, 0.50, 0.45)              # towards the sun: west, north, up
SHORE = -8.0                           # lake in front of this east coordinate


# ======================================================================= terrain

def load_tile(dem, name):
    path = dem / (name + ".tif")
    if not path.exists():
        dem.mkdir(parents=True, exist_ok=True)
        print("downloading", name)
        urllib.request.urlretrieve(TILE_URL.format(name), path)
    with tifffile.TiffFile(path) as t:
        p = t.pages[0]
        tags = {k.name: k.value for k in p.tags.values()}
        tp, sc = tags["ModelTiepointTag"], tags["ModelPixelScaleTag"]
        return p.asarray().astype(np.float64), tp[3], tp[4], sc[0]


def patch(dem, lat, lon, tile, r_km):
    a, lon0, lat0, s = load_tile(dem, tile)

    def grid(la0, lo0, n):
        yy, xx = np.mgrid[n:-n - 1:-1, -n:n + 1] * G           # row 0 = north
        la = la0 + yy / 111.32
        lo = lo0 + xx / (111.32 * math.cos(math.radians(la0)))
        return la, lo, ndi.map_coordinates(a, [(lat0 - la) / s - 0.5, (lo - lon0) / s - 0.5],
                                           order=1, mode="nearest")
    la, lo, z = grid(lat, lon, int(3.0 / G))                    # find the summit
    k = np.unravel_index(np.argmax(z), z.shape)
    return grid(la[k], lo[k], int(r_km / G))[2], z.max()


def landscape(dem):
    ys = np.arange(40.0, -40.0, -G)
    xs = np.arange(-20.0, 28.0, G)
    Z = np.zeros((len(ys), len(xs)))        # height above the lake, m
    SN = np.zeros_like(Z)                   # snow
    for name, lat, lon, tile, snowline in VOLC:
        p, top = patch(dem, lat, lon, tile, 15.0)
        n = p.shape[0] // 2
        base = np.percentile(p, 4)
        yy, xx = np.mgrid[n:-n - 1:-1, -n:n + 1] * G
        r = np.hypot(yy, xx) / PATCH_R.get(name, 15.0)
        w = np.clip(np.where(r < 0.5, 1.0, 0.5 + 0.5 * np.cos(math.pi * (r - 0.5) / 0.5)), 0, 1)
        w[r > 1] = 0
        rel = np.clip(p - base, 0, None) * w
        ty, tx = np.gradient(p / 1000.0, G)
        slope = np.degrees(np.arctan(np.hypot(tx, ty)))
        gully = ndi.laplace(ndi.gaussian_filter(p, 1.2))       # > 0 in gullies
        rng = np.random.default_rng(len(name))
        noise = ndi.gaussian_filter(rng.standard_normal(p.shape), 2) * 300
        sn = ((p + gully * 4.0 + noise) > snowline) & (slope < 42)
        sn = ndi.binary_opening(sn, iterations=1) & (w > 0.8)
        yc, xc = PLACE[name]
        iy = int(round((ys[0] - (yc + n * G)) / G))
        ix = int(round(((xc - n * G) - xs[0]) / G))
        sl = (slice(iy, iy + p.shape[0]), slice(ix, ix + p.shape[1]))
        take = rel > Z[sl]
        Z[sl] = np.where(take, rel, Z[sl])
        SN[sl] = np.where(take, sn, SN[sl])
        print("%-10s summit %4.0f m, base %4.0f m" % (name, top, base))
    shore = SHORE + 1.2 * np.sin(ys / 4.1) + 0.8 * np.sin(ys / 1.7 + 1.0)
    lake = xs[None, :] < shore[:, None]
    Z[lake] = 0.0
    return xs, ys, Z, SN


def render(xs, ys, Z, SN):
    """Perspective view from the west. Per pixel: sky, height, light, snow."""
    Xc, Yc, Hc = CAM
    f = W * 60.0 / SPAN
    cy = H - 0.6 * PX - f * Hc / 60.0
    gy, gx = np.gradient(Z / 1000.0 * EXAG, G)
    nrm = np.stack([-gx, gy, np.ones_like(Z)])                  # rows run south
    nrm /= np.linalg.norm(nrm, axis=0)
    L = np.array(SUN) / np.linalg.norm(SUN)
    light = np.clip(np.tensordot(L, nrm, 1), 0, 1)
    us = np.arange(W) + 0.5
    ds = np.arange(xs[0] - Xc, xs[-1] - Xc, 0.03)
    Yw = Yc - (us[None, :] - W / 2) * ds[:, None] / f
    r = (ys[0] - Yw) / G
    c = (Xc + ds[:, None] - xs[0]) / G * np.ones((1, W))
    z, li, sn = (ndi.map_coordinates(F, [r, c], order=1, mode="constant", cval=0.0)
                 for F in (Z, light, SN))
    v = cy - f * (z / 1000.0 * EXAG - Hc) / ds[:, None]
    run = np.minimum.accumulate(v, axis=0)                      # skyline so far
    rows = np.arange(H) + 0.5
    idx = np.empty((H, W), int)
    for j in range(W):
        idx[:, j] = np.searchsorted(-run[:, j], -rows, side="left")
    sky = idx >= len(ds)
    ii = np.where(sky, 0, idx)
    jj = np.broadcast_to(np.arange(W), (H, W))
    summits = [W / 2 - (PLACE[n][0] - Yc) * f / (PLACE[n][1] - Xc) for n, *_ in VOLC]
    return sky, z[ii, jj], li[ii, jj], sn[ii, jj] > 0.5, summits


# ======================================================================= engraving

def engrave(sky, z, light, snow, summits):
    rng = np.random.default_rng(5)
    rows = np.arange(H)[:, None] + 0.5
    cols = np.arange(W)[None, :] + 0.5
    P = PITCH * PX
    wl = H - int(LAKE * PX)                                     # waterline
    land = ~sky
    land[wl:] = False
    sky = sky.copy()
    sky[wl:] = False
    water = np.zeros_like(sky)
    water[wl:H - int(0.25 * PX)] = True                       # clear of the frame

    def lines(width_mm, phase=0.0):
        y = rows / P + phase
        return np.abs(y - np.round(y)) * P < np.asarray(width_mm) * PX / 2

    # snow: white, except deep shadow; stray patches lower down are dropped
    snow_ink = land & snow & (light > 0.12)
    lab, n = ndi.label(snow_ink)
    size = ndi.sum(snow_ink, lab, np.arange(1, n + 1))
    snow_ink &= np.isin(lab, 1 + np.flatnonzero(size >= 0.8 * PX * PX))
    # ravines: long streaks where the snow is darker than round it
    local = light - ndi.gaussian_filter(light, 0.25 * PX)
    rav = ndi.binary_opening(local < -0.12, np.ones((int(0.6 * PX), 3), bool))
    ink = snow_ink & ~rav
    # Villarrica's plume: rises from the crater and drifts away downwind
    vx = summits[2]
    vy = np.argmax(land[:, int(vx)])
    s = np.linspace(0, 1, 300)
    px = vx + 0.1 * PX + s * 9.0 * PX
    py = vy - 0.15 * PX - 1.0 * PX * (1 - np.exp(-4 * s)) - 0.15 * PX * s
    pw = 0.12 * PX + s * 0.65 * PX
    plume = np.zeros((H, W))
    for x, y, w, a in zip(px, py, pw, (1 - s) ** 1.2):
        g = np.exp(-(((cols - x) / (2.2 * w)) ** 2 + ((rows - y) / w) ** 2) / 2)
        plume = np.maximum(plume, a * g)

    # sky: continuous lines, heavier towards the horizon and in the plume
    t = np.clip(rows / wl, 0, 1)
    sw = np.minimum(MIN + 0.07 * t ** 1.5 + 0.12 * np.clip(plume * 1.6, 0, 1), PITCH - MIN)
    halo = ndi.binary_dilation(land, iterations=int(0.22 * PX))
    ink |= sky & ~halo & lines(sw)

    # lake: dark under the shore (the forest reflected), then ripples that
    # grow longer towards the viewer, fading at both ends; the snow reflected
    k = np.clip((rows - wl) / (H - wl), 0, 1)
    vig = np.clip(np.minimum(cols, W - cols) / (6.0 * PX), 0, 1) ** 0.8
    n1 = ndi.gaussian_filter(rng.standard_normal((H, W)), (0.4, 18))
    n2 = ndi.gaussian_filter(rng.standard_normal((H, W)), (0.4, 60))
    dash = (n1 * 0.6 + n2 * 4.0) > (0.35 - 0.9 * k)
    ww = np.where(dash, (MIN + 0.06 * k) * (0.5 + 0.5 * vig), 0.0)
    ww = np.where(ww < MIN, 0.0, ww)
    ink |= water & (rows > wl + 0.3 * PX) & lines(ww, 0.5)
    mr = np.clip(2 * wl - rows.astype(int), 0, H - 1)
    refl = np.take_along_axis(snow_ink, np.broadcast_to(mr, (H, W)), 0)
    rd = ndi.gaussian_filter(rng.standard_normal((H, W)), (0.4, 8)) > -0.3
    ink |= water & refl & rd & lines(0.2, 0.5)

    # silkscreen limits: remove anything thinner than MIN, fill gaps under MIN
    kk = int(round(MIN * PX))
    st = np.ones((kk, kk), bool)
    ink = ndi.binary_closing(ndi.binary_opening(ink, st), st)
    # the picture sits inside the rounded frame
    clip = np.zeros((H, W), np.uint8)
    rr = int((FRAME_R - FRAME_W - FRAME_GAP) * PX)
    cv2.rectangle(clip, (rr, 0), (W - 1 - rr, H - 1), 1, -1)
    cv2.rectangle(clip, (0, rr), (W - 1, H - 1 - rr), 1, -1)
    for cx_, cy_ in ((rr, rr), (W - 1 - rr, rr), (rr, H - 1 - rr), (W - 1 - rr, H - 1 - rr)):
        cv2.circle(clip, (cx_, cy_), rr, 1, -1)
    return ink & clip.astype(bool)


def vectorise(ink):
    """Binary image -> [[outline, hole, ...], ...] in board mm."""
    up = 4
    big = cv2.resize(ink.astype(np.float32), None, fx=up, fy=up, interpolation=cv2.INTER_LINEAR)
    big = (cv2.GaussianBlur(big, (0, 0), up * 0.6) > 0.5).astype(np.uint8)
    cnts, hier = cv2.findContours(big, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
    s = 1.0 / (PX * up)

    def mm(c):
        c = cv2.approxPolyDP(c, up * 0.3, True)[:, 0, :].astype(float)
        return [(round(SX0 + (x + 0.5) * s, 4), round(SY0 + (y + 0.5) * s, 4)) for x, y in c]
    polys = []
    for i, c in enumerate(cnts):
        if hier[0][i][3] != -1 or cv2.contourArea(c) * s * s < MIN * MIN:
            continue
        poly = [mm(c)]
        j = hier[0][i][2]
        while j != -1:
            if cv2.contourArea(cnts[j]) * s * s >= MIN * MIN:
                poly.append(mm(cnts[j]))
            j = hier[0][j][0]
        if len(poly[0]) >= 3:
            polys.append(poly)
    return polys


# ======================================================================= text and frame

class FlatPen(BasePen):
    def __init__(self, glyphs):
        super().__init__(glyphs)
        self.contours, self.cur = [], []

    def _moveTo(self, p):
        self.cur = [p]

    def _lineTo(self, p):
        self.cur.append(p)

    def _curveToOne(self, p1, p2, p3):
        p0 = self.cur[-1]
        for i in range(1, 9):
            t = i / 8
            a, b, c, d = (1 - t) ** 3, 3 * t * (1 - t) ** 2, 3 * t * t * (1 - t), t ** 3
            self.cur.append((a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0],
                             a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]))

    def _qCurveToOne(self, p1, p2):
        p0 = self.cur[-1]
        for i in range(1, 7):
            t = i / 6
            a, b, c = (1 - t) ** 2, 2 * t * (1 - t), t * t
            self.cur.append((a * p0[0] + b * p1[0] + c * p2[0], a * p0[1] + b * p1[1] + c * p2[1]))

    def _closePath(self):
        if len(self.cur) >= 3:
            self.contours.append(self.cur)
        self.cur = []

    _endPath = _closePath


def area(c):
    return 0.5 * sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(c, c[1:] + c[:1]))


def inside(p, c):
    x, y, n = p[0], p[1], False
    for (x0, y0), (x1, y1) in zip(c, c[1:] + c[:1]):
        if (y0 > y) != (y1 > y) and x < x0 + (x1 - x0) * (y - y0) / (y1 - y0):
            n = not n
    return n


def nest(contours):
    """Outer contours with their holes (even-odd nesting)."""
    contours = sorted(contours, key=lambda c: -abs(area(c)))
    depth = [sum(inside(c[0], o) for o in contours[:i]) for i, c in enumerate(contours)]
    polys = []
    for c, d in zip(contours, depth):
        if d % 2 == 0:
            polys.append([c])
        else:
            for p in polys[::-1]:
                if inside(c[0], p[0]):
                    p.append(c)
                    break
    return polys


def text(s, font, cap, track, xc, base):
    """Text centred on xc, baseline at base; returns (polys, x0, x1)."""
    ft = TTFont(font)
    gs, cmap, hmtx = ft.getGlyphSet(), ft.getBestCmap(), ft["hmtx"]
    k = cap / ft["OS/2"].sCapHeight
    em = ft["head"].unitsPerEm
    glyphs, x = [], 0.0
    for ch in s:
        g = cmap[ord(ch)]
        glyphs.append((g, x))
        x += hmtx[g][0] + track * em
    x -= track * em
    x0 = xc - x * k / 2
    contours = []
    for g, gx in glyphs:
        pen = FlatPen(gs)
        gs[g].draw(pen)
        for c in pen.contours:
            contours.append([(round(x0 + (gx + px) * k, 4), round(base - py * k, 4)) for px, py in c])
    return nest(contours), x0, x0 + x * k


def round_rect(x0, y0, x1, y1, r, n=10):
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0),
                       (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        pts += [(round(cx + r * math.cos(math.radians(a0 + 90 * i / n)), 4),
                 round(cy + r * math.sin(math.radians(a0 + 90 * i / n)), 4)) for i in range(n + 1)]
    return pts


def frame_and_text():
    x0, y0, x1, y1 = FRAME
    w = FRAME_W
    polys = [[round_rect(x0, y0, x1, y1, FRAME_R),
              round_rect(x0 + w, y0 + w, x1 - w, y1 - w, FRAME_R - w)[::-1]]]
    title, tx0, tx1 = text(TITLE, TITLE_FONT, TITLE_CAP, TITLE_TRACK, XC, TITLE_BASE)
    names, _, _ = text(NAMES, NAMES_FONT, NAMES_CAP, NAMES_TRACK, XC, NAMES_BASE)
    ym = TITLE_BASE - TITLE_CAP / 2
    for a, b in ((x0 + FRAME_R, tx0 - RULE_GAP), (tx1 + RULE_GAP, x1 - FRAME_R)):
        polys.append([[(a, ym - RULE_W / 2), (b, ym - RULE_W / 2),
                       (b, ym + RULE_W / 2), (a, ym + RULE_W / 2)]])
    return polys + title + names


# ======================================================================= main

def preview(polys, path, scale=24):
    bx0, by0, bx1, by1 = FRAME[0] - 1.5, FRAME[1] - 1.5, FRAME[2] + 1.5, 65.5
    size = (int((bx1 - bx0) * scale), int((by1 - by0) * scale))
    img = Image.new("L", size, 0)
    for poly in polys:
        one = Image.new("L", size, 0)
        d = ImageDraw.Draw(one)
        for i, c in enumerate(poly):
            d.polygon([((x - bx0) * scale, (y - by0) * scale) for x, y in c], fill=0 if i else 255)
        img = ImageChops.lighter(img, one)
    rgb = Image.merge("RGB", [img.point(lambda v: 30 + v * 225 // 255),
                              img.point(lambda v: 70 + v * 185 // 255),
                              img.point(lambda v: 45 + v * 210 // 255)])
    rgb.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dem", type=Path, required=True, help="folder for the DEM tiles")
    ap.add_argument("--preview", type=Path, help="write a PNG preview here")
    a = ap.parse_args()
    xs, ys, Z, SN = landscape(a.dem.expanduser())
    ink = engrave(*render(xs, ys, Z, SN))
    polys = vectorise(ink) + frame_and_text()
    OUT.write_text(json.dumps({"note": "generated by emblem.py, do not edit",
                               "polys": polys}, separators=(",", ":")))
    print("%d polygons, %d points -> %s" % (len(polys), sum(len(c) for p in polys for c in p), OUT))
    if a.preview:
        preview(polys, a.preview)


if __name__ == "__main__":
    main()
