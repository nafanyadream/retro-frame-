import colorsys
import math
import os
import shutil
import sys
import zipfile

KEY = (255, 0, 255)


def ppm(w, h, pixels):
    return b'P6\n%d %d\n255\n' % (w, h) + bytes(c for p in pixels for c in p)


def shape(kind, size, hue):
    r, g, b = [int(v * 255) for v in colorsys.hsv_to_rgb(hue, 0.8, 1.0)]
    pixels = []
    for y in range(size):
        for x in range(size):
            u, v = (x + 0.5) / size * 2 - 1, (y + 0.5) / size * 2 - 1
            if kind == 'sun':
                d = math.hypot(u, v)
                inside, edge = d < 0.8, 0.68 < d < 0.8
            elif kind == 'box':
                inside = abs(u) < 0.8 and abs(v) < 0.7
                edge = inside and (abs(u) > 0.68 or abs(v) > 0.58)
            else:
                inside = v > -0.8 and abs(u) < (v + 0.8) / 2
                edge = inside and (v > 0.62 or abs(u) > (v + 0.8) / 2 - 0.16)
            pixels.append((0, 0, 0) if edge else ((r, g, b) if inside else KEY))
    return ppm(size, size, pixels)


def photo(w, h, seed):
    pixels = []
    for y in range(h):
        for x in range(w):
            sky = y < h * 0.6
            t = (x + seed * 37) % 256
            pixels.append((40 + t // 4, 120 + y % 60, 220) if sky else (30, 120 + (x * 7 + y) % 80, 40))
    return ppm(w, h, pixels)


def main(root):
    icons = os.path.join(root, 'icons')
    shutil.rmtree(icons, ignore_errors=True)
    os.makedirs(icons)
    files = []
    kinds = ['sun', 'box', 'tri']
    for i in range(24):
        sub = ['', 'Значки', 'Значки/Старые', 'misc'][i % 4]
        name = f'{kinds[i % 3]}{i + 1}.' + ('ico' if i % 2 else 'png')
        files.append((f'{sub}/{name}' if sub else name, shape(kinds[i % 3], [16, 32, 48, 24][i % 4], i / 24)))
    files.append(('broken.ico', b'not an icon'))
    files.append(('readme.txt', b'hello'))
    for rel, data in files:
        path = os.path.join(icons, *rel.split('/'))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as f:
            f.write(data)
    with zipfile.ZipFile(os.path.join(root, 'pack.zip'), 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for rel, data in files:
            z.writestr(rel, data)
    for n, (w, h) in enumerate([(480, 320), (300, 420)]):
        with open(os.path.join(root, f'photo{n + 1}.ppm'), 'wb') as f:
            f.write(photo(w, h, n))


if __name__ == '__main__':
    main(sys.argv[1])
