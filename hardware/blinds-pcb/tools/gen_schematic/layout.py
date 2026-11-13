"""Generates blinds-pcb.kicad_sch (one A3 sheet) and checks the exported netlist
against the intended pin->net map.  WARNING: running this OVERWRITES the schematic,
including any edits made in KiCad afterwards.  Usage: python3 layout.py"""
import sys, subprocess, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sch import *
PRJ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
s = Sch('blinds-pcb', {'blinds': f'{PRJ}/lib/blinds.kicad_sym'})
s.title = ['title_block', ['title', Q('Smart blinds - control panel')], ['date', Q('2026-11-12')],
           ['rev', Q('0.1')], ['comment', '1', Q('ESP32 NodeMCU + TMC2209 + ring knob + LCD + WS2812 ring')],
           ['comment', '2', Q('Hand-soldered build: 0805 passives, socketed modules')]]

R0805 = 'Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder'
C0805 = 'Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder'
C1206 = 'Capacitor_SMD:C_1206_3216Metric_Pad1.33x1.80mm_HandSolder'
LED0805 = 'LED_SMD:LED_0805_2012Metric_Pad1.15x1.40mm_HandSolder'
RL = {'100R': '', '1k': 'C17513', '4.7k': 'C17673', '10k': '', '22k': '', '100k': 'C149504'}
CL = {'100nF': 'C38141', '10nF': 'C1710', '22uF': 'C98190', '10uF': 'C13585'}
CFP = {'100nF': C0805, '10nF': C0805, '22uF': C0805, '10uF': C1206}
CVAL = {'100nF': '100nF 50V', '10nF': '10nF 50V', '22uF': '22uF 16V', '10uF': '10uF 50V'}


def V(ref, lib, val, fp, x, y, n1, n2, lcsc='', rot=0, **kw):
    """vertical 2-pin part; n1 = top net, n2 = bottom net"""
    nets = {1: n1, 2: n2} if rot in (0, 270) else {1: n2, 2: n1}
    s.add(ref, lib, val, fp, x, y, rot, nets, lcsc, refpos=(x + 2.54, y - 1.27), valpos=(x + 2.54, y + 1.27), **kw)


def H(ref, lib, val, fp, x, y, nl, nr, lcsc='', rot=90, **kw):
    """horizontal 2-pin part; nl = left net, nr = right net"""
    nets = {1: nl, 2: nr} if rot in (90, 0) else {1: nr, 2: nl}
    s.add(ref, lib, val, fp, x, y, rot, nets, lcsc, refpos=(x - 3.6, y - 2.6), valpos=(x - 3.6, y + 2.7), **kw)


def R(ref, val, x, y, n1, n2, horiz=False, **kw):
    (H if horiz else V)(ref, 'Device:R', val, R0805, x, y, n1, n2, RL[val], **kw)


def C(ref, val, x, y, n1, n2='GND', horiz=False, **kw):
    (H if horiz else V)(ref, 'Device:C', CVAL[val], CFP[val], x, y, n1, n2, CL[val], **kw)


# ============ 1. POWER INPUT ============
s.box(12, 16, 112, 70, '1 · POWER INPUT  12 V DC')
s.add('J1', 'Connector:Barrel_Jack_Switch', 'DC-005 5.5x2.1', 'Connector_BarrelJack:BarrelJack_Horizontal',
      25.4, 40.64, 0, {1: '~VIN_JACK', 2: '~GND', 3: '~GND'}, 'C16214')
H('F1', 'Device:Polyfuse', '2A 33V PTC', 'Fuse:Fuse_1812_4532Metric_Pad1.30x3.40mm_HandSolder',
  53.34, 38.1, '~VIN_JACK', '~VIN_FUSED', 'C18198343')
H('D1', 'Device:D_Schottky', 'SS54', 'Diode_SMD:D_SMA_Handsoldering', 76.2, 38.1, '~VIN_FUSED', '~+12V', 'C7420369', rot=180)
V('D2', 'Device:D_TVS', 'SMBJ15A', 'Diode_SMD:D_SMB_Handsoldering', 88.9, 48.26, '~+12V', 'GND', 'C726824', rot=270)
s.wire('J1.1', 'F1.1'); s.label('VIN_JACK', 35.56, 38.1)
s.wire('F1.2', 'D1.2'); s.label('VIN_FUSED', 58.42, 38.1)
s.wire('D1.1', (104.14, 38.1)); s.power('+12V', 104.14, 38.1); s.flag(96.52, 38.1)
s.wire((88.9, 38.1), 'D2.1')
s.wire('J1.3', (38.1, 40.64), (38.1, 43.18)); s.wire('J1.2', (38.1, 43.18), (38.1, 53.34)); s.power('GND', 38.1, 53.34)
s.wire((38.1, 50.8), (48.26, 50.8)); s.flag(48.26, 50.8)
s.text('Centre-positive plug, 12 V 2 A supply.\nF1: resettable fuse.  D1: reverse-polarity protection.\nD2: clamps plug-in voltage spikes.', 16, 60)

