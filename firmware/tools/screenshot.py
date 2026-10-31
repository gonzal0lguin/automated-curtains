#!/usr/bin/env python3
"""Grab a screenshot of the round display over the serial port.

  python3 tools/screenshot.py                      # auto-detect port, saves screenshot.png
  python3 tools/screenshot.py -p /dev/ttyUSB0 -o menu.png
  python3 tools/screenshot.py --log monitor.txt    # extract every screenshot from a saved log
  python3 tools/screenshot.py --keys "knob 2,click" -o settings.png   # navigate first

Needs pyserial (bundled with PlatformIO: ~/.platformio/penv/bin/python).
Close the serial monitor first - only one program can use the port.
The firmware's console command "screenshot" prints the frame as base64 RGB565
(big-endian), one line per row.
"""
import argparse
import base64
import glob
import struct
import sys
import time
import zlib

HEADER = "=== SCREENSHOT"
END = "=== END ==="


def write_png(path, w, h, rgb565_rows, round_mask=True):
    raw = bytearray()
    cx = cy = (w - 1) / 2
    r2 = (min(w, h) / 2) ** 2
    for y, row in enumerate(rgb565_rows):
        raw.append(0)  # filter: none
        for x in range(w):
            v = (row[2 * x] << 8) | row[2 * x + 1]
            r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
            inside = (x - cx) ** 2 + (y - cy) ** 2 <= r2
            a = 255 if (inside or not round_mask) else 0
            raw += bytes(((r * 527 + 23) >> 6, (g * 259 + 33) >> 6, (b * 527 + 23) >> 6, a))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def parse(lines):
    """Yield (w, h, rows) for every screenshot block in an iterable of lines."""
    it = iter(lines)
    for line in it:
        line = line.strip()
        if not line.startswith(HEADER):
            continue
        parts = line.split()
        w, h = int(parts[2]), int(parts[3])
        rows = []
        for row in it:
            row = row.strip()
            if row == END:
                break
            try:
                data = base64.b64decode(row)
            except Exception:
                continue
            if len(data) == w * 2:
                rows.append(data)
        if len(rows) == h:
            yield w, h, rows
        else:
            print(f"incomplete screenshot ({len(rows)}/{h} rows), skipped", file=sys.stderr)


def find_port():
    ports = sorted(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*") + glob.glob("/dev/cu.usbserial*")
                   + glob.glob("/dev/cu.SLAB*") + glob.glob("/dev/cu.wchusbserial*"))
    if not ports:
        sys.exit("no serial port found, use -p")
    return ports[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-p", "--port")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("-o", "--out", default="screenshot.png")
    ap.add_argument("--log", help="parse a saved serial log instead of the port")
    ap.add_argument("--keys", help="comma separated console commands to send first, e.g. 'knob 2,click'")
    ap.add_argument("--square", action="store_true", help="don't mask the corners outside the round panel")
    a = ap.parse_args()

    if a.log:
        with open(a.log, errors="replace") as f:
            shots = list(parse(f))
        if not shots:
            sys.exit("no screenshot found in log")
        for i, (w, h, rows) in enumerate(shots):
            out = a.out if len(shots) == 1 else a.out.replace(".png", f"_{i + 1}.png")
            write_png(out, w, h, rows, not a.square)
            print("saved", out)
        return

    import serial  # pyserial

    port = a.port or find_port()
    with serial.Serial(port, a.baud, timeout=0.5) as s:
        time.sleep(0.3)
        s.reset_input_buffer()
        for k in (a.keys.split(",") if a.keys else []):
            s.write((k.strip() + "\n").encode())
            time.sleep(0.5)
        s.reset_input_buffer()
        s.write(b"screenshot\n")
        lines, deadline = [], time.time() + 15
        while time.time() < deadline:
            line = s.readline().decode(errors="replace")
            if line:
                lines.append(line)
                if line.strip() == END:
                    break
    shots = list(parse(lines))
    if not shots:
        sys.exit("no screenshot received - is the main or 'ui' test firmware running?")
    w, h, rows = shots[-1]
    write_png(a.out, w, h, rows, not a.square)
    print(f"saved {a.out} ({w}x{h}) from {port}")


if __name__ == "__main__":
    main()
