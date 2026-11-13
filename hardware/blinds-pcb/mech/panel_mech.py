#!/usr/bin/env python3
"""Centre mechanics of the blinds control panel: screen carrier + push button.

Parametric CadQuery model. Edit the values in ``P`` and re-run:

    uv venv .venv && uv pip install --python .venv/bin/python cadquery matplotlib
    .venv/bin/python mech/panel_mech.py

How it works
    tower   fixed. Screwed to the PCB inside the encoder bore. Guides the
            riser over its full height and is the hard stop for a press.
    riser   moves. Plate + hub + stem. The display is screwed to the plate,
            the stem presses the tact switch.
    3 x M3x25 screws go in from the back of the PCB, slide through the tower
            and thread into nyloc nuts captured in the riser. Their heads on
            the back of the PCB stop the riser from being pulled out, and
            turning them sets the rest position (no play, no shims).
    bezel   snaps over the display. knob: clamps on the encoder rotor.

Outputs go to ``mech/out/``:
    riser.step, tower.step, bezel.step, knob.step   printed parts (one body each)
    panel_mech_assembly.step                        everything, with reference parts
    section.png                                     cut through the knob axis

Frame (same as a KiCad footprint's 3D frame, seen from the front):
    X right, Y up, Z toward the user. Z = 0 is the front face of the main PCB,
    the origin is the knob axis. All parts are drawn in the REST position
    (button not pressed). Units: mm.
"""
from math import cos, sin, radians, hypot
from pathlib import Path
from types import SimpleNamespace

import cadquery as cq

HERE = Path(__file__).resolve().parent
OUT = HERE / "out"
DISPLAY_STEP = (HERE.parent / "jxl-1-28-inch-ips-colour-tft-lcd-display-module"
                / "1.28 inch IPS Color TFT LCD Display Module v11.step")
ENCODER_STEP = HERE.parent / "lib" / "3d" / "EC35A1520402.step"

