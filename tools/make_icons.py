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



def sky_bg(y, top=(40, 130, 240), bottom=(200, 240, 255)):
    t = y / 31.0
    return tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)


def glossy_orb(x, y, cx, cy, r, tint):
    d = math.hypot(x - cx, y - cy)
    if d > r:
        return None
    rim = (d / r) ** 3
    base = [int(c * (0.55 + 0.45 * rim)) for c in tint]
    ex, ey = (x - cx) / (r * 0.7), (y - (cy - r * 0.42)) / (r * 0.42)
    if ex * ex + ey * ey < 1:
        k = 0.75 * (1 - (y - (cy - r)) / r)
        base = [int(c + (255 - c) * max(0, k)) for c in base]
    return tuple(min(255, c) for c in base) + (255,)


def aurora(x, y):
    bg = sky_bg(y, (0, 20, 60), (0, 100, 110))
    for k, (h, a) in enumerate(((0.38, 1.0), (0.52, 0.8))):
        cy = 14 + 6 * math.sin(x * 0.2 + k * 2) + k * 5
        d = abs(y - cy)
        if d < 4:
            g = (1 - d / 4) * a
            c = hsv(h, 0.6, 1)
            bg = tuple(min(255, int(bg[i] + c[i] * g)) for i in range(3)) + (255,)
    return bg


def orbs(x, y):
    for cx, cy, r, tint in ((12, 18, 9, (60, 190, 255)), (24, 9, 6, (140, 240, 70)), (25, 25, 4, (60, 190, 255))):
        o = glossy_orb(x, y, cx, cy, r, tint)
        if o:
            return o
    return sky_bg(y)


def meadow(x, y):
    hill1 = 20 + 3 * math.sin(x * 0.18 + 1)
    hill2 = 25 + 2 * math.sin(x * 0.25 + 3)
    if y > hill2:
        return (40, 140, 20, 255)
    if y > hill1:
        return (110, 200, 60, 255) if y - hill1 > 1 else (200, 245, 160, 255)
    if math.hypot(x - 9, y - 9) < 4:
        return (255, 255, 230, 255)
    if (x - 22) ** 2 / 36 + (y - 9) ** 2 / 9 < 1:
        return (255, 255, 255, 255)
    return sky_bg(y)


def aquarium(x, y):
    bg = sky_bg(y, (120, 230, 255), (10, 90, 160))
    if (x - 16) ** 2 / 64 + (y - 16) ** 2 / 20 < 1:
        o = glossy_orb(x, y, 16, 16, 8, (255, 140, 30))
        return o if o else (255, 140, 30, 255)
    if 22 <= x <= 28 and abs(y - 16) < (x - 22) * 0.9:
        return (255, 190, 90, 255)
    for cx, cy in ((7, 8), (10, 4), (26, 7)):
        if math.hypot(x - cx, y - cy) < 1.6:
            return (230, 250, 255, 255)
    return bg


def glasspanes(x, y):
    bg = sky_bg(y, (0, 40, 90), (0, 120, 140))
    for x0, y0, w, h in ((4, 6, 16, 12), (13, 14, 16, 12)):
        if x0 <= x < x0 + w and y0 <= y < y0 + h:
            edge = x in (x0, x0 + w - 1) or y in (y0, y0 + h - 1)
            gloss = y < y0 + h * 0.45
            k = 0.9 if edge else (0.45 if gloss else 0.22)
            bg = tuple(int(bg[i] + (230 - bg[i]) * k) for i in range(3)) + (255,)
    return bg


def bokeh(x, y):
    bg = sky_bg(y, (0, 70, 120), (80, 190, 90))
    for cx, cy, r, c in ((9, 10, 7, (120, 240, 255)), (22, 18, 8, (200, 255, 120)), (15, 26, 4, (255, 255, 255)), (26, 6, 3, (160, 255, 220))):
        d = math.hypot(x - cx, y - cy)
        if d < r:
            k = 0.35 + 0.35 * (d / r) ** 4
            bg = tuple(min(255, int(bg[i] + c[i] * k)) for i in range(3)) + (255,)
    return bg


for name, fn in (('pipes', pipes), ('starfield', starfield), ('polyhedra', polyhedra),
                 ('mystify', mystify), ('matrix', matrix), ('tunnel', tunnel), ('ribbons', ribbons),
                 ('bubbles', bubbles), ('plasma', plasma), ('fireworks', fireworks), ('flowerbox', flowerbox),
                 ('aurora', aurora), ('orbs', orbs), ('meadow', meadow), ('aquarium', aquarium),
                 ('glasspanes', glasspanes), ('bokeh', bokeh)):
    write_ico(os.path.join(ROOT, 'savers', name, name + '.ico'), fn)
    print('wrote', name)