# ============ 2. 5 V BUCK ============
s.box(116, 16, 250, 70, '2 · 5 V / 2 A BUCK  (AP63205)')
s.add('U1', 'Regulator_Switching:AP63205WU', 'AP63205WU-7', 'Package_TO_SOT_SMD:TSOT-23-6_HandSoldering',
      177.8, 40.64, 0, {1: '~+5V', 2: '~+12V', 3: '~+12V', 4: 'GND', 5: '~SW', 6: '~BST'}, 'C2071056')
for ref, val, x in (('C1', '10uF', 124.46), ('C2', '10uF', 139.7), ('C3', '100nF', 154.94)):
    C(ref, val, x, 45.72, '~+12V'); s.wire((x, 38.1), f'{ref}.1')
s.wire((120.65, 38.1), 'U1.3'); s.power('+12V', 120.65, 38.1)
s.wire('U1.2', (165.1, 43.18), (165.1, 38.1))
s.add('C4', 'Device:C', '100nF 50V', C0805, 195.58, 40.64, 90, {1: '~BST', 2: '~SW'}, 'C38141',
      refpos=(193.04, 43.6), valpos=(193.04, 45.8))
s.wire('U1.6', 'C4.1'); s.wire('C4.2', (199.39, 38.1))
H('L1', 'Device:L', '4.7uH 3A', 'Inductor_SMD:L_Changjiang_FNR6045S', 208.28, 38.1, '~SW', '~+5V', 'C168067')
s.wire('U1.5', 'L1.1'); s.label('SW', 189.23, 38.1)
s.label('BST', 189.23, 40.64)
s.wire('L1.2', (243.84, 38.1)); s.power('+5V', 243.84, 38.1); s.flag(236.22, 38.1)
for ref, x in (('C5', 220.98), ('C6', 229.87)):
    pass
C('C5', '22uF', 219.71, 45.72, '~+5V'); s.wire((219.71, 38.1), 'C5.1')
C('C6', '22uF', 229.87, 53.34, '~+5V'); s.wire((229.87, 38.1), 'C6.1')
s.wire('U1.1', (190.5, 43.18), (190.5, 49.53), (214.63, 49.53), (214.63, 38.1))
s.text('Fixed 5 V output: FB goes straight to the output. EN tied to VIN = always on.\nKeep C1-C3, L1 and C5/C6 tight around U1 on the PCB (see datasheet layout).', 120, 62)

# ============ 3. INDICATORS + 12 V SENSE ============
X3 = 17.78
s.box(254, 16, 334, 70, '3 · POWER LEDS + 12 V SENSE')
R('R1', '4.7k', 243.84 + X3, 33.02, '+12V', '~$LED12'); s.wire('R1.2', (243.84 + X3, 39.37))
V('D3', 'Device:LED', 'Red 12V', LED0805, 243.84 + X3, 43.18, '~$LED12', 'GND', rot=90)
R('R2', '1k', 261.62 + X3, 33.02, '+5V', '~$LED5'); s.wire('R2.2', (261.62 + X3, 39.37))
V('D4', 'Device:LED', 'Green 5V', LED0805, 261.62 + X3, 43.18, '~$LED5', 'GND', rot=90)
R('R3', '100k', 281.94 + X3, 30.48, '+12V', '~VSENSE')
R('R4', '22k', 281.94 + X3, 43.18, '~VSENSE', 'GND')
C('C7', '100nF', 294.64 + X3, 43.18, '~VSENSE')
s.wire('R3.2', 'R4.1'); s.wire((281.94 + X3, 36.83), (299.72 + X3, 36.83)); s.wire((294.64 + X3, 36.83), 'C7.1')
s.label('VSENSE', 299.72 + X3, 36.83)
s.text('12 V -> 2.16 V at GPIO36\n(15 V -> 2.70 V)', 300, 60)

# ============ 4. TEST POINTS ============
s.box(338, 16, 408, 70, '4 · TEST POINTS')
TPF = 'TestPoint:TestPoint_Pad_D1.5mm'
for i, (ref, net) in enumerate((('TP1', '+12V'), ('TP2', '+5V'), ('TP3', '+3V3'))):
    x = 345.44 + 12.7 * i
    s.add(ref, 'Connector:TestPoint', net.replace('3V3', '3.3V'), TPF, x, 38.1, 180, {1: net},
          refpos=(x + 7.62, 43.18), valpos=(x + 7.62, 45.72))