# --------------------------------------------------------------------------
# Parameters
# --------------------------------------------------------------------------
P = SimpleNamespace(
    # --- main PCB -----------------------------------------------------------
    pcb_t=1.6,

    # --- ALPS EC35A1520402 (datasheet + STEP model) ---------------------------
    enc_bore_d=23.7,        # clear bore through the rotor
    enc_rotor_top=20.0,     # top of the rotating sleeve above the PCB
    enc_body_d=35.0,

    # --- centre tact switch (TS-1187A, 5.1 x 5.1 x 1.5) ----------------------
    sw_h=1.5,               # PCB to top of the un-pressed actuator
    sw_pocket_x=6.0,        # pocket in the foot; the switch is rotated 90 deg,
    sw_pocket_y=7.8,        #   so its pads are on the +Y / -Y sides
    nub_d=2.4,              # tip of the stem that presses the actuator
    nub_h=0.8,

    # --- button travel --------------------------------------------------------
    travel=0.4,             # rest gap hub -> tower top (hard stop). Switch clicks at ~0.25

    # --- tower: fixed guide inside the encoder bore (printed) ------------------
    tower_d=21.5,           # 1.1 mm radial clearance in the rotating bore
    mount_r=7.5,            # 2 x M3x8 from the back into heat-set inserts
    mount_angles=(30, 150),
    mount_pcb_hole_d=3.2,   # NPTH in the main PCB
    mount_screw_len=8.0,
    insert_hole_d=4.0,      # M3 heat-set insert (OD 4.6, L 5.7) in the tower base
    insert_hole_depth=7.0,
    stem_bore_d=5.8,

    # --- riser: plate + hub + stem (one printed part) ---------------------------
    plate_gap=1.0,          # rotor top -> plate underside, at rest
    plate_t=2.5,
    plate_d=47.0,           # covers the display tab corners (r 23.1) + 0.4
    plate_edge_chamfer=0.5,
    tube_plate_chamfer=0.6,
    hub_t=3.5,              # hub under the plate, same diameter as the tower
    stem_d=5.0,

    # --- retention / guide screws: 3 x M3x25 from the back ----------------------
    guide_r=7.0,
    guide_angles=(90, 210, 330),
    guide_pcb_hole_d=3.4,   # NPTH in the main PCB
    guide_bore_d=3.3,       # bore in the tower (ream to a sliding fit)
    ret_screw_len=25.0,
    ret_washer_t=0.5,       # M3 washer under the head, on the back of the PCB
    nut_af=5.5,             # M3 nyloc nut (DIN 985), captured in the riser
    nut_h=4.0,
    nut_fit=0.2,            # pocket is this much wider than the nut
    nut_floor=1.5,          # plastic under the nut

    # --- wire path (8 wires, crimps passed one by one) ------------------------
    wire_slot_len=7.0,      # NPTH oval in the main PCB
    wire_slot_w=3.0,
    wire_slot_r=7.25,       # slot centre from the axis
    wire_slot_angle=270,
    wire_window_extra=0.4,  # tower / riser channels are this much larger

    # --- knob (printed, clamps on the encoder rotor) ----------------------------
    rotor_od=28.8,
    knob_fit=0.2,           # diametral clearance hub -> rotor
    knob_hub_wall=2.4,
    knob_wall=2.0,
    knob_top_t=2.0,
    knob_z0=1.5,            # skirt bottom above the PCB (hides the encoder body)
    knob_gap=0.5,           # knob top -> bezel skirt
    knob_rib_angles=(90, 210, 330),
    knob_rib_w=7.0,
    knob_rib_z0=8.0,
    knob_screw_z=13.0,      # 3 x M3x6 set screws, radial, onto the rotor
    knob_thread_d=2.6,      # self-tapped in the rib
    knob_access_d=3.4,

    # --- display module (JXL 1.28" GC9A01, measured on the STEP model) ---------
    # module-local: +Y_cad is "towards the JST", the flex tab points to -Y_cad
    mod_standoffs=((13.5, 6.0), (-13.5, 6.0), (13.5, -6.0), (-13.5, -6.0)),
    mod_standoff_len=5.99,  # behind the module PCB
    mod_pcb_t=1.51,
    mod_glass_t=1.54,
    mod_active_d=32.4,
    mod_screw_d=3.4,        # M3x6 countersunk, from under the plate
    mod_csk_d=6.4,

    # --- bezel (printed cap over the display) ----------------------------------
    bezel_fit=0.15,         # radial clearance bezel -> plate
    bezel_wall=1.4,
    bezel_lip_t=1.0,
    bezel_glass_gap=0.2,
    bezel_window_d=33.0,
    bezel_skirt_below=0.9,  # skirt length below the plate underside
    rib_angles=(0, 135, 225, 315),   # ribs that sit on the plate
    rib_w=5.0,
    rib_depth=2.75,         # radial, from the bezel wall inward
    snap_angles=(45, 135, 225, 315),
    snap_w=6.0,
    snap_h=0.35,            # how far the snap bumps stand out of the wall
)

# --- derived ---------------------------------------------------------------
P.plate_z0 = P.enc_rotor_top + P.plate_gap            # plate underside
P.plate_z1 = P.plate_z0 + P.plate_t                   # plate top = standoff seats
P.mod_pcb_z0 = P.plate_z1 + P.mod_standoff_len        # module PCB back
P.mod_z_off = P.mod_pcb_z0 + P.mod_pcb_t              # module PCB front
P.glass_top = P.mod_z_off + P.mod_glass_t
P.lip_z0 = P.glass_top + P.bezel_glass_gap
P.bezel_top = P.lip_z0 + P.bezel_lip_t
P.bezel_ri = P.plate_d / 2 + P.bezel_fit
P.bezel_ro = P.bezel_ri + P.bezel_wall
P.hub_z0 = P.plate_z0 - P.hub_t                       # hub underside
P.tower_top = P.hub_z0 - P.travel
P.nut_z0 = P.hub_z0 + P.nut_floor                     # nut seat
P.ret_head_z = -P.pcb_t - P.ret_washer_t              # under the screw head, at rest
P.knob_ro = P.bezel_ro
P.knob_z1 = P.plate_z0 - P.bezel_skirt_below - P.knob_gap


