#!/usr/bin/env python3
"""Writes the motor board artwork as SVG files for reuse (../../artwork/).

Plain Python, no KiCad needed. Run after emblem_small.py:

    python3 tools/export_svg.py

Files (black on transparent, 1 SVG unit = 1 mm, the size printed on the board):
    emblem_small.svg          volcano emblem with the "ANDES MADE" title
    emblem_small_notext.svg   the same, frame and picture only
    flag.svg                  flag of Chile with hatching + "Designed in Chile by Gonz"
    flag_plain.svg            the same without the hatching lines
"""
import json
from pathlib import Path

from artwork import draw_flag, emblem_parts

HERE = Path(__file__).resolve().parent
OUT = HERE.parents[1] / "artwork"
INK = "#000"


class SvgPen:
    def __init__(self):
        self.items = []

    def line(self, a, b, w=0.15):
        self.items.append('<line x1="%.4f" y1="%.4f" x2="%.4f" y2="%.4f" stroke-width="%.3f"/>' % (*a, *b, w))

    def fill(self, pts):
        self.items.append('<polygon points="%s" stroke="none"/>' % " ".join("%.4f,%.4f" % tuple(p) for p in pts))

    def path(self, poly):
        d = " ".join("M" + " L".join("%.4f,%.4f" % tuple(p) for p in c) + " Z" for c in poly)
        self.items.append('<path d="%s" stroke="none" fill-rule="evenodd"/>' % d)

    def text(self, txt, x, y, h=0.8):
        # KiCad stroke font: h = cap height; a sans font with cap ~0.72 em
        self.items.append('<text x="%.4f" y="%.4f" font-size="%.3f" stroke="none" text-anchor="middle" '
                          'dominant-baseline="central" font-family="DejaVu Sans, Arial, sans-serif">%s</text>'
                          % (x, y, h / 0.72, txt))

    def write(self, path, box, title):
        x0, y0, x1, y1 = box
        w, h = x1 - x0, y1 - y0
        body = "\n  ".join(self.items)
        path.write_text(
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<svg xmlns="http://www.w3.org/2000/svg" width="%.3fmm" height="%.3fmm" viewBox="%.4f %.4f %.4f %.4f">\n'
            '<title>%s</title>\n'
            '<g fill="%s" stroke="%s" stroke-linecap="round">\n  %s\n</g>\n</svg>\n'
            % (w, h, x0, y0, w, h, title, INK, INK, body))
        print("%-26s %5.1f x %4.1f mm, %d elements" % (path.name, w, h, len(self.items)))


def main():
    OUT.mkdir(exist_ok=True)
    data = json.loads((HERE / "emblem_small.json").read_text())
    pic, title, frame_bottom = emblem_parts(data)
    x0, y0, x1, y1 = data["box"]
    for name, polys, box in (("emblem_small.svg", pic + title, (x0, y0, x1, y1 + 0.05)),
                             ("emblem_small_notext.svg", pic, (x0, y0, x1, frame_bottom))):
        pen = SvgPen()
        for poly in polys:
            pen.path(poly)
        pen.write(OUT / name, box, "Andes made - volcano emblem")
    for name, hatch in (("flag.svg", True), ("flag_plain.svg", False)):
        pen = SvgPen()
        box = draw_flag(pen, 0, 0, 7.2, hatch=hatch)
        pen.write(OUT / name, box, "Flag of Chile")


if __name__ == "__main__":
    main()
