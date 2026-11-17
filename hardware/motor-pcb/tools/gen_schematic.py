"""Generates motor-pcb.kicad_sch (one A4 sheet) and checks the exported netlist
against the intended pin->net map.

WARNING: running this OVERWRITES the schematic, including edits made in KiCad
afterwards. KiCad must be closed. Usage (from anywhere):

    python3 tools/gen_schematic.py

It reuses the panel board's generator library
(../blinds-pcb/tools/gen_schematic/sch.py).
"""
import os
import subprocess
import sys
import tempfile

PRJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(PRJ, '..', 'blinds-pcb', 'tools', 'gen_schematic'))
from sch import Sch, Q, check_netlist  # noqa: E402

s = Sch('motor-pcb', {'motor': f'{PRJ}/lib/motor.kicad_sym'}, paper='A4')
s.title = ['title_block', ['title', Q('Smart blinds - motor encoder board')], ['date', Q('2026-11-17')],
           ['rev', Q('A')], ['comment', '1', Q('AS5600 on the NEMA 17 shaft axis + RJ45 link to the panel')],
           ['comment', '2', Q('Hand-soldered build: 0805 passives')]]

R0805 = 'Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder'
C0805 = 'Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder'
LED0805 = 'LED_SMD:LED_0805_2012Metric_Pad1.15x1.40mm_HandSolder'
TPF = 'TestPoint:TestPoint_Pad_D1.5mm'


def V(ref, lib, val, fp, x, y, n1, n2, lcsc='', rot=0, **kw):
    """vertical 2-pin part; n1 = top net, n2 = bottom net"""
    s.add(ref, lib, val, fp, x, y, rot, {1: n1, 2: n2} if rot == 0 else {1: n2, 2: n1}, lcsc,
          refpos=(x + 2.54, y - 1.27), valpos=(x + 2.54, y + 1.27), **kw)


# ============ 1. CABLE FROM THE PANEL ============
s.box(10, 14, 148, 104, '1 · LINK TO THE PANEL (RJ45)  -  NOT ETHERNET')
s.add('J1', 'Connector:RJ45', 'RJ45 54602-908LF (no magnetics)', 'Connector_RJ:RJ45_Amphenol_54602-x08_Horizontal',
      27.94, 55.88, 0, {1: 'MOT_A1', 2: 'MOT_A2', 3: 'MOT_B1', 4: 'SDA', 5: ('GND', 17.78), 6: 'MOT_B2',
                        7: 'SCL', 8: ('+3V3', 17.78)}, 'C2847314', stub=7.62,
      refpos=(22.86, 68.58), valpos=(12.7, 71.12))
s.wire((50.8, 45.72), (50.8, 40.64)); s.flag(50.8, 40.64)
s.wire((50.8, 53.34), (50.8, 48.26)); s.flag(50.8, 48.26)       # cable-fed rails
V('R1', 'Device:R', '4.7k', R0805, 71.12, 55.88, '+3V3', 'SDA', 'C17673')
V('R2', 'Device:R', '4.7k', R0805, 83.82, 55.88, '+3V3', 'SCL', 'C17673')
s.add('D2', 'Power_Protection:SP0502BAHT', 'PESD3V3L2BT', 'Package_TO_SOT_SMD:SOT-23', 101.6, 58.42, 0,
      {1: 'SDA', 2: 'SCL', 3: 'GND'}, 'C55440', refpos=(106.68, 57.15), valpos=(106.68, 59.69))
V('C3', 'Device:C', '10uF 25V', C0805, 124.46, 58.42, '+3V3', 'GND', 'C92009')
V('R3', 'Device:R', '1k', R0805, 137.16, 50.8, '+3V3', '~$LED')
s.add('D1', 'Device:LED', 'Green', LED0805, 137.16, 63.5, 90, {1: 'GND', 2: '~$LED'}, 'C2297',
      refpos=(139.7, 62.23), valpos=(139.7, 64.77))
s.wire('R3.2', 'D1.2')
s.text('Pinout = panel J2 (straight T568B cable).\nPairs: 1-2 coil A, 3-6 coil B, 4-5 SDA/GND, 7-8 SCL/3V3.\n'
       'Never plug into a network device.\nNever unplug while powered (motor coils).', 12, 80)
s.text('R1/R2 sit in parallel with the panel 10k (R7/R8):\n3.2k in total, enough for 1 m of cable at 100 kHz.\n'
       'D2: ESD clamp on SDA/SCL where the cable enters.\nD1: lights when the cable brings 3V3.', 14, 94)