def polar(r, deg):
    return (r * cos(radians(deg)), r * sin(radians(deg)))


def cyl(d, z0, z1, xy=(0, 0)):
    return (cq.Workplane("XY").workplane(offset=z0).center(*xy)
            .circle(d / 2).extrude(z1 - z0))


def oval(length, width, z0, z1, r, angle):
    """Oval prism, long axis tangential, centred at polar (r, angle)."""
    s = (cq.Workplane("XY").workplane(offset=z0).center(r, 0)
         .slot2D(length, width, 90).extrude(z1 - z0))
    return s.rotate((0, 0, 0), (0, 0, 1), angle)


def revolve(points):
    """Revolve an (r, z) polygon around the knob axis."""
    return (cq.Workplane("XZ").polyline(points).close()
            .revolve(360, (0, 0, 0), (0, 1, 0)))


# --------------------------------------------------------------------------
# Parts
# --------------------------------------------------------------------------
def hexagon(af, z0, z1, xy):
    return (cq.Workplane("XY").workplane(offset=z0).center(*xy)
            .polygon(6, af / cos(radians(30))).extrude(z1 - z0))


def make_riser():
    """Moving part: plate that carries the display + hub + stem.

    Print with the plate's display side on the bed; no supports needed.
    """
    fr, pr = P.tower_d / 2, P.plate_d / 2
    c, e = P.tube_plate_chamfer, P.plate_edge_chamfer
    body = revolve([
        (0, P.hub_z0), (fr, P.hub_z0), (fr, P.plate_z0 - c), (fr + c, P.plate_z0),
        (pr - e, P.plate_z0), (pr, P.plate_z0 + e), (pr, P.plate_z1), (0, P.plate_z1),
    ])
    # stem down the middle of the tower, with a nub that touches the actuator
    body = body.union(cyl(P.stem_d, P.sw_h + P.nub_h, P.hub_z0 + 0.1))
    body = body.union(cyl(P.nub_d, P.sw_h, P.sw_h + P.nub_h + 0.1))
    # captured nyloc nuts for the retention screws (dropped in from the top)
    for a in P.guide_angles:
        xy = polar(P.guide_r, a)
        body = body.cut(cyl(P.guide_bore_d, P.hub_z0 - 1, P.plate_z1 + 1, xy))
        body = body.cut(hexagon(P.nut_af + P.nut_fit, P.nut_z0, P.plate_z1 + 1, xy))
    # wire channel
    x = P.wire_window_extra
    body = body.cut(oval(P.wire_slot_len + x, P.wire_slot_w + x, P.hub_z0 - 1,
                         P.plate_z1 + 1, P.wire_slot_r, P.wire_slot_angle))
    # countersunk M3 holes under the display standoffs
    csk_h = (P.mod_csk_d - P.mod_screw_d) / 2
    for xy in P.mod_standoffs:
        body = body.cut(cyl(P.mod_screw_d, P.plate_z0 - 1, P.plate_z1 + 1, xy))
        cone = cq.Solid.makeCone(P.mod_csk_d / 2, P.mod_screw_d / 2, csk_h,
                                 pnt=cq.Vector(xy[0], xy[1], P.plate_z0),
                                 dir=cq.Vector(0, 0, 1))
        body = body.cut(cq.Workplane("XY").add(cone))
    return body


