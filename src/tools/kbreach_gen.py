#!/usr/bin/env python3
"""s249 kbreach: writes runtime/sora_kbreach_tab.h = each KH2 Keyblade's own ATTACK SPHERES (the shapes KH2 hits with) from the
player's KH2 extract. The logic that uses them is hand-written in runtime/sora_kbreach.h (this tool writes only the table).

  python -I tools/kbreach_gen.py --kh2 D:/KH2Extract [--out runtime/sora_kbreach_tab.h] [--check]

Source (KH2 data): obj/W_EX010[_<set>].mdlx, BAR entry type 23 (the model's collision table: u32 count, rows from +0x40, 20 bytes
each, OpenKH ObjectCollision: u8 Group, u8 Parts, s16 Argument, u8 Type, u8 Shape, s16 Bone, s16 X, Y, Z, PositionHeight, Radius,
Height). The rows of TYPE 6 are the weapon's attack shapes; KH2 (exe FUN_14040f610, the YS::ATTACK_OBJ frame step) takes, while an
attack window (mset range trigger 10: atkp, group) is live, every type-6 row of the weapon object whose Group = the window's group
(recon/s249/S249_HELPER_KBREACH.md). In every Keyblade model all type-6 rows are spheres (shape 3, Radius = Height) on bone 2 (the
grip bone) at (X, 0, 0) with PositionHeight 0: the tool checks that and stops otherwise. The blade runs along bone 2's -X (the tip
marker, type 15, is at -78 / -103 / -130 cm), so the table stores cm = -X (0 = the grip, + toward the tip; negative = the pommel
side). Which Keyblade id has which set: KBFX_TAB in runtime/sora_kbfx.h (read here, not repeated). Rows without a W_EX010 set of
their own (our Keyblades "s1", "g1", "p1", "c1") get the Kingdom Key's spheres (no KH2 model exists for them).
--check: exit 1 when the header on disk differs from what the extract gives (nothing written).
Numbers only (ids, groups, cm) go into the header: no KH2 bytes.
"""
import argparse
import os
import re
import struct
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
PKG_ORDER = ["kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth", "kh2_sixth"]
GRIP_BONE = 2
ATTACK_TYPE = 6
SPHERE = 3


def die(m):
    raise SystemExit("[kbreach_gen] " + m)


def kh2_obj(kh2, name):
    hit = None
    for p in PKG_ORDER:
        c = os.path.join(kh2, p, "original", "obj", name)
        if os.path.isfile(c):
            hit = c
    return hit


def spheres(path):
    """[(group, cm along the blade, radius cm)] in file order (KH2 walks them in that order)"""
    d = open(path, "rb").read()
    if d[:4] != b"BAR\x01":
        die("%s is not a BAR file" % path)
    out = None
    for i in range(struct.unpack_from("<I", d, 4)[0]):
        t, _, _, off, sz = struct.unpack_from("<HH4sII", d, 16 + 16 * i)
        if t != 23:
            continue
        if out is not None:
            die("%s: two collision entries" % path)
        n = struct.unpack_from("<i", d, off)[0]
        if not 0 < n < 256 or 0x40 + 20 * n > sz:
            die("%s: odd collision table (%d rows)" % (path, n))
        out = []
        for k in range(n):
            g, _, _, ty, sh, bone, x, y, z, ph, r, h = struct.unpack_from("<BBhBBhhhhhhh", d, off + 0x40 + 20 * k)
            if ty != ATTACK_TYPE:
                continue
            if sh != SPHERE or bone != GRIP_BONE or y or z or ph or r != h or not 0 < r < 256:
                die("%s: attack row %d is not a sphere on bone 2's x axis (shape %d bone %d (%d %d %d) ph %d r %d h %d)"
                    % (path, k, sh, bone, x, y, z, ph, r, h))
            out.append((g, -x, r))
    if not out:
        die("%s: no attack spheres" % path)
    if len(out) > 32:
        die("%s: %d attack rows (KH2's attack object keeps 32)" % (path, len(out)))
    return out


def tab_rows(src, name):
    body = src[src.index(name + "[] = {"):]
    return body[:body.index("};")]


def kbfx_rows():
    src = open(os.path.join(ROOT, "runtime", "sora_kbfx.h"), encoding="utf-8").read()
    rows = re.findall(r'\{\s*(0x[0-9a-fA-F]+)\s*,\s*"([0-9a-z]*)"\s*,\s*"((?:[^"\\]|\\.)*)"', tab_rows(src, "KBFX_TAB"))
    if len(rows) < 30:
        die("could not read KBFX_TAB from runtime/sora_kbfx.h (%d rows)" % len(rows))
    return [(int(i, 16), s, n) for i, s, n in rows]


