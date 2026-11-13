"""Simple 3D models for parts KiCad ships no model for (height checks only).

Origin = footprint origin, X right, Y up (KiCad 3D convention: Y = -footprint
Y). Run with a Python that has CadQuery:  python make_models.py

- TMC2209_BTT_V1.3: StepStick with pin headers; Z = 0 is the underside of the
  header plastic, which sits on the socket top. Heatsink size is assumed.
- RJ45_8P8C_block: envelope of the 54602-type jack (13 mm high, assumed).
- Fuse_1812_block, TS-1187A_block: plain boxes.
"""
import cadquery as cq
from pathlib import Path

P = 2.54
pcb = cq.Workplane("XY").box(15.24, 20.32, 1.6, centered=(True, True, False)) \
    .translate((6.35, -8.89, 2.5))
parts = [pcb]
for x in (0, 12.7):
    parts.append(cq.Workplane("XY").box(2.5, 20.32, 2.5, centered=(True, True, False))
                 .translate((x, -8.89, 0)))
    for i in range(8):
        parts.append(cq.Workplane("XY").box(0.64, 0.64, 6.0, centered=(True, True, False))
                     .translate((x, -i * P, -6.0)))
# stick-on heatsink (assumed 9 x 9 x 5 mm) and the trim pot
parts.append(cq.Workplane("XY").box(9, 9, 5, centered=(True, True, False))
             .translate((6.35, -8.0, 4.1)))
parts.append(cq.Workplane("XY").circle(1.5).extrude(1.5).translate((6.35, -17.0, 4.1)))
solid = parts[0]
for p in parts[1:]:
    solid = solid.union(p)
LIB = Path(__file__).resolve().parents[2] / "lib/3d"


def save(shape, name):
    cq.exporters.export(shape, str(LIB / name))
    print("wrote", name)


save(solid, "TMC2209_BTT_V1.3.step")

# RJ45: footprint fab outline x -3.27..12.15, y -3.83..14.03 (opening at +y)
rj = (cq.Workplane("XY").box(15.42, 17.86, 13.0, centered=False)
      .translate((-3.27, -14.03, 0)))
rj = rj.cut(cq.Workplane("XY").box(11.7, 10.0, 8.0, centered=False)
            .translate((4.44 - 5.85, -14.03, 2.5)))
save(rj, "RJ45_8P8C_block.step")

save(cq.Workplane("XY").box(4.5, 3.2, 1.0, centered=(True, True, False)),
     "Fuse_1812_block.step")
tact = (cq.Workplane("XY").box(5.1, 5.1, 1.2, centered=(True, True, False))
        .union(cq.Workplane("XY").circle(1.5).extrude(1.5)))
save(tact, "TS-1187A_block.step")
