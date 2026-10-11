import os
import random
import shutil
import subprocess
import sys
import zipfile


def payloads():
    rnd = random.Random(7)
    yield 'empty.png', b''
    yield 'one.ico', b'x'
    yield 'text.png', b'hello icon world ' * 500
    yield 'random.ico', bytes(rnd.getrandbits(8) for _ in range(70000))
    yield 'mixed.png', b''.join(bytes([rnd.choice(b'abcd')]) * rnd.randint(1, 300) for _ in range(3000))
    yield 'long_runs.png', b'\0' * 200000 + b'\1' * 70000
    yield 'Папка/Вложенная/значок.ico', bytes(range(256)) * 300
    yield 'deep/a/b/c/d/icon.png', bytes(rnd.getrandbits(8) for _ in range(1000)) + b'z' * 5000


def build_cases(work):
    cases = []
    for method, level in [(zipfile.ZIP_STORED, None), (zipfile.ZIP_DEFLATED, 0), (zipfile.ZIP_DEFLATED, 1),
                          (zipfile.ZIP_DEFLATED, 6), (zipfile.ZIP_DEFLATED, 9)]:
        path = os.path.join(work, f'm{method}_l{level}.zip')
        with zipfile.ZipFile(path, 'w', compression=method, compresslevel=level) as z:
            z.writestr(zipfile.ZipInfo('Папка/'), b'')
            for name, data in payloads():
                z.writestr(name, data)
        cases.append((path, 'utf-8'))
    path = os.path.join(work, 'zip64.zip')
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for name, data in payloads():
            with z.open(name, 'w', force_zip64=True) as f:
                f.write(data)
    cases.append((path, 'utf-8'))
    path = os.path.join(work, 'cp866.zip')
    names = ['Иконки/Мой компьютер.ico', 'Иконки/Корзина.png']
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for n in names:
            z.writestr('X' * len(n.encode('cp866')), (n * 50).encode('utf-8'))
    with open(path, 'rb') as f:
        raw = f.read()
    for n in names:
        raw = raw.replace(b'X' * len(n.encode('cp866')), n.encode('cp866'), 2)
    with open(path, 'wb') as f:
        f.write(raw)
    cases.append((path, 'cp866'))
    if shutil.which('zip'):
        src = os.path.join(work, 'src')
        os.makedirs(os.path.join(src, 'sub'), exist_ok=True)
        for name, data in payloads():
            with open(os.path.join(src, 'sub', os.path.basename(name)), 'wb') as f:
                f.write(data)
        for flag in ['-0', '-1', '-9']:
            path = os.path.join(work, f'infozip{flag}.zip')
            if os.path.exists(path):
                os.remove(path)
            subprocess.run(['zip', '-q', '-r', flag, path, 'sub'], cwd=src, check=True)
            cases.append((path, 'raw-utf-8'))
    return cases


def expected_name(info, encoding):
    name = info.filename
    if not info.flag_bits & 0x800:
        raw = name.encode('cp437')
        name = raw.decode('cp866' if encoding == 'cp866' else 'utf-8')
    return name.replace('/', '\\')


def check(exe, path, encoding, work):
    out = os.path.join(work, 'out')
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    r = subprocess.run([exe, path, out], capture_output=True)
    lines = r.stdout.decode('utf-8').splitlines()
    ok = r.returncode == 0
    with zipfile.ZipFile(path) as z:
        infos = z.infolist()
        ok = ok and len(lines) == len(infos)
        for line, info in zip(lines, infos):
            idx, name, err, _, _ = line.split('\t')
            if name != expected_name(info, encoding):
                print('  name mismatch', repr(name))
                ok = False
            if info.is_dir():
                continue
            if err != '0':
                print('  error', name, err)
                ok = False
                continue
            with open(os.path.join(out, idx + '.bin'), 'rb') as f:
                if f.read() != z.read(info):
                    print('  data mismatch', name)
                    ok = False
    print('ok  ' if ok else 'FAIL', os.path.basename(path))
    return ok


def fuzz(exe, cases, work, rounds):
    rnd = random.Random(11)
    bad = 0
    for _ in range(rounds):
        with open(rnd.choice(cases)[0], 'rb') as f:
            b = bytearray(f.read())
        for _ in range(rnd.randint(1, 20)):
            b[rnd.randrange(len(b))] = rnd.getrandbits(8)
        if rnd.random() < 0.3:
            b = b[:rnd.randrange(len(b))]
        path = os.path.join(work, 'fuzz.zip')
        with open(path, 'wb') as f:
            f.write(b)
        out = os.path.join(work, 'out')
        shutil.rmtree(out, ignore_errors=True)
        os.makedirs(out)
        try:
            r = subprocess.run([exe, path, out], capture_output=True, timeout=20)
            if r.returncode < 0 or b'Sanitizer' in r.stderr or b'runtime error' in r.stderr:
                bad += 1
        except subprocess.TimeoutExpired:
            bad += 1
    print('ok  ' if bad == 0 else 'FAIL', f'fuzz {rounds} damaged archives, crashes: {bad}')
    return bad == 0


def main(exe, work):
    exe = os.path.abspath(exe)
    work = os.path.abspath(work)
    os.makedirs(work, exist_ok=True)
    cases = build_cases(work)
    ok = all([check(exe, path, enc, work) for path, enc in cases])
    ok = fuzz(exe, cases, work, 300) and ok
    print('ALL OK' if ok else 'SOME FAILED')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1], sys.argv[2]))