def make_tower():
    """Fixed guide inside the encoder bore. Its top is the press hard stop.

    Print with the PCB face on the bed.
    """
    body = cyl(P.tower_d, 0, P.tower_top)
    body = body.cut(cq.Workplane("XY").workplane(offset=-1)
                    .rect(P.sw_pocket_x, P.sw_pocket_y).extrude(P.sw_h + 0.4 + 1))
    body = body.cut(cyl(P.stem_bore_d, -1, P.tower_top + 1))
    for a in P.guide_angles:
        body = body.cut(cyl(P.guide_bore_d, -1, P.tower_top + 1, polar(P.guide_r, a)))
    for a in P.mount_angles:
        body = body.cut(cyl(P.insert_hole_d, -1, P.insert_hole_depth,
                            polar(P.mount_r, a)))
    x = P.wire_window_extra
    return body.cut(oval(P.wire_slot_len + x, P.wire_slot_w + x, -1,
                         P.tower_top + 1, P.wire_slot_r, P.wire_slot_angle))


def make_knob():
    """Ring knob: hub on the rotor sleeve, outer shell flush with the bezel.

    Three radial M3 set screws (self-tapped in the ribs) centre and clamp it.
    Print with the top face on the bed.
    """
    hub_ri = (P.rotor_od + P.knob_fit) / 2
    hub_ro = hub_ri + P.knob_hub_wall
    ro, ri = P.knob_ro, P.knob_ro - P.knob_wall
    zt = P.knob_z1 - P.knob_top_t
    body = revolve([
        (hub_ri, P.knob_rib_z0 - 2), (hub_ro, P.knob_rib_z0 - 2), (hub_ro, zt),
        (ri, zt), (ri, P.knob_z0), (ro - 0.6, P.knob_z0), (ro, P.knob_z0 + 0.6),
        (ro, P.knob_z1 - 0.6), (ro - 0.6, P.knob_z1), (hub_ri, P.knob_z1),
    ])
    mid, length = (hub_ro + ri) / 2, ri - hub_ro + 1.0
    for a in P.knob_rib_angles:
        rib = (cq.Workplane("XY").workplane(offset=P.knob_rib_z0).center(mid, 0)
               .rect(length, P.knob_rib_w).extrude(zt - P.knob_rib_z0 + 0.1))
        hole = (cq.Workplane("YZ").workplane(offset=hub_ri - 1)
                .center(0, P.knob_screw_z).circle(P.knob_thread_d / 2)
                .extrude(ro - hub_ri + 2))
        access = (cq.Workplane("YZ").workplane(offset=hub_ro + 3.5)
                  .center(0, P.knob_screw_z).circle(P.knob_access_d / 2)
                  .extrude(ro - hub_ro))
        body = body.union(rib).cut(hole).cut(access)
        body = body.rotate((0, 0, 0), (0, 0, 1), 120)
    return body.rotate((0, 0, 0), (0, 0, 1), P.knob_rib_angles[0])


def make_bezel():
    """Cap over the display. Sits on the plate with ribs, held by snap bumps.

    Print with the front face on the bed.
    """
    ri, ro = P.bezel_ri, P.bezel_ro
    wr = P.bezel_window_d / 2
    z0 = P.plate_z0 - P.bezel_skirt_below
    body = revolve([
        (ri, z0), (ro, z0), (ro, P.bezel_top - 0.6), (ro - 0.6, P.bezel_top),
        (wr + P.bezel_lip_t, P.bezel_top), (wr, P.lip_z0), (ri, P.lip_z0),
    ])
    for a in P.rib_angles:
        rib = (cq.Workplane("XY").workplane(offset=P.plate_z1)
               .center(ri - P.rib_depth / 2 + 0.2, 0)
               .rect(P.rib_depth + 0.4, P.rib_w)
               .extrude(P.lip_z0 - P.plate_z1 + 0.1))
        body = body.union(rib.rotate((0, 0, 0), (0, 0, 1), a))
    # snap bumps that hook under the plate's chamfered edge
    ring = (cq.Workplane("XY").workplane(offset=z0 + 0.3)
            .circle(ri + 0.2).circle(ri - P.snap_h)
            .extrude(P.plate_z0 + 0.15 - (z0 + 0.3)))
    for a in P.snap_angles:
        box = (cq.Workplane("XY").workplane(offset=z0).center(ri, 0)
               .rect(4, P.snap_w).extrude(3))
        body = body.union(ring.intersect(box).rotate((0, 0, 0), (0, 0, 1), a))
    return body


