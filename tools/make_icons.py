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



def hsv(h, s_, v):
    import colorsys
    r, g, b = colorsys.hsv_to_rgb(h % 1.0, s_, v)
    return (int(r * 255), int(g * 255), int(b * 255), 255)


def near_segment(x, y, ax, ay, bx, by, w):
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / float(dx * dx + dy * dy)))
    return math.hypot(x - (ax + t * dx), y - (ay + t * dy)) <= w


def mystify(x, y):
    quads = [((4, 6), (27, 3), (22, 26), (8, 20), 0.55), ((10, 12), (29, 18), (14, 29), (3, 24), 0.85)]
    for pts_h in quads:
        pts, h = pts_h[:4], pts_h[4]
        for i in range(4):
            (ax, ay), (bx, by) = pts[i], pts[(i + 1) % 4]
            if near_segment(x, y, ax, ay, bx, by, 0.9):
                return hsv(h + i * 0.05, 0.8, 1)
    return (0, 0, 0, 255)


def matrix(x, y):
    col = x // 4
    head = (col * 7 + 5) % 32
    if x % 4 == 3 or (y // 3) % 2 == 1 and (x + y) % 3 == 0:
        return (0, 0, 0, 255)
    d = head - y
    if 0 <= d < 14:
        v = 255 if d == 0 else int(220 * (1 - d / 14.0))
        return (v // 4 if d else 200, v, v // 4 if d else 200, 255)
    return (0, 0, 0, 255)


def tunnel(x, y):
    r = math.hypot(x - 15.5, y - 15.5)
    if r > 15.5:
        return (0, 0, 0, 0)
    a = math.atan2(y - 15.5, x - 15.5)
    depth = 40.0 / (r + 1)
    check = (int(depth) + int(a / (math.pi / 6))) % 2
    v = min(1.0, r / 12)
    return hsv(depth * 0.05, 0.8, v * (1.0 if check else 0.45))


def ribbons(x, y):
    for k, h in ((0, 0.0), (1, 0.33), (2, 0.6)):
        cy = 16 + 9 * math.sin(x * 0.22 + k * 2.1)
        if abs(y - cy) < 1.6 + 1.2 * math.sin(x * 0.15 + k):
            return hsv(h + x * 0.01, 0.85, 1)
    return (0, 0, 0, 255)


def bubbles(x, y):
    for cx, cy, r in ((11, 19, 9), (23, 9, 6), (25, 24, 4)):
        d = math.hypot(x - cx, y - cy)
        if d <= r:
            if math.hypot(x - (cx - r * 0.4), y - (cy - r * 0.4)) < r * 0.25:
                return (255, 255, 255, 255)
            rim = (d / r) ** 3
            return hsv(0.55 + d * 0.03, 0.6, 0.2 + 0.8 * rim)
    return (0, 0, 20, 255)


def plasma(x, y):
    v = math.sin(x * 0.3) + math.sin(y * 0.25) + math.sin((x + y) * 0.2) + math.sin(math.hypot(x - 16, y - 16) * 0.4)
    return hsv((v + 4) / 8 * 2, 0.9, 1)


def fireworks(x, y):
    for cx, cy, h in ((12, 11, 0.0), (23, 19, 0.55)):
        d = math.hypot(x - cx, y - cy)
        a = math.atan2(y - cy, x - cx)
        if d < 10 and abs(math.sin(a * 6)) < 0.25 and d > 1.5:
            return hsv(h, 0.7, 1 - d / 12)
        if d <= 1.5:
            return (255, 255, 220, 255)
    return (0, 0, 15, 255)


def flowerbox(x, y):
    cx, cy = x - 15.5, y - 15.5
    a = math.atan2(cy, cx)
    r = 9 + 4 * abs(math.cos(3 * a))
    d = math.hypot(cx, cy)
    if d <= r:
        shade = 0.55 + 0.45 * (1 - d / r) + (0.2 if cx < 0 and cy < 0 else 0)
        return hsv(0.9 + a / (2 * math.pi) * 0.3, 0.8, min(1.0, shade))
    return (0, 0, 0, 0)


for name, fn in (('pipes', pipes), ('starfield', starfield), ('polyhedra', polyhedra),
                 ('mystify', mystify), ('matrix', matrix), ('tunnel', tunnel), ('ribbons', ribbons),
                 ('bubbles', bubbles), ('plasma', plasma), ('fireworks', fireworks), ('flowerbox', flowerbox)):
    write_ico(os.path.join(ROOT, 'savers', name, name + '.ico'), fn)
    print('wrote', name)
