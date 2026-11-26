"""levels/updrafts.txt from the player's own BotW romfs (s288 port of PR #21 updraft: the table is built on the player's PC,
no game-derived numbers in the source). BotW's updraft columns (actors AscendingCurrent*) of the main map, for Sora's KH2
glide (runtime/sora_updraft.h). BotW lifts only the paraglider in them; Sora's glide is the mod's own, so it needs the
columns itself.

  python -E -P gen_updrafts.py <romfs> <out.txt> [--aoc <aoc dir>]
Read (no network): the map units romfs/Map/MainField/*/*.smubin (+ with --aoc the DLC's aoc/Map/AocField/*/*.smubin) and
each variant's AIProgram (Actor/Pack/<name>.sbactorpack, action AscendingCurrent, SInst WindSpeed).
--aoc is for dev compares only (the PR's table had 93 rows = these 76 MainField + 17 AocField, its commit message): the
AocField is a map of its own (GUESS: the DLC's Trial of the Sword field), the DLL lifts on MainField only and does not know
a row's field, so the sorabuild step never passes it.

The column (BotW 1.9.0 AscendingCurrent, 0x71005f0d80 / 0x71005f1614 / 0x71005f0f60, from the PR): a box of size 2 * Scale
along the actor's axes, standing on Translate (center = Translate + up * Scale.y), pushing along the actor's up axis at
WindSpeed * 30 (the wind area's speed; the Chemical_Updraft element has the same size). Rotate is one float (about y) or
three (x, y, z; R = Rz * Ry * Rx, GUESS: sead's order).

Output (text, LF): line 1 "updrafts v1 <count>", then one column per line, 16 numbers (%.4f) separated by one space:
  pos x y z, R rows (9: world = R * local), half size x y z (= map Scale), push u/s (WindSpeed * 30)
Rows sorted by (x, z). Needs oead (the kit's python has it).
"""
import argparse
import glob
import math
import os

import oead


def vec(v, n=3):
    if isinstance(v, (int, float)) or hasattr(v, 'v'):
        f = float(v.v if hasattr(v, 'v') else v)
        return [f] * n
    return [float(x.v if hasattr(x, 'v') else x) for x in v]


def rot_matrix(r):
    """rows of R (world = R * local); r = [rx, ry, rz] radians, R = Rz * Ry * Rx"""
    cx, sx, cy, sy, cz, sz = math.cos(r[0]), math.sin(r[0]), math.cos(r[1]), math.sin(r[1]), math.cos(r[2]), math.sin(r[2])
    return [[cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx],
            [sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx],
            [-sy, cy * sx, cy * cx]]


def wind_speed(name, packdirs, cache={}):
    if name in cache:
        return cache[name]
    sp = None
    for d in packdirs:
        p = os.path.join(d, name + '.sbactorpack')
        if not os.path.isfile(p):
            continue
        sarc = oead.Sarc(oead.yaz0.decompress(open(p, 'rb').read()))
        for f in sarc.get_files():
            if not f.name.endswith('.baiprog'):
                continue
            pio = oead.aamp.ParameterIO.from_binary(bytes(f.data))
            for _, lst in pio.lists['Action'].lists.items():
                df = lst.objects['Def']
                if str(df.params['ClassName'].v) == 'AscendingCurrent' and 'SInst' in lst.objects:
                    sp = float(lst.objects['SInst'].params['WindSpeed'].v)
        if sp is not None:
            break
    cache[name] = sp
    return sp


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('romfs')
    ap.add_argument('out')
    ap.add_argument('--aoc')
    a = ap.parse_args()
    units = sorted(glob.glob(os.path.join(a.romfs, 'Map', 'MainField', '*', '*.smubin')))
    if not units:
        raise SystemExit('gen_updrafts: no map units in %s/Map/MainField' % a.romfs)
    if a.aoc:
        units += sorted(glob.glob(os.path.join(a.aoc, 'Map', 'AocField', '*', '*.smubin')))
    packdirs = [os.path.join(a.romfs, 'Actor', 'Pack')] + ([os.path.join(a.aoc, 'Actor', 'Pack')] if a.aoc else [])
    rows, seen = [], set()
    for u in units:
        d = oead.byml.from_binary(oead.yaz0.decompress(open(u, 'rb').read()))
        for o in d.get('Objs', []):
            n = str(o.get('UnitConfigName', ''))
            if not n.startswith('AscendingCurrent'):
                continue
            t = vec(o['Translate'])
            r = o.get('Rotate', 0.0)
            r = [0.0, float(r.v if hasattr(r, 'v') else r), 0.0] if not isinstance(r, (list, oead.byml.Array)) else vec(r)
            s = vec(o.get('Scale', 1.0))
            key = (n, round(t[0], 2), round(t[1], 2), round(t[2], 2))
            if key in seen:
                continue
            seen.add(key)
            ws = wind_speed(n, packdirs)
            if ws is None:
                print('skip %s (no WindSpeed found)' % n)
                continue
            rows.append((t, rot_matrix(r), s, ws * 30.0))
    rows.sort(key=lambda x: (x[0][0], x[0][2]))
    out = ['updrafts v1 %d' % len(rows)]
    for t, R, s, sp in rows:
        out.append(' '.join('%.4f' % v for v in t + [x for row in R for x in row] + s + [sp]))
    with open(a.out, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(out) + '\n')
    print('updrafts: %d columns' % len(rows))


if __name__ == '__main__':
    main()