# --------------------------------------------------------------------------
# Reference parts (not printed)
# --------------------------------------------------------------------------
def load_display():
    """Vendor model: thickness along its Y, flex tab at -X. Move into place."""
    shape = cq.importers.importStep(str(DISPLAY_STEP)).val()
    # 120 deg about (1,1,1): model X -> Y, model Y -> Z, model Z -> X
    shape = shape.rotate((0, 0, 0), (1, 1, 1), 120)
    return shape.translate((0, 0, P.mod_z_off))


def load_encoder():
    shape = cq.importers.importStep(str(ENCODER_STEP)).val()
    return shape.translate((0, 0, P.enc_rotor_top))


def make_pcb_ref(r=32.0):
    """A disc of main PCB with the holes of the Knob_Centre_Mechanics footprint."""
    body = cyl(2 * r, -P.pcb_t, 0)
    for a in P.guide_angles:
        body = body.cut(cyl(P.guide_pcb_hole_d, -3, 1, polar(P.guide_r, a)))
    for a in P.mount_angles:
        body = body.cut(cyl(P.mount_pcb_hole_d, -3, 1, polar(P.mount_r, a)))
    return body.cut(oval(P.wire_slot_len, P.wire_slot_w, -3, 1,
                         P.wire_slot_r, P.wire_slot_angle))


def make_hardware():
    """Retention screws (M3x25 + washer + nyloc nut), tower screws, switch."""
    hw = cq.Workplane("XY")
    for a in P.guide_angles:
        xy = polar(P.guide_r, a)
        hw = hw.union(cyl(5.5, P.ret_head_z - 3.0, P.ret_head_z, xy))
        hw = hw.union(cyl(7.0, P.ret_head_z, -P.pcb_t, xy))
        hw = hw.union(cyl(3.0, P.ret_head_z, P.ret_head_z + P.ret_screw_len, xy))
        hw = hw.union(hexagon(P.nut_af, P.nut_z0, P.nut_z0 + P.nut_h, xy))
    for a in P.mount_angles:
        xy = polar(P.mount_r, a)
        hw = hw.union(cyl(5.5, -P.pcb_t - 3.0, -P.pcb_t, xy))
        hw = hw.union(cyl(3.0, -P.pcb_t, -P.pcb_t + P.mount_screw_len, xy))
    sw = (cq.Workplane("XY").rect(5.1, 5.1).extrude(P.sw_h - 0.3)
          .union(cyl(2.0, 0, P.sw_h)))
    return hw, sw


# --------------------------------------------------------------------------
# Checks, export
# --------------------------------------------------------------------------
def common_volume(a, b):
    try:
        return a.intersect(b).Volume()
    except Exception:
        return float("nan")


