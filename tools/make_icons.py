"""Generates the 32x32 icons for each screensaver (stdlib only).

Usage: python3 tools/make_icons.py
"""
import math, os, struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
N = 32


def write_ico(path, pixel):
    rows = []
    for y in range(N - 1, -1, -1):          # BMP rows are bottom-up
        for x in range(N):
            r, g, b, a = pixel(x, y)
            rows.append(struct.pack('BBBB', b, g, r, a))
    xor = b''.join(rows)
    and_mask = b'\0' * (N * 4)               # 32 rows x 4 bytes, all opaque (alpha decides)
    hdr = struct.pack('<IiiHHIIiiII', 40, N, N * 2, 1, 32, 0, len(xor) + len(and_mask), 0, 0, 0, 0)
    img = hdr + xor + and_mask
    ico = struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHII', N, N, 0, 0, 1, 32, len(img), 22) + img
    with open(path, 'wb') as f:
        f.write(ico)


def pipes(x, y):
    # Two pipes with an elbow, on a dark rounded square.
    def tube(d, col):
        t = max(0.0, 1 - d / 4.0)
        return tuple(int(c * (0.45 + 0.55 * t)) for c in col)
    bg = (20, 20, 40, 255)
    if abs(y - 10) <= 4 and x <= 20:
        return tube(abs(y - 10), (230, 60, 60)) + (255,)
    if abs(x - 20) <= 4 and 10 <= y:
        return tube(abs(x - 20), (230, 60, 60)) + (255,)
    if abs(y - 23) <= 3 and x <= 15:
        return tube(abs(y - 23), (60, 120, 240)) + (255,)
    if math.hypot(x - 20, y - 10) <= 5:
        return (240, 90, 90, 255)
    return bg


def starfield(x, y):
    import random
    rnd = random.Random(x * 131 + y * 7)
    if rnd.random() < 0.06:
        v = rnd.randint(160, 255)
        return (v, v, 255, 255)
    cx, cy = x - 15.5, y - 15.5
    if abs(cx * 0.4 - cy) < 0.6 and cx > 2 or abs(-cx * 0.6 - cy) < 0.6 and cx < -2:
        return (200, 200, 255, 255)
    return (0, 0, 20, 255)


def polyhedra(x, y):
    # A shaded diamond (octahedron seen from above).
    cx, cy = x - 15.5, y - 15.5
    if abs(cx) + abs(cy) <= 13:
        if cx < 0 and cy < 0: return (120, 200, 255, 255)
        if cx >= 0 and cy < 0: return (60, 140, 230, 255)
        if cx < 0: return (40, 90, 190, 255)
        return (20, 50, 130, 255)
    return (0, 0, 0, 0)


for name, fn in (('pipes', pipes), ('starfield', starfield), ('polyhedra', polyhedra)):
    write_ico(os.path.join(ROOT, 'savers', name, name + '.ico'), fn)
    print('wrote', name)