s.add('TP4', 'Connector:TestPoint', 'GND', TPF, 383.54, 40.64, 0, {1: 'GND'}, refpos=(385, 36.83), valpos=(385, 39.37))
s.add('TP5', 'Connector:TestPoint', 'LED_DOUT', TPF, 394.97, 40.64, 0, {1: 'LED_DOUT'}, refpos=(396.5, 35.56), valpos=(396.5, 38.1))
s.text('TP5 = output of the last LED\n(to chain more LEDs later).', 342, 58)

# ============ 5. ESP32 ============
s.box(12, 74, 150, 204, '5 · ESP32 NODEMCU  (in sockets)')
NC = 'NC'
s.add('U2', 'blinds:ESP32_NODEMCU', 'ESP32 NodeMCU 38-pin', 'blinds:ESP32_NODEMCU', 86.36, 124.46, 0, {
    'J5_18': 'ESP_EN', 'J5_17': 'VSENSE', 'J5_16': 'AMBIENT', 'J5_15': 'KNOB_SW', 'J5_14': 'TMC_UART',
    'J4_14': 'BOOT', 'J4_15': NC, 'J4_10': NC, 'J5_7': NC, 'J4_16': NC, 'J4_4': NC, 'J4_5': NC,
    'J4_19': NC, 'J4_18': NC, 'J4_17': NC, 'J5_4': NC, 'J5_3': NC, 'J5_2': NC,
    'J5_1': '5V_MCU', 'J5_19': ('+3V3', 22.86),
    'J4_13': 'TFT_RST', 'J5_5': 'TFT_CS', 'J5_8': 'DIR', 'J4_12': 'LED_DATA', 'J4_11': 'TMC_TX',
    'J4_9': 'TFT_SCLK', 'J4_8': 'TFT_DC', 'J4_6': 'SDA', 'J4_3': 'SCL', 'J4_2': 'TFT_MOSI',
    'J5_11': 'TFT_BL', 'J5_10': 'MOTOR_EN', 'J5_9': 'STEP', 'J5_13': 'KNOB_A', 'J5_12': 'KNOB_B',
    'J4_1': ('GND', 7.62), 'J4_7': ('GND', 12.7), 'J5_6': ('GND', 17.78)}, stub=7.62,
    extra={'Sockets': '2x 1x19 female header 2.54 mm (LCSC C2932678)'})
s.add('SW2', 'Switch:SW_Push', 'RESET', 'Button_Switch_THT:SW_Tactile_SPST_Angled_PTS645Vx31-2LFS',
      27.94, 96.52, 0, {1: 'GND', 2: 'ESP_EN'})
s.add('SW3', 'Switch:SW_Push', 'BOOT', 'Button_Switch_THT:SW_Tactile_SPST_Angled_PTS645Vx31-2LFS',
      27.94, 114.3, 0, {1: 'GND', 2: 'BOOT'})
H('D5', 'Device:D_Schottky', 'B5819W', 'Diode_SMD:D_SOD-123', 27.94, 172.72, '+5V', '~5V_MCU', 'C8598', rot=180)
s.wire('D5.1', (55.88, 172.72)); s.label('5V_MCU', 55.88, 172.72); s.flag(45.72, 172.72)
C('C8', '10uF', 35.56, 180.34, '~5V_MCU'); s.wire((35.56, 172.72), 'C8.1')
C('C9', '100nF', 50.8, 180.34, '~5V_MCU'); s.wire((50.8, 172.72), 'C9.1')
C('C10', '100nF', 129.54, 180.34, '+3V3')
s.text('D5 lets USB and 12 V be connected together:\nUSB cannot feed the LEDs or the buck.', 16, 192)
s.text('3V3 decoupling', 120, 192)
s.text('GPIO 0, 2, 5, 12, 15 are boot\nstrapping pins: left free so the\nboard can be flashed in place.\nGPIO 34-39 are input-only.', 16, 128)

# ============ 6. STEPPER DRIVER ============
s.box(154, 74, 266, 138, '6 · STEPPER DRIVER  (BTT TMC2209 V1.3, in sockets)')
s.add('A1', 'blinds:TMC2209_BTT', 'BTT TMC2209 V1.3', 'Module:Pololu_Breakout-16_15.2x20.3mm', 208.28, 101.6, 0, {
    1: 'MOTOR_EN', 2: '~GND', 3: ('GND', 15.24), 4: 'TMC_UART', 5: NC, 6: ('GND', 15.24), 7: 'STEP', 8: 'DIR',
    16: '+12V', 15: ('GND', 20.32), 14: 'MOT_A2', 13: 'MOT_A1', 12: 'MOT_B1', 11: 'MOT_B2', 10: ('+3V3', 20.32), 9: 'GND'},
    extra={'Sockets': '2x 1x8 female header 2.54 mm'}, refpos=(195.58, 88.9), valpos=(203.2, 115.57))