def run_checks(parts):
    riser, tower, bezel, knob = (parts[k].val() for k in ("riser", "tower", "bezel", "knob"))
    display, encoder = parts["display"], parts["encoder"]
    pcb, hw, sw = (parts[k].val() for k in ("pcb", "hardware", "switch"))
    down = (0, 0, -P.travel)
    pressed = riser.translate(down)
    rows = [
        ("riser / tower", common_volume(riser, tower)),
        ("riser pressed / tower", common_volume(pressed, tower)),
        ("riser pressed / encoder", common_volume(pressed, encoder)),
        ("riser pressed / knob", common_volume(pressed, knob)),
        ("riser / display", common_volume(riser, display)),
        ("riser / bezel", common_volume(riser, bezel)),
        ("riser / switch (rest)", common_volume(riser, sw)),
        ("riser / screws + nuts", common_volume(riser, hw)),
        ("tower / encoder", common_volume(tower, encoder)),
        ("tower / PCB", common_volume(tower, pcb)),
        ("tower / switch", common_volume(tower, sw)),
        ("tower / screws", common_volume(tower, hw)),
        ("bezel / display", common_volume(bezel, display)),
        ("bezel pressed / knob", common_volume(bezel.translate(down), knob)),
        ("knob / encoder", common_volume(knob, encoder)),
        ("knob / PCB", common_volume(knob, pcb)),
        ("display / screws + nuts", common_volume(display, hw)),
        ("screws / PCB", common_volume(hw, pcb)),
    ]
    print("\nInterference (mm^3, expect 0):")
    for name, v in rows:
        print(f"  {name:30s} {v:8.3f}")
    rmax = max(hypot(v.X, v.Y) for v in display.Vertices())
    bb = display.BoundingBox()
    print(f"\nDisplay bbox X[{bb.xmin:.2f},{bb.xmax:.2f}] Y[{bb.ymin:.2f},{bb.ymax:.2f}] "
          f"Z[{bb.zmin:.2f},{bb.zmax:.2f}]")
    print(f"Display max radius {rmax:.2f}  (plate r {P.plate_d / 2:.2f}, "
          f"bezel inner r {P.bezel_ri:.2f}, bezel OD {2 * P.bezel_ro:.1f})")
    print(f"Radial clearance tower -> encoder bore: {(P.enc_bore_d - P.tower_d) / 2:.2f}")
    print(f"Stack above PCB: tower top {P.tower_top:.2f}, hub {P.hub_z0:.2f}, plate "
          f"{P.plate_z0:.2f}-{P.plate_z1:.2f}, module PCB {P.mod_pcb_z0:.2f}, "
          f"glass top {P.glass_top:.2f}, bezel top {P.bezel_top:.2f}")
    tip = P.ret_head_z + P.ret_screw_len
    print(f"Retention screw: tip at z {tip:.2f} (nut {P.nut_z0:.2f}-{P.nut_z0 + P.nut_h:.2f}, "
          f"plate top {P.plate_z1:.2f}); guide length in tower {P.tower_top:.1f}; "
          f"head {-P.ret_head_z + 3:.1f} behind PCB front")
    print(f"Knob: OD {2 * P.knob_ro:.1f}, z {P.knob_z0:.1f}-{P.knob_z1:.1f}, "
          f"bore {P.rotor_od + P.knob_fit:.1f}")


def wire_points(wire, n=12):
    """Ordered points along a closed wire (its edges come unordered)."""
    segs = [[edge.positionAt(t / n) for t in range(n + 1)] for edge in wire.Edges()]
    chain = segs.pop(0)
    while segs:
        end = chain[-1]
        i, rev = min(((i, r) for i in range(len(segs)) for r in (0, 1)),
                     key=lambda ir: (segs[ir[0]][-ir[1]] - end).Length)
        seg = segs.pop(i)
        chain += (seg[::-1] if rev else seg)[1:]
    return chain


PRINTED = ("riser", "tower", "bezel", "knob")
FOOTPRINT = HERE.parent / "lib" / "blinds.pretty" / "Knob_Centre_Mechanics.kicad_mod"


