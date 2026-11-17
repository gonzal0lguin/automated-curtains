"""Artwork geometry shared by silk.py (KiCad board) and export_svg.py (SVG files).

No KiCad import here: the drawing functions only call a "pen" with
line(a, b, w), fill(points) and text(txt, x, y, h). silk.py passes a pen that
adds silkscreen shapes, export_svg.py one that writes SVG elements.
Units: mm, Y down.
"""
import math


def draw_flag(pen, x0, y0, w, hatch=True, caption=True):
    """Flag of Chile, 3:2, monochrome. White field solid, star solid.
    hatch=True: heraldic hatching, blue canton horizontal lines, red band
    vertical lines (as printed on the boards). hatch=False: outline only.
    caption=True: "Designed / in Chile / by Gonz" under the flag."""
    h = w * 2 / 3
    x1, y1, ym, xc = x0 + w, y0 + h, y0 + h / 2, x0 + h / 2
    for a, b in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
        pen.line(a, b, 0.18)
    pen.fill([(xc, y0), (x1, y0), (x1, ym), (xc, ym)])
    pen.line((x0, ym), (xc, ym), 0.15)
    sx, sy, ro = x0 + h / 4, y0 + h / 4, h / 8
    star = []
    for i in range(10):
        r = ro if i % 2 == 0 else ro * 0.382
        a = -math.pi / 2 + i * math.pi / 5
        star.append((sx + r * math.cos(a), sy + r * math.sin(a)))
    pen.fill(star)
    if not hatch:
        pen.line((xc, y0), (xc, ym), 0.15)       # canton edge, drawn by the hatching otherwise
    else:
        keep = ro + 0.3
        n = max(2, int(round((h / 2) / 0.45)))
        for i in range(1, n):
            y = y0 + i * (h / 2) / n
            dy = abs(y - sy)
            if dy < keep:
                dx = math.sqrt(keep * keep - dy * dy)
                if sx - dx - x0 > 0.3:
                    pen.line((x0, y), (sx - dx, y))
                if xc - sx - dx > 0.3:
                    pen.line((sx + dx, y), (xc, y))
            else:
                pen.line((x0, y), (xc, y))
        n = int(round(w / 0.45))
        for i in range(1, n):
            x = x0 + i * w / n
            pen.line((x, ym), (x, y1))
    if caption:
        cx = x0 + w / 2
        pen.text("Designed", cx, y1 + 1.2, 0.8)
        pen.text("in Chile", cx, y1 + 2.4, 0.8)
        pen.text("by Gonz", cx, y1 + 3.6, 0.8)
    return (x0 - 0.1, y0 - 0.1, x1 + 0.1, (y1 + 4.2) if caption else (y1 + 0.1))


def emblem_parts(data):
    """Split emblem_small.json polygons: (picture + frame, title + rules).
    The title line lies wholly below the frame (data["box"][1] + frame height)."""
    polys = data["polys"]
    frame_bottom = max(max(p[1] for p in poly[0]) for poly in polys
                       if min(p[0] for p in poly[0]) <= data["box"][0] + 1e-6)
    pic = [p for p in polys if min(q[1] for q in p[0]) < frame_bottom - 1e-6]
    title = [p for p in polys if min(q[1] for q in p[0]) >= frame_bottom - 1e-6]
    return pic, title, frame_bottom