s.wire('A1.2', (175.26, 95.25), (175.26, 97.79))
C('C11', '100nF', 165.1, 124.46, '+12V')
s.add('C12', 'Device:C_Polarized', '100uF 35V', 'Capacitor_THT:CP_Radial_D6.3mm_P2.50mm', 181.61, 124.46, 0,
      {1: '+12V', 2: 'GND'}, 'C109392', refpos=(184.15, 123.19), valpos=(184.15, 125.73))
C('C13', '100nF', 200.66, 124.46, '+3V3')
R('R5', '1k', 229.87, 124.46, 'TMC_TX', 'TMC_UART', horiz=True)
R('R6', '10k', 251.46, 124.46, '+3V3', 'MOTOR_EN')
s.text('MS1 = MS2 = GND: UART address 0.\nR6 keeps the driver off until\nthe firmware enables it.\nDIAG / INDEX pins: not socketed.', 156, 82)

# ============ 7. MOTOR + ENCODER LINK ============
s.box(270, 74, 408, 138, '7 · MOTOR + ENCODER LINK (RJ45)  -  NOT ETHERNET')
s.add('J2', 'Connector:RJ45', 'RJ45 no magnetics', 'Connector_RJ:RJ45_Amphenol_54602-x08_Horizontal', 386.08, 104.14, 180, {
    1: 'MOT_A1', 2: 'MOT_A2', 3: 'MOT_B1', 4: 'SDA', 5: ('GND', 17.78), 6: 'MOT_B2', 7: 'SCL', 8: ('+3V3', 25.4)},
    'C2683359', refpos=(380, 121.9), valpos=(380, 124.5))
R('R7', '10k', 279.4, 96.52, '+3V3', 'SDA')
R('R8', '10k', 294.64, 96.52, '+3V3', 'SCL')
C('C14', '100nF', 309.88, 96.52, '+3V3')
s.add('J3', 'Connector_Generic:Conn_01x04', 'I2C spare (JST-XH)', 'Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical',
      332.74, 121.92, 0, {1: '+3V3', 2: ('GND', 12.7), 3: 'SDA', 4: 'SCL'}, 'C594232')
s.text('Pairs: 1-2 coil A, 3-6 coil B,\n4-5 SDA/GND, 7-8 SCL/3V3.\nNever unplug while powered.', 274, 118)

# ============ 8. KNOB ============
s.box(154, 142, 266, 204, '8 · RING KNOB + CENTRE BUTTON')
s.add('SW1', 'blinds:EC35A1520402', 'EC35A1520402', 'blinds:RotaryEncoder_Alps_EC35A1520402', 248.92, 172.72, 0,
      {'A': '~ENC_A', 'B': '~ENC_B', 'C': '~GND', 'MP': 'GND'})
s.wire('SW1.C', (231.14, 172.72)); s.power('GND', 231.14, 172.72)
for n, yrow, ypu, ycap, rs, rp, c in (('A', 160.02, 154.94, 165.1, 'R9', 'R10', 'C15'), ('B', 185.42, 180.34, 190.5, 'R11', 'R12', 'C16')):
    R(rs, '10k', 218.44, yrow, f'~KNOB_{n}', f'~ENC_{n}', horiz=True)
    R(rp, '10k', 226.06, ypu, '+3V3', f'~ENC_{n}')
    C(c, '10nF', 205.74, ycap, f'~KNOB_{n}')
    s.wire(f'SW1.{n}', (236.22, s.pinpos[f'SW1.{n}'][1]), (236.22, yrow), f'{rs}.2')
    s.wire(f'{rp}.2', (226.06, yrow))
    s.wire(f'{rs}.1', (200.66, yrow)); s.wire((205.74, yrow), f'{c}.1')
    s.label(f'KNOB_{n}', 200.66, yrow, left=True)
s.label('ENC_A', 236.22, 165.1); s.label('ENC_B', 236.22, 180.34)
s.add('SW4', 'Switch:SW_Push', 'TS-1187A', 'Button_Switch_SMD:SW_Push_1P1T_XKB_TS-1187A', 167.64, 172.72, 0,
      {1: ('GND', 5.08), 2: '~KNOB_SW'}, 'C318889')