def write_footprint():
    """PCB side of the mechanics: holes, wire slot, outlines, 3D models.

    KiCad footprint coordinates have Y pointing down, so Y is negated here.
    After changing hole parameters: re-run, then in the PCB editor use
    "Update footprints from library" on MECH1.
    """
    def f(v):
        return (f"{v:.4f}".rstrip("0").rstrip(".") or "0").replace("-0.", "-0.") if abs(v) > 5e-5 else "0"

    def npth(xy, d, d2=None):
        shape, size = ("circle", f"{f(d)} {f(d)}") if d2 is None else ("oval", f"{f(d)} {f(d2)}")
        drill = f(d) if d2 is None else f"oval {f(d)} {f(d2)}"
        return (f'\t(pad "" np_thru_hole {shape}\n\t\t(at {f(xy[0])} {f(-xy[1])})\n'
                f'\t\t(size {size})\n\t\t(drill {drill})\n'
                f'\t\t(layers "*.Cu" "*.Mask")\n\t)\n')

    def circle(r, layer, width=0.1):
        return (f'\t(fp_circle\n\t\t(center 0 0)\n\t\t(end {f(r)} 0)\n'
                f'\t\t(stroke\n\t\t\t(width {width})\n\t\t\t(type solid)\n\t\t)\n'
                f'\t\t(fill no)\n\t\t(layer "{layer}")\n\t)\n')

    def poly(points, layer, width):
        pts = " ".join(f"(xy {f(p.x)} {f(-p.y)})" for p in points)
        return (f'\t(fp_poly\n\t\t(pts {pts})\n'
                f'\t\t(stroke\n\t\t\t(width {width})\n\t\t\t(type solid)\n\t\t)\n'
                f'\t\t(fill no)\n\t\t(layer "{layer}")\n\t)\n')

    def text(kind, name, value, y, layer, hide=False):
        head = f'property "{name}"' if kind == "property" else f"fp_text {name}"
        return (f'\t({head} "{value}"\n\t\t(at 0 {f(y)} 0)\n\t\t(layer "{layer}")\n'
                + ("\t\t(hide yes)\n" if hide else "")
                + "\t\t(effects\n\t\t\t(font\n\t\t\t\t(size 1 1)\n"
                  "\t\t\t\t(thickness 0.15)\n\t\t\t)\n\t\t)\n\t)\n")

    def model(path, rot=(0, 0, 0), off=(0, 0, 0)):
        return (f'\t(model "{path}"\n\t\t(offset\n\t\t\t(xyz {f(off[0])} {f(off[1])} {f(off[2])})\n\t\t)\n'
                "\t\t(scale\n\t\t\t(xyz 1 1 1)\n\t\t)\n"
                f"\t\t(rotate\n\t\t\t(xyz {rot[0]} {rot[1]} {rot[2]})\n\t\t)\n\t)\n")

    out = ('(footprint "Knob_Centre_Mechanics"\n\t(version 20260206)\n'
           '\t(generator "panel_mech.py")\n\t(layer "F.Cu")\n'
           '\t(descr "Holes for the screen riser and its guide tower inside the EC35 '
           'ring encoder. Generated by mech/panel_mech.py - do not edit by hand")\n'
           '\t(tags "mechanical knob screen riser tower")\n')
    out += text("property", "Reference", "REF**", -P.bezel_ro - 1.5, "F.SilkS", hide=True)
    out += text("property", "Value", "Knob_Centre_Mechanics", P.bezel_ro + 1.5, "F.Fab")
    out += "\t(attr board_only exclude_from_pos_files exclude_from_bom)\n"
    out += circle(P.tower_d / 2, "F.Fab")
    out += circle(P.bezel_ro, "F.Fab")
    out += circle(P.knob_ro + 1.0, "Cmts.User", 0.15)
    out += text("text", "user", "Knob + 1 mm: nothing taller than 1 mm inside",
                P.bezel_ro + 3.5, "Cmts.User")
    for a in P.guide_angles:      # washers of the retention screws, on the back
        x, y = polar(P.guide_r, a)
        out += circle(3.5, "B.Fab").replace("(center 0 0)", f"(center {f(x)} {f(-y)})") \
                                   .replace(f"(end {f(3.5)} 0)", f"(end {f(x + 3.5)} {f(-y)})")
    for a in P.mount_angles:
        x, y = polar(P.mount_r, a)
        out += circle(2.75, "B.Fab").replace("(center 0 0)", f"(center {f(x)} {f(-y)})") \
                                    .replace(f"(end {f(2.75)} 0)", f"(end {f(x + 2.75)} {f(-y)})")
    for a in P.guide_angles:
        out += npth(polar(P.guide_r, a), P.guide_pcb_hole_d)
    for a in P.mount_angles:
        out += npth(polar(P.mount_r, a), P.mount_pcb_hole_d)
    # oval NPTH: KiCad size is (x y); the slot's long axis is tangential
    cx, cy = polar(P.wire_slot_r, P.wire_slot_angle)
    horizontal = abs(cx) < abs(cy)
    dims = (P.wire_slot_len, P.wire_slot_w) if horizontal else (P.wire_slot_w, P.wire_slot_len)
    out += npth((cx, cy), *dims)
    for name in PRINTED:
        out += model(f"${{KIPRJMOD}}/mech/out/{name}.step")
    out += model("${KIPRJMOD}/" + str(DISPLAY_STEP.relative_to(HERE.parent)),
                 rot=DISPLAY_KICAD_ROT, off=(0, 0, P.mod_z_off))
    out += "\t(embedded_fonts no)\n)\n"
    FOOTPRINT.write_text(out)
    print(f"Wrote {FOOTPRINT}")