def kb_ids():
    src = open(os.path.join(ROOT, "runtime", "sora_kb.h"), encoding="utf-8").read()
    body = tab_rows(src, "KB_TAB")
    ids = [int(i, 16) for i in re.findall(r'\{\s*(0x[0-9a-fA-F]+)\s*,', body)]
    ids += [0x29] * body.count("{KB_KINGDOM_KEY,") + [0x1e0] * body.count("{KB_STAR_SEEKER,")
    if len(ids) < 28:
        die("could not read KB_TAB from runtime/sora_kb.h (%d rows)" % len(ids))
    return ids


def table(kh2):
    rows = []
    for item, st, name in kbfx_rows():
        own = not st or st[1] == "0"
        mdl = "W_EX010%s.mdlx" % ("_" + st.upper() if own and st else "")
        p = kh2_obj(kh2, mdl)
        if not p:
            die("no obj/%s in the KH2 extract %s" % (mdl, kh2))
        rows.append((item, name, mdl[:-5], own, spheres(p)))
    have = {r[0] for r in rows}
    miss = [i for i in kb_ids() if i not in have]
    if miss:
        die("KB_TAB ids without a KBFX_TAB row: %s" % " ".join("0x%x" % i for i in miss))
    return rows


def header(rows):
    sets, first = [], {}
    for _, _, _, _, sp in rows:
        k = tuple(sp)
        if k not in first:
            first[k] = sum(len(s) for s in sets)
            sets.append(sp)
    L = [
        "// sora_kbreach_tab.h (s249 kbreach): each KH2 Keyblade's own ATTACK SPHERES. GENERATED by tools/kbreach_gen.py from the KH2",
        "// extract (do not edit by hand: re-run it). The logic is runtime/sora_kbreach.h. Notes: recon/s249/S249_HELPER_KBREACH.md.",
        "//",
        "// KH2 data: obj/W_EX010_<set>.mdlx collision rows of type 6 (attack), all spheres on bone 2 (the grip): group, cm along the",
        "// blade (0 = the grip, + toward the tip, - = the pommel side), radius cm. KH2 hits, while a window (atkp, group) is live, with",
        "// every sphere of that group (exe FUN_14040f610). Groups 1 / 2 differ per Keyblade; 3..7 are the same in every model.",
        "#pragma once",
        "",
        "typedef struct { uint8_t grp; int16_t cm; uint8_t r; } KbreachSph;  // one attack sphere: group, centre cm along the blade, radius cm",
        "typedef struct { uint16_t id; uint8_t first, n; bool own; } KbreachRow;  // own = the item has a KH2 model (else the Kingdom Key's)",
        "static const KbreachSph KBREACH_SPH[] = {",
    ]
    for sp in sets:
        names = [r[1] for r in rows if tuple(r[4]) == tuple(sp) and r[3]]
        L.append("    // %d: %s" % (first[tuple(sp)], ", ".join(names) if names else "?"))
        for g, cm, r in sp:
            L.append("    {%d, %4d, %3d}," % (g, cm, r))
    if sum(len(s) for s in sets) > 255:
        die("%d spheres: KbreachRow.first is a u8" % sum(len(s) for s in sets))
    L += ["};", "static const KbreachRow KBREACH_TAB[] = {"]
    for item, name, mdl, own, sp in rows:
        L.append("    {0x%03x, %3d, %2d, %-5s},  // %s (%s)" % (item, first[tuple(sp)], len(sp), "true" if own else "false", name,
                                                       mdl if own else "ours: no KH2 model, the Kingdom Key's spheres"))
    L += ["};", "#define KBREACH_NSPH ((int)(sizeof KBREACH_SPH / sizeof KBREACH_SPH[0]))",
          "#define KBREACH_N ((int)(sizeof KBREACH_TAB / sizeof KBREACH_TAB[0]))", ""]
    return "\n".join(L)


def summary(sp, g):
    s = [x for x in sp if x[0] == g]
    return "/".join(str(c) for _, c, _ in sorted(s, key=lambda x: x[1])) + " r" + "/".join(sorted({str(r) for _, _, r in s}))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kh2", required=True)
    ap.add_argument("--out", default=os.path.join(ROOT, "runtime", "sora_kbreach_tab.h"))
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    rows = table(a.kh2)
    h = header(rows)
    for item, name, mdl, own, sp in rows:
        print("  0x%03x %-22s g1 %-22s g2 %-18s%s" % (item, name, summary(sp, 1), summary(sp, 2), "" if own else "  (Kingdom Key's)"))
    if a.check:
        old = open(a.out, encoding="utf-8").read() if os.path.isfile(a.out) else ""
        print("kbreach_gen: %s is %s" % (a.out, "up to date" if old == h else "DIFFERENT"))
        sys.exit(0 if old == h else 1)
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write(h)
    print("kbreach_gen: %d rows, %d distinct sphere sets -> %s" % (len(rows), len({tuple(r[4]) for r in rows}), a.out))


if __name__ == "__main__":
    main()