R('R13', '10k', 180.34, 165.1, '+3V3', '~KNOB_SW'); s.wire('R13.2', (180.34, 172.72))
C('C17', '100nF', 187.96, 177.8, '~KNOB_SW'); s.wire((187.96, 172.72), 'C17.1')
s.wire('SW4.2', (190.5, 172.72)); s.label('KNOB_SW', 190.5, 172.72)

# ============ 9. LCD ============
s.box(270, 142, 340, 204, '9 · LCD  (GC9A01, JST-XH)')
s.add('J4', 'Connector_Generic:Conn_01x08', 'LCD GC9A01', 'Connector_JST:JST_XH_B8B-XH-A_1x08_P2.50mm_Vertical', 322.58, 167.64, 0, {
    1: 'TFT_BL', 2: 'TFT_CS', 3: 'TFT_DC', 4: 'TFT_RST', 5: 'TFT_MOSI', 6: 'TFT_SCLK', 7: ('+3V3', 20.32), 8: 'GND'}, 'C10385', refpos=(320.04, 157.48), valpos=(320.04, 182.88))
C('C18', '10uF', 279.4, 190.5, '+3V3')
C('C19', '100nF', 299.72, 190.5, '+3V3')
s.text('Same order as the module:\nBLK CS DC RES SDA SCL VIN GND', 273, 150)

# ============ 10. AMBIENT LIGHT ============
s.box(344, 142, 408, 204, '10 · AMBIENT LIGHT')
s.add('Q1', 'Sensor_Optical:TEPT4400', 'TEPT4400 (side-looking)', 'LED_THT:LED_D3.0mm_Horizontal_O1.27mm_Z2.0mm_Clear', 365.76, 165.1, 0,
      {1: '+3V3', 2: '~AMBIENT'}, 'C132178')
R('R14', '4.7k', 368.3, 182.88, '~AMBIENT', 'GND')
s.wire('Q1.2', 'R14.1'); s.wire((368.3, 175.26), (375.92, 175.26)); s.label('AMBIENT', 375.92, 175.26)

# ============ 11. LED RING ============
s.box(12, 208, 296, 279, '11 · LED RING  (10 x WS2812B, 5 V data through level shifter)')
s.add('U3', '74xGxx:74AHCT1G125', 'SN74AHCT1G125', 'Package_TO_SOT_SMD:SOT-23-5_HandSoldering', 43.18, 236.22, 0,
      {1: '~GND', 2: 'LED_DATA', 3: 'GND', 4: '~$LED5V', 5: '+5V'}, 'C7484', refpos=(46.99, 241.3), valpos=(46.99, 243.84))
s.wire('U3.1', (43.18, 223.52), (53.34, 223.52), (53.34, 226.06)); s.power('GND', 53.34, 226.06)
R('R16', '100R', 66.04, 236.22, '~$LED5V', '~$DIN1', horiz=True); s.wire('U3.4', 'R16.1')
R('R15', '10k', 22.86, 264.16, 'LED_DATA', 'GND')
C('C20', '100nF', 43.18, 264.16, '+5V')
s.add('C21', 'Device:C_Polarized', '470uF 16V', 'Capacitor_THT:CP_Radial_D8.0mm_P3.50mm', 60.96, 264.16, 0,
      {1: '+5V', 2: 'GND'}, 'C2943989', refpos=(63.5, 262.89), valpos=(63.5, 265.43))
prev = 'R16.2'
for i in range(10):
    x = 86.36 + 20.32 * i
    ref = f'D{6 + i}'
    s.add(ref, 'LED:WS2812B', 'WS2812B', 'LED_SMD:LED_WS2812B_PLCC4_5.0x5.0mm_P3.2mm', x, 236.22, 0,
          {1: '+5V', 2: ('LED_DOUT' if i == 9 else f'~$DIN{i + 2}'), 3: 'GND', 4: f'~$DIN{i + 1}'}, 'C114588')
    s.wire(prev, f'{ref}.4'); prev = f'{ref}.2'
    C(f'C{22 + i}', '100nF', x, 264.16, '+5V')

if __name__ == '__main__':
    out = f'{PRJ}/blinds-pcb.kicad_sch'
    open(out, 'w').write(s.render())
    import tempfile
    net = os.path.join(tempfile.mkdtemp(), 'out.net')
    r = subprocess.run(['kicad-cli', 'sch', 'export', 'netlist', '--format', 'kicadsexpr', '-o', net, out], capture_output=True, text=True)
    print(r.stdout[-300:], r.stderr[-500:])
    check_netlist(net, s.intent)