# KiCad model rotation that puts the vendor model like load_display() does
DISPLAY_KICAD_ROT = (-90, 0, -90)


def section_png(parts, path):
    """Cut along the YZ plane (through the top guide screw, wire slot, JST, tab)."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Polygon

    plane = cq.Face.makePlane(140, 140, basePnt=(0, 0, 10), dir=(1, 0, 0))
    colors = {"pcb": "#3c8d5a", "encoder": "#b9b9b9", "display": "#40434a",
              "switch": "#8a6d3b", "hardware": "#6f7f96", "tower": "#e9a23b",
              "riser": "#3d7fd9", "bezel": "#d9534f", "knob": "#7a5ea8"}
    fig, ax = plt.subplots(figsize=(11, 9), dpi=110)
    for name, color in colors.items():
        shape = parts[name]
        shape = shape.val() if isinstance(shape, cq.Workplane) else shape
        try:
            cut = shape.intersect(plane)
        except Exception:
            continue
        for face in cut.Faces():
            outer = face.outerWire()
            for wire, fc in [(outer, color)] + [(w, "white") for w in face.innerWires()]:
                chain = [(p.y, p.z) for p in wire_points(wire)]
                ax.add_patch(Polygon(chain, closed=True, fc=fc, ec="black", lw=0.4))
        ax.plot([], [], "s", color=color, label=name.replace("_", " "))
    ax.set_aspect("equal")
    ax.set_xlim(-33, 33)
    ax.set_ylim(-8, 37)
    ax.set_xlabel("Y (mm)   wire slot / display flex tab  <-->  JST, top retention screw")
    ax.set_ylabel("Z above PCB front (mm)")
    ax.set_title("Section through the knob axis (X = 0), rest position")
    ax.legend(loc="lower right", fontsize=8)
    ax.grid(True, lw=0.2)
    fig.savefig(path, bbox_inches="tight")


def main():
    OUT.mkdir(exist_ok=True)
    hw, sw = make_hardware()
    parts = {
        "riser": make_riser(), "tower": make_tower(), "bezel": make_bezel(),
        "knob": make_knob(),
        "display": load_display(), "encoder": load_encoder(),
        "pcb": make_pcb_ref(), "hardware": hw, "switch": sw,
    }
    for name in PRINTED:
        cq.exporters.export(parts[name], str(OUT / f"{name}.step"))
        print(f"{name}: {parts[name].val().Volume() / 1000:.1f} cm^3, "
              f"{len(parts[name].solids().vals())} solid(s)")

    colors = {"riser": (0.24, 0.5, 0.85), "bezel": (0.2, 0.2, 0.22),
              "tower": (0.91, 0.64, 0.23), "knob": (0.48, 0.37, 0.66),
              "display": (0.1, 0.1, 0.12),
              "encoder": (0.6, 0.6, 0.6), "pcb": (0.1, 0.45, 0.25),
              "hardware": (0.75, 0.75, 0.8), "switch": (0.5, 0.4, 0.2)}
    assy = cq.Assembly(name="panel_mech")
    for name, shape in parts.items():
        assy.add(shape, name=name, color=cq.Color(*colors[name]))
    assy.save(str(OUT / "panel_mech_assembly.step"))

    run_checks(parts)
    write_footprint()
    section_png(parts, OUT / "section.png")
    print(f"\nWrote {OUT}")


if __name__ == "__main__":
    main()