# ============ 2. MOTOR CONNECTOR ============
s.box(10, 108, 148, 196, '2 · STEPPER MOTOR  (JST-XH 4-pin)')
s.add('J2', 'Connector_Generic:Conn_01x04', 'Motor (JST-XH)', 'Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical',
      58.42, 142.24, 0, {1: 'MOT_A1', 2: 'MOT_A2', 3: 'MOT_B1', 4: 'MOT_B2'}, 'C144395', stub=7.62,
      refpos=(55.88, 133.35), valpos=(55.88, 150.0))
s.text('Pins 1-2 = coil A, pins 3-4 = coil B.\nUsual NEMA 17 cable: black, green, red, blue.\n'
       'Check the coil pairs with an ohmmeter (about 1-3 ohm per coil).\n'
       'A wrong turning direction is fixed in firmware (DIR),\nnot by swapping wires.', 14, 162)

# ============ 3. ENCODER ============
s.box(152, 14, 287, 136, '3 · MAGNETIC ENCODER  (AS5600, motor side)')
s.add('U1', 'motor:AS5600', 'AS5600-ASOM', 'Package_SO:SOIC-8_3.9x4.9mm_P1.27mm', 215.9, 66.04, 0,
      {1: '~+3V3', 2: '~+3V3', 3: 'ENC_OUT', 4: ('GND', 5.08), 5: 'NC', 6: 'SDA', 7: 'SCL', 8: '~DIR'},
      'C79815', stub=5.08, refpos=(208.28, 55.88), valpos=(218.44, 55.88))
s.wire('U1.1', (198.12, 60.96)); s.wire('U1.2', (198.12, 63.5)); s.wire((198.12, 63.5), (198.12, 45.72))
s.power('+3V3', 198.12, 45.72)
V('C1', 'Device:C', '100nF 50V', C0805, 165.1, 55.88, '+3V3', 'GND', 'C38141')
V('C2', 'Device:C', '1uF 50V', C0805, 182.88, 55.88, '+3V3', 'GND', 'C28323')
s.add('JP1', 'Jumper:SolderJumper_3_Bridged12', 'DIR', 'Jumper:SolderJumper-3_P1.3mm_Bridged12_RoundedPad1.0x1.5mm',
      246.38, 81.28, 0, {1: 'GND', 2: '~DIR', 3: '+3V3'}, refpos=(243.84, 86.36), valpos=(250.19, 86.36))
s.wire('U1.8', (246.38, 68.58), 'JP1.2'); s.label('DIR', 233.68, 68.58)
for i, (ref, net) in enumerate((('TP1', '+3V3'), ('TP2', 'GND'), ('TP3', 'ENC_OUT'))):
    x = 172.72 + 25.4 * i
    s.add(ref, 'Connector:TestPoint', net.replace('3V3', '3.3V'), TPF, x, 104.14, 0, {1: net},
          refpos=(x + 1.6, 99.06), valpos=(x + 1.6, 101.6))
s.text('C1 + C2 at pins 1-2: with a 3.3 V supply VDD5V and VDD3V3\nare tied together (AS5600 datasheet, 3.3 V operation).\n'
       'PGO open (internal pull-up). OUT: analog angle, test pad only.\nProbe SDA / SCL on R1 / R2.\n'
       'JP1 (DIR): GND = angle rises clockwise seen from the chip top.\n'
       'Cut the bridge and bridge 2-3 to reverse it.', 155, 114)

# ============ 4. NOTES ============
s.box(152, 140, 287, 196, '4 · MECHANICS')
s.text('42.3 x 42.3 mm board on the back of the NEMA 17, M3 holes on the\n31 mm motor pattern (long M3 screws through the motor body).\n'
       'U1 is centred on the shaft axis, on the side facing the motor.\n'
       'Magnet: diametric 6 x 2.5 mm (or 8 x 3), glued to the rear shaft end.\n'
       'Air gap magnet to chip top: 0.5 - 3 mm.\n'
       'Only U1, C1, C2 and JP1 are on the motor side (max 1.75 mm high).', 155, 152)

if __name__ == '__main__':
    out = f'{PRJ}/motor-pcb.kicad_sch'
    open(out, 'w').write(s.render())
    net = os.path.join(tempfile.mkdtemp(), 'out.net')
    r = subprocess.run(['kicad-cli', 'sch', 'export', 'netlist', '--format', 'kicadsexpr', '-o', net, out],
                       capture_output=True, text=True)
    print(r.stdout[-300:], r.stderr[-500:])
    sys.exit(1 if check_netlist(net, s.intent) else 0)
