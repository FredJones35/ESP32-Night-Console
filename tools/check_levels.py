#!/usr/bin/env python3
"""Doom bölüm dosyalarını kontrol eder: harita kapalı mı, başlangıç var mı,
anahtarlarla çıkışa ulaşılabiliyor mu? Kullanım: tools/check_levels.py sd/doom/*.lvl
Gömülü bölümler için: tools/check_levels.py src/games/doom/levels.cpp"""
import re, sys
from collections import deque

WALLS = set("123456X")
DOORS = {"D": None, "S": None, "R": "r", "Y": "y", "B": "b"}
THINGS = set("zdfkhamesgrybtc")

def levels_from(path):
    txt = open(path).read()
    if path.endswith(".cpp"):
        return [(path + ":" + m.group(1), m.group(2)) for m in re.finditer(r'static const char (\w+)\[\] = R"\((.*?)\)";', txt, re.S)]
    return [(path, txt)]

def check(name, txt):
    errs = []
    rows, header, sec = [], {}, ""
    for line in txt.splitlines():
        s = line.strip()
        if not s or s.startswith("#"): continue
        if s.startswith("["): sec = s[1:s.index("]")].lower(); continue
        if sec == "" and "=" in s:
            k, v = s.split("=", 1); header[k.strip()] = v.strip()
        elif sec == "map": rows.append(s)
    if not rows: return [f"{name}: harita yok"]
    w, h = max(map(len, rows)), len(rows)
    if w > 32 or h > 32: errs.append(f"harita {w}x{h} > 32x32")
    grid = [r.ljust(w, "1") for r in rows]
    for y, r in enumerate(grid):
        for x, c in enumerate(r):
            if (x in (0, w - 1) or y in (0, h - 1)) and c not in WALLS and c not in DOORS:
                errs.append(f"kenar acik: ({x},{y}) '{c}'")
            if c not in WALLS and c not in DOORS and c not in THINGS and c not in ".@ ":
                errs.append(f"bilinmeyen karakter '{c}' ({x},{y})")
    starts = [(x, y) for y, r in enumerate(grid) for x, c in enumerate(r) if c == "@"]
    if len(starts) != 1: errs.append(f"{len(starts)} baslangic noktasi (1 olmali)")
    if not starts: return [f"{name}: " + e for e in errs]
    if sum(r.count("X") for r in grid) == 0: errs.append("cikis (X) yok")
    # anahtarlarla erişilebilirlik
    keys = set()
    while True:
        seen = {starts[0]}; q = deque([starts[0]]); exit_ok = False
        while q:
            x, y = q.popleft()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < w and 0 <= ny < h) or (nx, ny) in seen: continue
                c = grid[ny][nx]
                if c == "X": exit_ok = True; continue
                if c in WALLS: continue
                if c in DOORS and DOORS[c] and DOORS[c] not in keys: continue
                seen.add((nx, ny)); q.append((nx, ny))
        got = {grid[y][x] for x, y in seen if grid[y][x] in "ryb"}
        if got <= keys: break
        keys |= got
    if not exit_ok: errs.append("cikisa ulasilamiyor")
    count = lambda ch: sum(r.count(ch) for r in grid)
    info = f"{w}x{h} z{count('z')} d{count('d')} f{count('f')} k{count('k')} gizli{count('S')} anahtar{''.join(sorted(keys)) or '-'}"
    print(f"{name}: {header.get('name', '?')} | {info}" + ("" if errs else " | OK"))
    return [f"{name}: " + e for e in errs]

errors = []
for p in sys.argv[1:]:
    for n, t in levels_from(p):
        errors += check(n, t)
for e in errors: print("HATA", e)
sys.exit(1 if errors else 0)
