#!/usr/bin/env python3
"""(s216 kbfx) The mod's own two Keyblades get their OWN swing ribbon, hit sparks, hit sound and (Sealing Blade) beam: APPENDED rows
of fx.bin / sfx.bin. Nothing old is touched (checked at the end, byte for byte, like chickfx.py).

  python -E -P kbownfx.py --fx-in <fx.bin> --sfx-in <sfx.bin> --out <dir> --ours <tools/sorabuild/ours/kb> [--no-fx] [--no-sfx]
                          [--part s216|s247]

Inputs: the running packs of the fx chain (fx.bin already holds KH2's per-Keyblade sets kb<xx>_* of tools/sorabuild/fx/fxbake.py,
made at install from the player's own KH2 data) + our synthesised WAVs (ours/kb/kbsynth.py). No KH2 / BotW byte is in this file
or in the WAVs.

EFFECTS (fx.bin, SKFX v1). Set names: Sealing Blade "s1", Guardian Core "g1" (KH2's own sets all end in '0').
  kbs1_hit / _hit_finish / _hit_guard   = Fenrir's (kbm0_*) sparks, same frames / motion / size / timing (the placeholder the user
                                          liked), RECOLOURED: every texture they use becomes a grey copy (its brightness: max of
                                          r g b, alpha kept) and the hue moves to the vertex colours: core = blue-white
                                          178 235 255, deep = indigo 92 100 230, accent = gold 255 206 96 (Fenrir's warm parts:
                                          its orange embers / gold stars / finisher ring -> gold sparkle).
  kbg1_hit / _hit_finish / _hit_guard   = Photon Debugger's (kb90_*) sparks recoloured the same way: core = cyan 76 235 255,
                                          deep = 24 150 190, accent = orange 255 122 15 (the particle-atlas embers + the
                                          finisher's ring).
  kbs1_trail / kbg1_trail               = kbm0_trail / kb90_trail (the runtime reads only the first batch's texture + blend and the
                                          first vertex's colour) with a NEW ribbon texture made from the source ribbon's
                                          brightness: Sealing = indigo -> blue-white -> white + gold sparkle dots; Guardian =
                                          dark teal -> cyan -> white-cyan + an orange line along the v 0 edge (the tip, GUESS
                                          default of sora_fx.c). Vertex colour white (the texture carries the colour).
  kbs1_beam                             = PROCEDURAL (ours): a flying blue-white crescent, 150 cm wide, a 32-frame LOOP,
                                          unbound (pax 0, bone 0) like mk_pearl_ball: draw it with sora_fx_spawn at the beam's
                                          point + sora_fx_move every frame, yaw = its travel direction (the crescent's convex
                                          side faces +z = forward, its ends trail 35 cm back and droop 43 cm); ghost copies at
                                          vertex age 2 / 4 / 6 and gold sparkles at age 0..10 lag along the flight path.
  kbs1_beam_hit                         = kbs1_hit_finish's frames scaled x1.5 + a procedural expanding ground ring (blue glow
                                          + a later thin gold ring), one-shot, 50 frames.
  kbs1_beam_r000 .. kbs1_beam_r165      = (s217) PROCEDURAL: the crescent lying in the swing's plane (flat C, convex side + sharp
                                          core forward, glow trailing; a second band across the plane so it never vanishes
                                          edge-on), pre-rolled about the travel axis in 15 deg steps (sora_fx.c is exe side and
                                          places unbound elements with a yaw only). Same loop / ghosts / sparkles / colours as
                                          kbs1_beam, which stays as it was (sora_kbbeam.h: "kbbeamroll 0").
  New textures (grey copies, the two ribbons, our band + sparkle) are inserted BEFORE the pack's last texture (the ribbon gradient
  sora_fx.c emit_trail uses); one identical to an existing texture is reused. Old ids / frames / batches / verts / textures keep
  their indices.
SOUNDS (sfx.bin, SKSF v1): + kbs1_hit, kbg1_hit, kbs1_beam, kbs1_beam_hit (keys "ours/kb/<name>.wav", names = the same) and the
  marker name "kbse_v1". No trigger / voice / hit-table row changes.

(s247 kbfx2) --part s247 (its own chain step fxsfx_kbown2, after the party steps): the user's Silent Princess "p1" and Calamity's
Gaze "c1", ALL PROCEDURAL (our shapes + colours, nothing taken from a source element), camera-facing (batch flags 1) like KH2's
sparks, so the flower / the eye always face the camera:
  kbp1_hit / _hit_finish / _hit_guard   = a flower blooming open (6 / 6 + 5 inner / 5 petals fanning out from a bud: an additive
                                          blue glow under each, an alpha white-cyan petal with a lilac tip), a bright centre,
                                          sparkle dust; the finisher x1.7 + a soft blue light burst and a light ring.
  kbp1_trail                            = our ribbon (ADDITIVE): white core at the tip edge, cyan-white body, lilac toward the old end.
  kbp1_shed_0..2                        = 2 / 2 / 3 glowing petals drifting down, swaying, fluttering, fading (48 frames).
  kbc1_hit / _hit_finish / _hit_guard   = a Malice splash (black-purple goo blobs flung out and falling, alpha, with magenta rims;
                                          dark smoke puffs; a magenta flash) and the pink-red slit eye opening in a dark cloud,
                                          then closing (atlas of 6 lid stages); the finisher x1.6 + a dark shockwave ring with a
                                          magenta edge; the guard hit = a small splash, no eye.
  kbc1_trail                            = our ribbon (ALPHA): smoky black-purple body breaking into wisps, magenta edges.
  kbc1_shed_0..2                        = 3 / 3 / 2 magenta embers rising a little, flickering, dying (36 frames).
  Sounds kbp1_hit, kbc1_hit + the marker "kbse_v2".
"""
import argparse
import hashlib
import mmap
import os
import struct
import sys
import traceback
import wave

import numpy as np

OUT_TEX = struct.Struct("<IIII")
OUT_EFF = struct.Struct("<32sIIIIiIII")
OUT_FRAME = struct.Struct("<II")
OUT_BATCH = struct.Struct("<HBBB3BII9f")
OUT_HDR = struct.Struct("<4s15I")
VERT = np.dtype([("p", "<f4", 3), ("off", "<f4", 3), ("uv", "<f4", 2), ("age", "<u2"), ("pad", "<u2"), ("rgba", "u1", 4)])
assert VERT.itemsize == 40 and OUT_BATCH.size == 52 and OUT_EFF.size == 64
SK = struct.Struct("<4s11I")

PAL = {  # role -> rgb (GUESS colours, from the approved looks recon/s212/S212_HELPER_KEYBLADES.md)
    "s1": {"core": (178, 235, 255), "deep": (92, 100, 230), "accent": (255, 206, 96)},
    "g1": {"core": (76, 235, 255), "deep": (24, 150, 190), "accent": (255, 122, 15)},
}
SRC = {"s1": "m0", "g1": "90"}  # the placeholder set each one is shaped from (the feel the user approved)
KINDS = ["hit", "hit_finish", "hit_guard"]
# Which colour each (texture, blend) group of the source sparks takes (read from the live pack, s216: mean / max vertex colours per
# group + the textures as pictures). blend None = any. A texture of another size than TEX_WH (a different pack) -> "auto".
# auto = the hue rule: warm and saturated (Fenrir's gold-tinted finisher slashes) -> accent, else core.
TEX_WH = {0: (256, 512), 1: (128, 128), 2: (128, 64), 3: (128, 64), 57: (128, 64), 58: (128, 64), 59: (128, 64), 90: (128, 64),
          91: (128, 64), 92: (128, 128), 93: (128, 128)}
ROLE_TAB = {
    # Fenrir (kbm0_*): 0 = the particle atlas (orange embers), 2 = the finisher's ring arc, 92 add = gold stars -> gold; 3 = the big
    # translucent wave behind the burst -> indigo haze; 1 / 90 / 92 alpha / 93 = flash, streaks, pale stars, the finisher's sparkle
    # cloud -> blue-white
    "s1": {(0, None): "accent", (2, None): "accent", (92, 0): "accent", (93, None): "core", (3, None): "deep",
           (1, None): "core", (90, None): "core", (92, 1): "core", (91, None): "auto"},
    # Photon Debugger (kb90_*): 0 = the particle atlas (embers) + 2 = the finisher's ring -> orange; 58 alpha (blended stars) -> deep
    # cyan; 1 / 57 / 58 add / 59 (flash, streaks, stars, the circuit lines) -> cyan
    "g1": {(0, None): "accent", (2, None): "accent", (58, 1): "deep", (1, None): "core", (57, None): "core", (58, 0): "core",
           (59, None): "core"},
}
# The shapes that made the placeholder read as Fenrir's / Photon Debugger's (their stars) are redrawn as ours: Fenrir's 5-point stars
# (92, and the square cells of the 91 and 0 atlases: the 0 atlas's cells are the gold embers) -> four-point glints; Photon
# Debugger's star outlines (58 atlas) -> hexagons. (Guardian keeps the 0 atlas as it is: its round flare is the orange burst.)
RESHAPE = {"s1": {0: "glint", 91: "glint", 92: "glint"}, "g1": {58: "hex"}}
# (s217 beam) the crescent that lies in the swing's plane, pre-rolled: sora_fx.c (exe side) places an unbound element with a yaw
# only, so each roll about the travel axis is its own element (APPENDED after kbs1_beam_hit; every s216 row stays as it was).
# kbs1_beam_r<deg> = the flat crescent rolled by <deg> (0 = flat, 90 = upright; 180 = 0 again, the shape is symmetric).
BEAM_ROLL_STEP = 15
BEAM_ROLLS = list(range(0, 180, BEAM_ROLL_STEP))
BEAM_ROLL_FX = ["kbs1_beam_r%03d" % r for r in BEAM_ROLLS]
NEW_FX = (["kbs1_" + k for k in KINDS] + ["kbs1_trail"] + ["kbg1_" + k for k in KINDS] + ["kbg1_trail", "kbs1_beam", "kbs1_beam_hit"]
          + BEAM_ROLL_FX)
SOUNDS = ["kbs1_hit", "kbg1_hit", "kbs1_beam", "kbs1_beam_hit"]
SFX_MARKER = "kbse_v1"
# (s247 kbfx2) PART "s247": the user's two new Keyblades, Silent Princess "p1" (0x7e03) and Calamity's Gaze "c1" (0x7e04). ALL
# PROCEDURAL (our shapes, our colours; no source element), appended by its own chain step (fxsfx_kbown2, after fx_party /
# sfx_party2) so a fresh install makes the same bytes as the live packs + this part. "<set>_shed_<k>" = the one-shot bits the
# runtime (sora_kbfx.h kbfx_shed) drops off the blade while its ribbon is drawn: petals (p1) / embers (c1).
SHED_N = 3
NEW_FX_S247 = (["kbp1_" + k for k in KINDS] + ["kbp1_trail"] + ["kbp1_shed_%d" % k for k in range(SHED_N)]
               + ["kbc1_" + k for k in KINDS] + ["kbc1_trail"] + ["kbc1_shed_%d" % k for k in range(SHED_N)])
SOUNDS_S247 = ["kbp1_hit", "kbc1_hit"]
SFX_MARKER_S247 = "kbse_v2"
PARTS = {"s216": (NEW_FX, SOUNDS, SFX_MARKER), "s247": (NEW_FX_S247, SOUNDS_S247, SFX_MARKER_S247)}


def die(msg):
    raise SystemExit("[kbownfx] " + msg)


# (s217 review F2) Every float this tool GENERATES (procedural positions / offsets / UVs, and the alphas / colours it computes before
# they become bytes) goes through an integer first: the release pins fx.bin by sha256, and sin / cos / exp / ** may differ in the last
# bit on another PC's C runtime / CPU. Near-cancellations (x cos r - y sin r that should be 0 came out as 1e-15 garbage: 26,031 such
# position floats in the s217 pack) then become exact 0, everything else an exact multiple of 1/Q (exact in float32 for |v| < 16384).
# Values copied from KH2's own elements are NOT touched (they stay byte-identical). The 0..255 values use a fine grid: it only has
# to catch values a hair from an int() / round() edge (which a last bit could push across); a coarse one would itself move values.
Q_POS = 1024      # positions + offsets, KH cm: half a quantum = 0.0005 cm
Q_UV = 1 << 16    # UVs (0..1)
Q_VAL = 1 << 30   # alphas / colours (0..255) before int() / round()


def qz(a, q):
    """a (any shape) -> float64 multiples of 1/q (from int64, so -0.0 and garbage near 0 become +0.0)"""
    return np.rint(np.asarray(a, np.float64) * q).astype(np.int64) / q


def rnd(a):
    """a generated 0..255 value -> its whole number: on the 2^-30 grid first, then half UP (a value a hair above .5 that the s217
    pack rounded up stays up: the pack the user saw keeps its pixels)"""
    return np.floor(qz(a, Q_VAL) + 0.5)


def qverts(v):
    """a generated VERT array, ready to write: p / off / uv on their grids (once, last: after any roll)"""
    v["p"] = qz(v["p"], Q_POS)
    v["off"] = qz(v["off"], Q_POS)
    v["uv"] = qz(v["uv"], Q_UV)
    return v


# ================================================================================================================ fx: reading
class Pack:
    def __init__(self, path):
        if not os.path.isfile(path):
            die("no fx pack at %s" % path)
        self.f = open(path, "rb")
        self.mm = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
        if len(self.mm) < 64:
            die("%s is too small for an SKFX pack" % path)
        h = OUT_HDR.unpack_from(self.mm, 0)
        if h[0] != b"SKFX" or h[1] != 1:
            die("%s is not an SKFX v1 pack" % path)
        self.h = h
        self.ntex, self.neff, self.nfr, self.nba, self.nve, self.ntr = h[2:8]
        self.o_tex, self.o_eff, self.o_fr, self.o_ba, self.o_ve, self.o_tr, self.o_pix, self.pixb = h[8:16]
        if self.o_pix + self.pixb > len(self.mm):
            die("%s is cut short" % path)
        self.tex = [OUT_TEX.unpack_from(self.mm, self.o_tex + i * 16) for i in range(self.ntex)]
        self.eff = {}
        for i in range(self.neff):
            r = OUT_EFF.unpack_from(self.mm, self.o_eff + i * 64)
            self.eff[r[0].split(b"\0")[0].decode("latin1")] = r[1:]

    def pixels(self, t):
        w, h, off, _ = self.tex[t]
        return np.frombuffer(self.mm[self.o_pix + off:self.o_pix + off + w * h * 4], np.uint8).reshape(h, w, 4)  # BGRA

    def element(self, name):
        """[(batch tuple, verts ndarray copy)] per frame"""
        ff, nf = self.eff[name][0], self.eff[name][1]
        out = []
        for k in range(nf):
            fb, n = OUT_FRAME.unpack_from(self.mm, self.o_fr + (ff + k) * 8)
            fr = []
            for b in range(fb, fb + n):
                bt = OUT_BATCH.unpack_from(self.mm, self.o_ba + b * 52)
                v = np.frombuffer(self.mm[self.o_ve + bt[7] * 40:self.o_ve + (bt[7] + bt[8]) * 40], VERT).copy()
                fr.append((bt, v))
            out.append(fr)
        return out

    def close(self):
        self.mm.close()
        self.f.close()


# ================================================================================================================ fx: textures
def grey(px):
    """brightness copy: rgb = max(r, g, b), alpha kept (the hue moves to the vertex colours)"""
    v = px[:, :, :3].max(axis=2)
    out = px.copy()
    out[:, :, 0] = out[:, :, 1] = out[:, :, 2] = v
    return out


def ramp(g, stops):
    """g 0..1 (h, w) -> rgb (h, w, 3) through the (pos, (r, g, b)) stops"""
    pos = np.array([s[0] for s in stops])
    out = np.zeros(g.shape + (3,))
    for c in range(3):
        out[..., c] = np.interp(g, pos, [s[1][c] for s in stops])
    return out


def trail_tex(px, which):
    """our ribbon texture from a source ribbon's brightness (BGRA in, BGRA out)"""
    h, w = px.shape[:2]
    g = px[:, :, :3].max(axis=2).astype(np.float64) / 255
    a = px[:, :, 3].astype(np.float64)
    if which == "s1":
        rgb = ramp(g, [(0.0, (70, 80, 210)), (0.45, (178, 235, 255)), (1.0, (240, 250, 255))])
        rng = np.random.default_rng(2165)
        for _ in range(max(12, w * h // 200)):  # gold sparkle dots, older (larger u) ones fewer and fainter
            u = float(qz(rng.random() ** 1.6 * 0.8, Q_VAL))
            x, y = int(u * w), int((0.12 + 0.76 * rng.random()) * h)
            fade = max(0.0, 1 - u / 0.8)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < w and 0 <= yy < h:
                        k = 1.0 if dx == dy == 0 else 0.45
                        rgb[yy, xx] = rgb[yy, xx] * (1 - k) + np.array((255, 214, 110)) * k
                        a[yy, xx] = max(a[yy, xx], 235 * fade * k)
    else:
        rgb = ramp(g, [(0.0, (10, 70, 90)), (0.45, (76, 235, 255)), (1.0, (220, 255, 255))])
        r0, r1 = max(1, int(h * 0.04)), max(2, int(h * 0.13))  # the orange line along the v 0 (tip) edge
        for y in range(r0, r1):
            k = 0.9 * (1 - abs((y - (r0 + r1) / 2) / ((r1 - r0) / 2 + 0.5)))
            rgb[y] = rgb[y] * (1 - k) + np.array((255, 122, 15)) * k
    out = np.zeros_like(px)
    out[:, :, 0], out[:, :, 1], out[:, :, 2] = rgb[..., 2], rgb[..., 1], rgb[..., 0]
    out[:, :, 3] = np.clip(rnd(a), 0, 255)
    return out


def components(mask):
    """4-connected components of a bool mask -> [(ys, xs)] (small textures: a plain flood fill)"""
    h, w = mask.shape
    seen = np.zeros_like(mask)
    out = []
    for y0 in range(h):
        for x0 in range(w):
            if not mask[y0, x0] or seen[y0, x0]:
                continue
            stack, ys, xs = [(y0, x0)], [], []
            seen[y0, x0] = True
            while stack:
                y, x = stack.pop()
                ys.append(y), xs.append(x)
                for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                    if 0 <= yy < h and 0 <= xx < w and mask[yy, xx] and not seen[yy, xx]:
                        seen[yy, xx] = True
                        stack.append((yy, xx))
            out.append((np.array(ys), np.array(xs)))
    return out


def shape_alpha(kind, x, y, outline):
    """our shapes on -1..1 coords: "glint" = a four-point glint (long thin rays, the vertical one longest, a round core: the
    Sealing Blade's holy sparkle), "hex" = a hexagon (Guardian tech: an outline ring + a centre dot, or a filled plate with a
    bright rim)"""
    if kind == "glint":
        core = np.exp(-(x * x + y * y) / 0.04)
        rays = np.exp(-np.abs(x) / 0.06) * np.clip(1 - np.abs(y), 0, 1) ** 1.5 +             0.75 * np.exp(-np.abs(y) / 0.06) * np.clip(1 - np.abs(x) / 0.8, 0, 1) ** 1.5
        diag = 0.3 * np.exp(-np.abs(x - y) / 0.05) * np.exp(-np.abs(x + y) / 0.35) +             0.3 * np.exp(-np.abs(x + y) / 0.05) * np.exp(-np.abs(x - y) / 0.35)
        return np.clip(core + rays + diag, 0, 1)
    d = np.maximum(np.abs(x), np.abs(x) * 0.5 + np.abs(y) * 0.866)  # 1 = the hexagon's edge
    rim = np.clip(1 - np.abs(d - 0.86) / 0.1, 0, 1)
    if outline:
        return np.clip(rim + np.clip(1 - d / 0.18, 0, 1), 0, 1)
    return np.clip(np.where(d < 0.86, 0.5, 0) + rim, 0, 1)


def reshape(px, kind):
    """keep the texture's layout (where its shapes sit: KH2 UVs point there), draw OUR shape in each roughly square shape's box"""
    out = px.copy()
    a = px[:, :, 3]
    n = 0
    for ys, xs in components(a > 24):
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        bw, bh = x1 - x0, y1 - y0
        if min(bw, bh) < 6 or max(bw, bh) > 1.6 * min(bw, bh):
            continue  # streaks / tiny dots stay as they are
        fill = len(ys) / float(bw * bh)
        amax = float(a[ys, xs].max())
        out[ys, xs, 3] = 0
        gy, gx = np.mgrid[y0:y1, x0:x1]
        sx, sy = (gx + 0.5 - (x0 + x1) / 2) / (bw / 2), (gy + 0.5 - (y0 + y1) / 2) / (bh / 2)
        sa = shape_alpha(kind, sx, sy, fill < 0.3) * amax
        reg = out[y0:y1, x0:x1]
        reg[:, :, 3] = np.maximum(reg[:, :, 3], rnd(sa).astype(np.uint8))
        reg[:, :, :3] = np.where((sa > 0)[..., None], 255, reg[:, :, :3])
        n += 1
    return out, n


def band_tex(w=64, h=32):
    """ours: a soft band, bright in the middle across v, a faint streak grain along u (white; the colour is the vertex's)"""
    rng = np.random.default_rng(2166)
    v = (np.arange(h) + 0.5) / h
    across = np.sin(np.pi * v) ** 1.5
    grain = 0.82 + 0.18 * np.interp(np.arange(w), np.linspace(0, w, 9), rng.random(9))
    a = np.outer(across, grain) * 255
    out = np.full((h, w, 4), 255, np.uint8)
    out[:, :, 3] = np.clip(rnd(a), 0, 255)
    return out


def sparkle_tex(n=32):
    """ours: a four-point star with a round core (white)"""
    c = (np.arange(n) + 0.5) / n * 2 - 1
    x, y = np.meshgrid(c, c)
    core = np.exp(-(x * x + y * y) / 0.03)
    arms = np.exp(-np.abs(x) / 0.05) * np.exp(-np.abs(y) / 0.45) + np.exp(-np.abs(y) / 0.05) * np.exp(-np.abs(x) / 0.45)
    a = np.clip(core + 0.8 * arms, 0, 1) * np.clip(1 - np.sqrt(x * x + y * y), 0, 1) ** 0.5
    out = np.full((n, n, 4), 255, np.uint8)
    out[:, :, 3] = np.clip(rnd(a * 255), 0, 255)
    return out


class Textures:
    """new textures (deduped against the old pack and each other); refs are ("old", i) or ("new", k)"""

    def __init__(self, pk):
        self.pk = pk
        self.by_hash = {}
        for i, (w, h, off, _) in enumerate(pk.tex[:-1]):
            self.by_hash.setdefault((w, h, hashlib.md5(pk.mm[pk.o_pix + off:pk.o_pix + off + w * h * 4]).digest()), ("old", i))
        self.new = []  # (w, h, bytes)
        self.grey_of = {}

    def add(self, px):
        h, w = px.shape[:2]
        b = np.ascontiguousarray(px, np.uint8).tobytes()
        key = (w, h, hashlib.md5(b).digest())
        if key not in self.by_hash:
            self.by_hash[key] = ("new", len(self.new))
            self.new.append((w, h, b))
        return self.by_hash[key]

    def grey(self, t, kind=None):
        if (t, kind) not in self.grey_of:
            px = grey(self.pk.pixels(t))
            if kind:
                px, n = reshape(px, kind)
                print("    texture %d: %d shapes redrawn as our %s" % (t, n, kind), flush=True)
            self.grey_of[(t, kind)] = self.add(px)
        return self.grey_of[(t, kind)]


# ================================================================================================================ fx: recolour
def recolour_element(pk, frames, which, log):
    """frames of a source spark element -> the same frames, grey textures + our palette in the vertex colours.
    Each (texture, blend) group of the element gets ONE role (from its alpha-weighted mean vertex colour over all frames, so a
    group never changes colour mid-effect); every vertex keeps its own brightness (max of r g b) and, partly, its whiteness."""
    pal = {k: np.array(v, np.float64) for k, v in PAL[which].items()}
    acc = {}
    for fr in frames:
        for bt, v in fr:
            c = v["rgba"][:, :3].astype(np.float64)
            wgt = v["rgba"][:, 3].astype(np.float64) + 1e-3
            s = acc.setdefault((bt[0], bt[2]), [np.zeros(3), 0.0])
            s[0] += (c * wgt[:, None]).sum(0)
            s[1] += wgt.sum()
    roles = {}
    for (t, blend), (cs, ws) in acc.items():
        em = cs / max(ws, 1e-6)
        tab = ROLE_TAB[which]
        role = tab.get((t, blend), tab.get((t, None), "auto"))
        if role != "auto" and t != 0xFFFF and tuple(pk.tex[t][:2]) != TEX_WH.get(t):
            log("    NOTE: texture %d is %dx%d, not the expected %s: hue rule" % ((t,) + tuple(pk.tex[t][:2]) + (TEX_WH.get(t),)))
            role = "auto"
        if role == "auto":
            V, S = em.max(), (em.max() - em.min()) / max(em.max(), 1e-6)
            role = "accent" if (em[0] > em[2] + 15 and S > 0.3 and V >= 90) else "core"
        roles[(t, blend)] = role
    out = []
    for fr in frames:
        nf = []
        for bt, v in fr:
            t, blend = bt[0], bt[2]
            role = roles[(t, blend)]
            e = v["rgba"][:, :3].astype(np.float64)
            Vv = e.max(1)
            Sv = (Vv - e.min(1)) / np.maximum(Vv, 1e-6)
            sat = {"core": 0.55 + 0.45 * Sv, "deep": 0.85 + 0.15 * Sv, "accent": 0.8 + 0.2 * Sv}[role]
            P = pal[role] / pal[role].max()
            rgb = Vv[:, None] * ((1 - sat)[:, None] + sat[:, None] * P[None, :])
            nv = v.copy()
            nv["rgba"][:, :3] = np.clip(np.round(rgb), 0, 255).astype(np.uint8)
            rk = RESHAPE[which].get(t) if t != 0xFFFF and tuple(pk.tex[t][:2]) == TEX_WH.get(t) else None
            nf.append((bt, ("grey", t, rk) if t != 0xFFFF else None, nv))
        out.append(nf)
    log("    roles (tex/blend) -> %s" % ", ".join("%s/%d %s" % (t if t != 0xFFFF else "-", b, r) for (t, b), r in sorted(roles.items())))
    return out


def resolve_frames(tx, frames):
    """("grey", t) texture refs -> Textures refs"""
    out = []
    for fr in frames:
        out.append([(bt, None if r is None else tx.grey(r[1], r[2]) if r[0] == "grey" else r, v) for bt, r, v in fr])
    return out


# ================================================================================================================ fx: procedural
def mkverts(pts, uvs, rgba, off=None, age=0):
    """(only the procedural generators make verts here; their callers put them on the grid with qverts, review F2)"""
    n = len(pts)
    v = np.zeros(n, VERT)
    v["p"] = pts
    if off is not None:
        v["off"] = off
    v["uv"] = uvs
    v["age"] = age
    v["rgba"] = rgba
    return v


def strip(cent, nrm, width, rgba_a, color, age=0):
    """a band along the centre points (N+1, 3) with in-plane normals (N+1, 3) and widths (N+1,): triangle list"""
    n = len(cent) - 1
    o, i_ = cent + nrm * (width[:, None] / 2), cent - nrm * (width[:, None] / 2)
    pts, uvs, cols = [], [], []
    for k in range(n):
        u0, u1 = k / n, (k + 1) / n
        quad = [(o[k], (u0, 0.0), rgba_a[k]), (i_[k], (u0, 1.0), rgba_a[k]), (o[k + 1], (u1, 0.0), rgba_a[k + 1]),
                (i_[k + 1], (u1, 1.0), rgba_a[k + 1])]
        for q in (0, 1, 2, 1, 3, 2):
            pts.append(quad[q][0]), uvs.append(quad[q][1]), cols.append(tuple(color) + (int(qz(quad[q][2], Q_VAL)),))
    return mkverts(np.array(pts), np.array(uvs), np.array(cols), age=age)


def crescent(nseg=24, span=150.0, theta_m=np.radians(60), back=35.0):
    """the beam's centre line in KH effect space (x, y DOWN, z forward), cm: an arch facing +z"""
    th = np.linspace(-theta_m, theta_m, nseg + 1)
    R = span / 2 / np.sin(theta_m)
    up = R * np.cos(th) - R + 20.0   # top 20 cm above the point, the ends ~23 cm below it
    x = R * np.sin(th)
    z = 10.0 - back * (1 - np.cos(th)) / (1 - np.cos(theta_m))
    cent = np.stack([x, -up, z], 1)
    nrm = np.stack([np.sin(th), -np.cos(th), np.zeros_like(th)], 1)  # radial, in the x-y plane (y down)
    taper = np.cos(th / theta_m * np.pi / 2) ** 0.8
    return th, cent, nrm, taper


def quads_camera(pts, size, rgba, age):
    """camera-facing quads (flags 1: the offsets are camera space) at pts (n, 3), sizes (n,), rgba (n, 4), ages (n,)"""
    P, O, U, C, A = [], [], [], [], []
    corners = [(-1, -1, 0, 0), (1, -1, 1, 0), (-1, 1, 0, 1), (1, 1, 1, 1)]
    for p, s, c, ag in zip(pts, size, rgba, age):
        q = [(p, (cx * s, cy * s, 0.0), (u, w)) for cx, cy, u, w in corners]
        for k in (0, 1, 2, 1, 3, 2):
            P.append(q[k][0]), O.append(q[k][1]), U.append(q[k][2]), C.append(c), A.append(ag)
    v = mkverts(np.array(P), np.array(U), np.array(C), off=np.array(O))
    v["age"] = np.array(A)
    return v


def beam_frames(band, spark, nf=32):
    th, cent, nrm, taper = crescent()
    rng = np.random.default_rng(2167)
    ns = 14
    sp_t = rng.uniform(-0.9, 0.9, ns)          # where along the arc (fraction of theta_m)
    sp_r = rng.uniform(-0.5, 0.5, ns)          # across the band (fraction of the glow width)
    sp_age = rng.integers(0, 11, ns)           # lag along the flight path (frames)
    sp_n = rng.integers(1, 3, ns)              # twinkles per loop
    sp_ph = rng.random(ns)
    sp_sz = rng.uniform(4.0, 8.0, ns)
    frames = []
    for f in range(nf):
        ph = 2 * np.pi * f / nf
        fr = []
        wg = 34.0 * taper * (1 + 0.08 * np.sin(2 * ph))
        fr.append((band, 0, strip(cent, nrm, wg, 150 * taper ** 0.5, (120, 170, 255))))
        wc = 12.0 * taper
        ac = 255 * (0.9 + 0.1 * np.sin(3 * ph)) * taper ** 0.3
        fr.append((band, 0, strip(cent, nrm, wc, ac, (230, 246, 255))))
        for ag, k in ((2, 0.55), (4, 0.3), (6, 0.15)):  # ghosts: drawn at the owner's pose `ag` frames ago = along its path
            fr.append((band, 0, strip(cent, nrm, wc * (1 + 0.15 * ag / 2), ac * k, (150, 190, 255), age=ag)))
        th_k = sp_t * th[-1]
        idx = np.interp(th_k, th, np.arange(len(th)))
        pc = np.stack([np.interp(idx, np.arange(len(th)), cent[:, c]) for c in range(3)], 1)
        nk = np.stack([np.interp(idx, np.arange(len(th)), nrm[:, c]) for c in range(3)], 1)
        wk = np.interp(idx, np.arange(len(th)), wg)
        pts = pc + nk * (sp_r * wk)[:, None]
        tw = 0.5 + 0.5 * np.sin(2 * np.pi * (sp_n * f / nf + sp_ph))
        cols = np.array([(255, 212, 110, int(qz(255 * x, Q_VAL))) for x in tw])
        fr.append((spark, 1, quads_camera(pts, sp_sz * (0.7 + 0.3 * tw), cols, sp_age)))
        frames.append([(t, fl, qverts(v)) for t, fl, v in fr])
    return frames


def crescent_flat(nseg=24, span=150.0, theta_m=np.radians(60), back=35.0):
    """(s217) the swing-plane crescent's centre line, FLAT (roll 0): in the x-z plane of KH effect space (x right, y down,
    z forward), cm, its middle 10 cm ahead of the point, its ends 35 cm further back (convex side = +z = the travel direction,
    the sharp edge leads). -> th, cent, in-plane outward normal, taper"""
    th = np.linspace(-theta_m, theta_m, nseg + 1)
    R = span / 2 / np.sin(theta_m)
    k = back / (1 - np.cos(theta_m))
    cent = np.stack([R * np.sin(th), np.zeros_like(th), 10.0 - k * (1 - np.cos(th))], 1)
    tg = np.stack([R * np.cos(th), np.zeros_like(th), -k * np.sin(th)], 1)
    tg /= np.linalg.norm(tg, axis=1)[:, None]
    nrm = np.stack([-tg[:, 2], np.zeros_like(th), tg[:, 0]], 1)  # in the plane, outward (forward at the middle)
    taper = np.cos(th / theta_m * np.pi / 2) ** 0.8
    return th, cent, nrm, taper


def roll_verts(v, deg):
    """turn every point about the z (travel) axis by deg: the crescent's width goes from right (KH +x) toward up (KH -y).
    In sora_fx.c's world: width = cos(deg) x Sora's right + sin(deg) x up (right = (-cos yaw, 0, sin yaw) in BotW x, z).
    Camera-space offsets (the sparkles' billboard corners) are not turned."""
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    x, y = v["p"][:, 0].copy(), v["p"][:, 1].copy()
    v["p"][:, 0] = x * c + y * s
    v["p"][:, 1] = -x * s + y * c
    return v


def beam_flat_frames(band, spark, roll, nf=32):
    """(s217) kbs1_beam_r<roll>: the crescent in the swing plane. Two bands crossed at right angles (one in the crescent's plane,
    one across it) so it never vanishes edge-on: seen along the travel direction a flat one is a thin level line of light.
    The glow trails behind the sharp core (the leading edge); ghosts at age 2 / 4 / 6 (both bands) and 14 gold sparkles at
    age 0..10 trail along the flight path (sora_fx.c's pose ring), as kbs1_beam. Same colours as kbs1_beam. Loop, nf frames."""
    th, cent, nrm, taper = crescent_flat()
    up = np.tile(np.array([0.0, 1.0, 0.0]), (len(th), 1))  # across the plane (KH y, down; the band is symmetric)
    rng = np.random.default_rng(2167)
    ns = 14
    sp_t = rng.uniform(-0.9, 0.9, ns)          # where along the arc (fraction of theta_m)
    sp_r = rng.uniform(-0.5, 0.3, ns)          # across the band (fraction of the glow width; mostly on the trailing side)
    sp_y = rng.uniform(-0.5, 0.5, ns)          # across the plane (fraction of the cross glow width)
    sp_age = rng.integers(0, 11, ns)           # lag along the flight path (frames)
    sp_n = rng.integers(1, 3, ns)              # twinkles per loop
    sp_ph = rng.random(ns)
    sp_sz = rng.uniform(4.0, 8.0, ns)
    frames = []
    for f in range(nf):
        ph = 2 * np.pi * f / nf
        fr = []
        wg = 34.0 * taper * (1 + 0.08 * np.sin(2 * ph))            # glow in the plane: mostly BEHIND the core
        fr.append((band, 0, strip(cent - nrm * (0.3 * wg)[:, None], nrm, wg, 150 * taper ** 0.5, (120, 170, 255))))
        wx = 20.0 * taper * (1 + 0.08 * np.sin(2 * ph))            # glow across the plane
        fr.append((band, 0, strip(cent, up, wx, 120 * taper ** 0.5, (120, 170, 255))))
        wc = 12.0 * taper
        ac = 255 * (0.9 + 0.1 * np.sin(3 * ph)) * taper ** 0.3
        fr.append((band, 0, strip(cent, nrm, wc, ac, (230, 246, 255))))       # the sharp core
        wcx = 7.0 * taper
        fr.append((band, 0, strip(cent, up, wcx, ac, (230, 246, 255))))       # its cross
        for ag, k in ((2, 0.55), (4, 0.3), (6, 0.15)):  # ghosts: drawn at the owner's pose `ag` frames ago = along its path
            fr.append((band, 0, strip(cent, nrm, wc * (1 + 0.15 * ag / 2), ac * k, (150, 190, 255), age=ag)))
            fr.append((band, 0, strip(cent, up, wcx * (1 + 0.15 * ag / 2), ac * k, (150, 190, 255), age=ag)))
        th_k = sp_t * th[-1]
        idx = np.interp(th_k, th, np.arange(len(th)))
        pc = np.stack([np.interp(idx, np.arange(len(th)), cent[:, c]) for c in range(3)], 1)
        nk = np.stack([np.interp(idx, np.arange(len(th)), nrm[:, c]) for c in range(3)], 1)
        wk = np.interp(idx, np.arange(len(th)), wg)
        wxk = np.interp(idx, np.arange(len(th)), wx)
        pts = pc + nk * (sp_r * wk)[:, None] + np.array([0.0, 1.0, 0.0]) * (sp_y * wxk)[:, None]
        tw = 0.5 + 0.5 * np.sin(2 * np.pi * (sp_n * f / nf + sp_ph))
        cols = np.array([(255, 212, 110, int(qz(255 * x, Q_VAL))) for x in tw])
        fr.append((spark, 1, quads_camera(pts, sp_sz * (0.7 + 0.3 * tw), cols, sp_age)))
        frames.append([(t, fl, qverts(roll_verts(v, roll))) for t, fl, v in fr])   # (review F2: after the roll, whose
        # near-cancellations x cos r - y sin r become exact 0)
    return frames


def ring_batch(f, n, band, r0, r1, w0, col, a0, delay=0):
    """a flat expanding ring on the ground plane (x-z) at frame f of n"""
    k = (f - delay) / max(1, n - 1 - delay)
    if k < 0 or k > 1:
        return None
    r = r0 + (r1 - r0) * k ** 0.6
    seg = 40
    phi = np.linspace(0, 2 * np.pi, seg + 1)
    cent = np.stack([r * np.cos(phi), np.zeros_like(phi), r * np.sin(phi)], 1)
    nrm = np.stack([np.cos(phi), np.zeros_like(phi), np.sin(phi)], 1)
    w = np.full(seg + 1, w0 * (1 - 0.5 * k))
    a = np.full(seg + 1, a0 * (1 - k) ** 1.3)
    return (band, 0, qverts(strip(cent, nrm, w, a, col)))


# ================================================================================================================ fx: writing
def batch_rec(tex, flags, blend, clamp, first, n, upright=(0.0,) * 9):
    return [tex, flags, blend, clamp, first, n, upright]


# ================================================================================================================ s247: p1 + c1
# Everything below is PROCEDURAL (ours). Space = KH effect space, cm: x right, y DOWN, z forward (Sora's facing; sora_fx.c turns it
# by the spawn yaw only). Hit sparks / shed bits are camera-facing: batch flags bit 0 = the vertex offsets are CAMERA space
# (x right, y down, z away from the camera), the way KH2's own sparks are drawn, so a flower / the eye always faces the camera.
# blend 0 = additive (glow), 1 = alpha (src alpha / 1 - src alpha: can draw dark: the Malice). Colours GUESS (from the concept art).
P1 = {"white": (238, 246, 255), "cyan": (165, 235, 245), "blue": (120, 175, 255), "lilac": (226, 200, 245), "pink": (240, 205, 240)}
C1 = {"black": (26, 7, 34), "smoke": (52, 16, 70), "purple": (96, 30, 128), "magenta": (240, 45, 185), "hot": (255, 110, 215),
      "eye": (250, 90, 125)}
EYE_OPEN = (0.12, 0.3, 0.5, 0.7, 0.88, 1.0)  # the eye atlas's 6 cells (lid openness)


def _grid(w, h):
    """pixel centres -> x, y in -1..1 (x across w, y across h)"""
    y, x = np.mgrid[0:h, 0:w]
    return (x + 0.5) / w * 2 - 1, (y + 0.5) / h * 2 - 1


def _smooth(e0, e1, x):
    k = np.clip((x - e0) / (e1 - e0), 0, 1)
    return k * k * (3 - 2 * k)


def _noise(w, h, cells, seed, octaves=3):
    """deterministic value noise 0..1 (h, w): bilinear-upsampled random grids, fbm"""
    rng = np.random.default_rng(seed)
    out, amp, tot = np.zeros((h, w)), 1.0, 0.0
    for o in range(octaves):
        c = cells * 2 ** o
        g = rng.random((c + 1, c + 1))
        gy, gx = (np.arange(h) + 0.5) / h * c, (np.arange(w) + 0.5) / w * c
        iy, ix = np.minimum(gy.astype(int), c - 1), np.minimum(gx.astype(int), c - 1)
        fy, fx = (gy - iy)[:, None], (gx - ix)[None, :]
        a, b = g[iy][:, ix], g[iy][:, ix + 1]
        cc, d = g[iy + 1][:, ix], g[iy + 1][:, ix + 1]
        out += amp * ((a * (1 - fx) + b * fx) * (1 - fy) + (cc * (1 - fx) + d * fx) * fy)
        tot += amp
        amp *= 0.5
    return out / tot


def _bgra(rgb, a):
    """rgb (h, w, 3) 0..255 floats, a (h, w) 0..255 -> BGRA uint8 (every value through rnd: review F2)"""
    h, w = a.shape
    out = np.zeros((h, w, 4), np.uint8)
    rgb = np.broadcast_to(np.asarray(rgb, np.float64), (h, w, 3))
    for c in range(3):
        out[:, :, 2 - c] = np.clip(rnd(rgb[..., c]), 0, 255)
    out[:, :, 3] = np.clip(rnd(a), 0, 255)
    return out


def petal_tex(w=32, h=64):
    """a pointed lotus petal along v (row 0 = its base, the last row = its tip), white with a brighter middle vein"""
    x, y = _grid(w, h)
    v = (y + 1) / 2
    hw = np.sin(np.pi * np.clip(v, 0, 1) ** 0.72) ** 0.85 * 0.96 + 1e-3
    e = 1 - np.abs(x) / hw
    a = _smooth(0.0, 0.3, e) * _smooth(0.0, 0.07, v) * 255
    g = 255 * np.clip(0.8 + 0.2 * e + 0.12 * np.exp(-(x / 0.07) ** 2) * (1 - v), 0, 1)
    return _bgra(np.stack([g, g, g], -1), a)


def glow_tex(n=64):
    x, y = _grid(n, n)
    r = np.sqrt(x * x + y * y)
    a = np.clip(1 - r, 0, 1) ** 1.6 * (0.35 + 0.65 * np.exp(-r * r / 0.05)) * 255
    return _bgra((255, 255, 255), a)


def ring_tex(n=64, r0=0.72, wr=0.13):
    """a soft annulus (rims, the light ring)"""
    x, y = _grid(n, n)
    r = np.sqrt(x * x + y * y)
    a = np.exp(-((r - r0) / wr) ** 2) * np.clip((1 - r) / 0.12, 0, 1) * 255
    return _bgra((255, 255, 255), a)


def blob_tex(cell=64, seed=2471):
    """2 x 2 atlas of Malice goo blobs: a wobbly round edge + a few droplets, a little darker grain inside (white: the vertex
    colour makes it black-purple)"""
    rng = np.random.default_rng(seed)
    out = np.zeros((2 * cell, 2 * cell, 4), np.uint8)
    for k in range(4):
        x, y = _grid(cell, cell)
        r, th = np.sqrt(x * x + y * y), np.arctan2(y, x)
        R = np.full_like(r, 0.6)
        for m in range(2, 7):
            R += rng.uniform(0.02, 0.09) / m ** 0.5 * np.cos(m * th + rng.uniform(0, 2 * np.pi))
        a = _smooth(0.0, 0.07, R - r)
        for _ in range(3):  # droplets near the edge
            ang, d, s = rng.uniform(0, 2 * np.pi), rng.uniform(0.68, 0.84), rng.uniform(0.05, 0.1)
            a = np.maximum(a, _smooth(0.0, 0.05, s - np.sqrt((x - d * np.cos(ang)) ** 2 + (y - d * np.sin(ang)) ** 2)))
        a *= np.clip((1 - r) / 0.08, 0, 1)
        g = 255 * (0.7 + 0.3 * _noise(cell, cell, 4, seed + 10 + k))
        out[(k // 2) * cell:(k // 2 + 1) * cell, (k % 2) * cell:(k % 2 + 1) * cell] = _bgra(np.stack([g, g, g], -1), a * 255)
    return out


def smoke_tex(cell=64, seed=2472):
    """2 x 2 atlas of soft noisy puffs (white)"""
    out = np.zeros((2 * cell, 2 * cell, 4), np.uint8)
    for k in range(4):
        x, y = _grid(cell, cell)
        r = np.sqrt(x * x + y * y)
        n = _noise(cell, cell, 3, seed + k, 4)
        a = np.clip(1 - r * (0.85 + 0.4 * n), 0, 1) ** 1.3 * (0.45 + 0.55 * n) * np.clip((1 - r) / 0.1, 0, 1) * 255
        out[(k // 2) * cell:(k // 2 + 1) * cell, (k % 2) * cell:(k % 2 + 1) * cell] = _bgra((255, 255, 255), a)
    return out


def eye_tex(cw=128, ch=64):
    """the Calamity eye, 2 x 3 atlas: cell k = the lids open EYE_OPEN[k]. Coloured (vertex colour white, alpha blend): a pink-red
    glowing eye (bright middle, darker red toward the lids), a darker iris ring, a vertical purple slit pupil with a lilac rim, a
    small highlight, a black lid outline. The lids cut the iris / pupil (they do not squash it)."""
    out = np.zeros((3 * ch, 2 * cw, 4), np.uint8)
    for k, o in enumerate(EYE_OPEN):
        x, y = _grid(cw, ch)
        X, Y = x, y * ch / cw  # the same scale on both axes (1 unit = cw / 2 px)
        s = np.clip(1 - (X / 0.94) ** 2, 0, 1)
        up, lo = -o * 0.44 * s ** 0.8, o * 0.36 * s ** 0.8
        din = np.minimum(Y - up, lo - Y)  # > 0 inside the lids
        px = 2.0 / cw  # one pixel in units
        inside = _smooth(-px, px, din) * (np.abs(X) < 0.94)
        line = np.exp(-((din - 0.0) / (2.2 * px)) ** 2) * (s > 0.002)
        rr = np.sqrt(X * X + Y * Y)
        base = np.array(C1["eye"], np.float64)
        rgb = (base[None, None] * (0.55 + 0.45 * np.exp(-(rr / 0.55) ** 2))[..., None]
               + np.array((255, 160, 180.0))[None, None] * (0.6 * np.exp(-(rr / 0.2) ** 2))[..., None])
        ir = np.exp(-((rr - 0.32) / 0.035) ** 2)[..., None]  # the iris ring
        rgb = rgb * (1 - 0.6 * ir) + np.array((150, 15, 60.0)) * 0.6 * ir
        pe = (X / 0.07) ** 2 + (Y / 0.3) ** 2  # the slit
        rim = _smooth(1.9, 1.3, pe)[..., None]
        pup = _smooth(1.15, 0.85, pe)[..., None]
        rgb = rgb * (1 - rim) + np.array((205, 125, 235.0)) * rim
        rgb = rgb * (1 - pup) + np.array((72, 16, 104.0)) * pup
        hl = np.exp(-(((X + 0.14) ** 2 + (Y + 0.1) ** 2) / 0.0012))[..., None]
        rgb = rgb * (1 - 0.8 * hl) + np.array((255, 235, 245.0)) * 0.8 * hl
        ln = np.clip(line, 0, 1)[..., None]
        rgb = rgb * (1 - ln) + np.array((20, 4, 22.0)) * ln
        a = np.maximum(inside, np.clip(line, 0, 1)) * 255
        out[(k // 2) * ch:(k // 2 + 1) * ch, (k % 2) * cw:(k % 2 + 1) * cw] = _bgra(np.clip(rgb, 0, 255), a)
    return out


def eye_uv(k, cw=128, ch=64):
    """uv rect (u0, v0, u1, v1) of eye cell k (half a texel in)"""
    W, H = 2 * cw, 3 * ch
    x0, y0 = (k % 2) * cw, (k // 2) * ch
    return ((x0 + 0.5) / W, (y0 + 0.5) / H, (x0 + cw - 0.5) / W, (y0 + ch - 0.5) / H)


def cell_uv(k, n=2):
    """uv rect of cell k of an n x n atlas (half a texel in for 64 px cells)"""
    e = 0.5 / (64 * n)
    return ((k % n) / n + e, (k // n) / n + e, (k % n + 1) / n - e, (k // n + 1) / n - e)


def ribbon_tex_s247(which, w=128, h=64):
    """our ribbons: u (x) = the sample's age (0 = the blade now .. 1 = the old end), v (y) = across the blade, row 0 = the TIP
    (sora_fx.c's GUESS default; "set kbfxflip 1" puts it at the grip). p1 is drawn ADDITIVE (moonlight glow), c1 ALPHA (smoke)."""
    x, y = _grid(w, h)
    u, v = (x + 1) / 2, (y + 1) / 2
    if which == "p1":
        b = 0.22 + 0.78 * np.exp(-(v / 0.38) ** 1.6)
        core = np.exp(-((v - 0.05) / 0.05) ** 2) * (1 - u) ** 0.6
        stops = [(0.0, P1["blue"]), (0.45, P1["cyan"]), (0.8, P1["white"]), (1.0, (255, 255, 255))]
        rgb = ramp(np.clip(b + 0.4 * core, 0, 1), stops)
        k = (np.clip(u, 0, 1) ** 1.2 * 0.85)[..., None]
        rgb = rgb * (1 - k) + np.array(P1["lilac"], np.float64) * k
        n = _noise(w, h, 6, 2473)
        a = np.clip(b * (0.85 + 0.15 * n) + 0.5 * core, 0, 1) * (1 - u) ** 0.35 * _smooth(0.0, 0.04, v) * 255
        rng = np.random.default_rng(2474)  # a few soft white-lilac motes (petal glints), fewer toward the old end
        for _ in range(26):
            mu, mv = float(qz(rng.random() ** 1.5 * 0.85, Q_VAL)), float(qz(0.08 + 0.7 * rng.random(), Q_VAL))
            m = np.exp(-(((u - mu) * w / 2.2) ** 2 + ((v - mv) * h / 1.6) ** 2)) * (1 - mu / 0.85)
            rgb = rgb * (1 - m[..., None]) + np.array((252, 240, 255.0)) * m[..., None]
            a = np.maximum(a, 230 * m)
        return _bgra(np.clip(rgb, 0, 255), a)
    n = _noise(w, h, 5, 2475, 4)
    n2 = _noise(w, h, 9, 2476, 3)
    body = np.array(C1["black"], np.float64) * (1 - n[..., None]) + np.array(C1["purple"], np.float64) * (0.55 * n[..., None])
    ab = (0.62 + 0.3 * n) * _smooth(1.02, 0.6, v + 0.25 * u)
    e1 = np.exp(-((v - 0.07) / 0.06) ** 2)  # the tip edge: magenta, a brighter line in it
    e2 = np.exp(-((v - 0.03) / 0.018) ** 2)
    e3 = 0.6 * np.exp(-((v - 0.93) / 0.05) ** 2)  # a weaker one at the grip
    rgb = body
    for e, col in ((e3, C1["magenta"]), (e1, C1["magenta"]), (e2, C1["hot"])):
        rgb = rgb * (1 - e[..., None]) + np.array(col, np.float64) * e[..., None]
    a = np.clip(np.maximum(ab, 0.95 * np.maximum(e1, e3)), 0, 1)
    a *= _smooth(0.0, 0.03, v)
    a *= np.clip(1.25 - u * (0.5 + 0.9 * n2), 0, 1)  # older smoke breaks up into wisps
    return _bgra(np.clip(rgb, 0, 255), a * 255)


class Layer:
    """one batch of a frame: camera-facing quads (flags 1) on one texture + blend"""

    def __init__(self, tex, blend):
        self.tex, self.blend, self.v = tex, blend, []

    def quad(self, p, c, au, av, rgba, uv=(0.0, 0.0, 1.0, 1.0), oz=0.0):
        """corners (camera space, KH cm) c - au - av (uv u0 v0), c + au - av (u1 v0), c - au + av (u0 v1), c + au + av (u1 v1);
        rgba (4,) or per corner (4, 4); oz < 0 = nearer the camera (so a spark at a body's skin is not hidden in it)"""
        rgba = np.broadcast_to(np.asarray(rgba, np.float64), (4, 4))
        c, au, av = np.asarray(c, np.float64), np.asarray(au, np.float64), np.asarray(av, np.float64)
        cs = [c - au - av, c + au - av, c - au + av, c + au + av]
        u0, v0, u1, v1 = uv
        uvs = [(u0, v0), (u1, v0), (u0, v1), (u1, v1)]
        for q in (0, 1, 2, 1, 3, 2):
            self.v.append((tuple(p), (cs[q][0], cs[q][1], oz), uvs[q], tuple(rgba[q])))

    def sprite(self, p, size, rgba, rot=0.0, uv=(0.0, 0.0, 1.0, 1.0), oz=0.0, sx=1.0):
        """a square billboard of side `size` turned by rot (rad, in the camera plane); sx squashes its width"""
        cr, sr = np.cos(rot), np.sin(rot)
        self.quad(p, (0.0, 0.0), (cr * size / 2 * sx, sr * size / 2 * sx), (-sr * size / 2, cr * size / 2), rgba, uv, oz)

    def petal(self, p, base, ang, length, width, rgba_base, rgba_tip, oz=0.0):
        """a petal quad in the camera plane from `base` (camera offset) pointing at angle ang (0 = right, pi / 2 = DOWN)"""
        d = np.array((np.cos(ang), np.sin(ang)))
        n = np.array((-d[1], d[0]))
        c = np.asarray(base, np.float64) + d * length / 2
        self.quad(p, c, n * width / 2, d * length / 2, [rgba_base, rgba_base, rgba_tip, rgba_tip], oz=oz)

    def verts(self):
        if not self.v:
            return None
        P = np.array([q[0] for q in self.v])
        O = np.array([q[1] for q in self.v])
        U = np.array([q[2] for q in self.v])
        C = np.array([q[3] for q in self.v])
        C = np.clip(rnd(C), 0, 255).astype(np.uint8)
        return qverts(mkverts(P, U, C, off=O))


def frame_rows(layers):
    """[Layer] -> the frame's batch rows (texref, flags 1, blend, clamp 1, upright 0, verts); empty layers dropped"""
    out = []
    for L in layers:
        v = L.verts()
        if v is not None:
            out.append((L.tex, 1, L.blend, 1, (0.0,) * 9, v))
    if not out:  # (a frame must not be empty: one invisible quad)
        L = Layer(layers[0].tex, 0)
        L.sprite((0, 0, 0), 0.01, (0, 0, 0, 0))
        out.append((L.tex, 1, 0, 1, (0.0,) * 9, L.verts()))
    return out


def _ease_out(k):
    k = min(max(k, 0.0), 1.0)
    return 1 - (1 - k) ** 3


def _fade(f, f0, f1):
    """1 before f0, 0 after f1, smooth between"""
    if f <= f0:
        return 1.0
    if f >= f1:
        return 0.0
    k = (f - f0) / (f1 - f0)
    return 1 - k * k * (3 - 2 * k)


def _rgba(col, a):
    return (col[0], col[1], col[2], a)


def _mix(c0, c1, k):
    return tuple(c0[i] * (1 - k) + c1[i] * k for i in range(3))


def dust_init(rng, n, speed, spread_up=0.3):
    """n sparkle / droplet starts: a random 3D direction (a little upward bias), a speed in [speed / 2, speed]"""
    d = rng.normal(size=(n, 3))
    d[:, 1] -= spread_up  # (y down: up = negative)
    d /= np.linalg.norm(d, axis=1)[:, None]
    return d * rng.uniform(speed / 2, speed, n)[:, None], rng.random(n), rng.uniform(0.7, 1.0, n)


def dust_pos(vel, f, damp=0.9, grav=0.03):
    """positions at frame f of particles leaving 0 with vel, damped per frame, falling (y down) by grav per frame^2"""
    k = (1 - damp ** f) / (1 - damp)
    p = vel * k
    p[:, 1] += 0.5 * grav * f * f
    return p


def bloom_layers(T, f, n_frames, scale, petals, open_frames, oz, rng_seed, inner=0, light=False):
    """Silent Princess's flower opening at the hit: `petals` petals fan out from a bud pointing up to a full ring in `open_frames`
    frames (a soft additive glow under each, an alpha-blended white-blue petal over it with a lilac-pink tip), a bright centre,
    sparkle dust flying out. inner = a second, smaller ring of petals between them (finisher); light = the soft blue light burst."""
    rng = np.random.default_rng(rng_seed)
    nd = {1: 10, 0: 5}[int(scale >= 0.9)] + (10 if light else 0)
    vel, ph, br = dust_init(rng, nd, 2.6 * scale)
    sz = rng.uniform(4.0, 7.0, nd) * scale
    lil = rng.random(nd) < 0.35
    glowL, petG, petL, cenL, dustL, burstL = (Layer(T["glow"], 0), Layer(T["petal"], 0), Layer(T["petal"], 1), Layer(T["glow"], 0),
                                              Layer(T["spark"], 0), Layer(T["ring"], 0))
    o = _ease_out(f / open_frames)
    fade = _fade(f, n_frames * 0.5, n_frames - 1)
    if f < 9:  # the hit flash
        k = 1 - f / 9
        glowL.sprite((0, 0, 0), (26 + 26 * (1 - k)) * scale, _rgba(P1["white"], 240 * k), oz=oz)
    if light:  # the finisher's soft blue light burst + a thin light ring
        kb = _ease_out(f / 14)
        ab = 95 * _fade(f, 3, 22)
        if ab > 0:
            glowL.sprite((0, 0, 0), (60 + 140 * kb) * scale, _rgba(P1["blue"], ab), oz=oz)
            burstL.sprite((0, 0, 0), (40 + 190 * kb) * scale, _rgba(P1["cyan"], 125 * _fade(f, 5, 20)), oz=oz)
    rings = [(petals, 0.0, 1.0, 0)] + ([(inner, np.pi / inner, 0.62, 2)] if inner else [])
    for npet, a0, sc, delay in rings:
        ok = _ease_out((f - delay) / open_frames) if f >= delay else 0.0
        if ok <= 0:
            continue
        grow = 1 + 0.12 * max(0.0, f - open_frames) / n_frames  # it keeps opening a little while it fades
        L = 30 * scale * sc * (0.3 + 0.7 * ok) * grow
        W = L * (0.34 + 0.16 * ok)
        b = (2 + 4 * ok) * scale * sc
        spin = 0.15 * ok + 0.004 * f
        for i in range(npet):
            fin = -np.pi / 2 + a0 + 2 * np.pi * i / npet + spin  # final angle (the first petal up: y down -> -pi / 2)
            ang = -np.pi / 2 + (fin + np.pi / 2) * ok   # a bud pointing up fans out
            base = (b * np.cos(ang), b * np.sin(ang))
            a = 235 * fade
            petG.petal((0, 0, 0), base, ang, L * 1.22, W * 1.45, _rgba(P1["blue"], 0.42 * a), _rgba(P1["lilac"], 0.3 * a), oz=oz)
            petL.petal((0, 0, 0), base, ang, L, W, _rgba(_mix(P1["white"], P1["cyan"], 0.5), a), _rgba(P1["lilac"], 0.9 * a), oz=oz)
    ac = 255 * _fade(f, n_frames * 0.35, n_frames * 0.8) * min(1.0, f / 2 + 0.5)
    cenL.sprite((0, 0, 0), 14 * scale, _rgba((255, 250, 235), ac), oz=oz - 1)
    for j in range(nd):
        p = dust_pos(vel[j:j + 1], f, 0.9, 0.02)[0]
        tw = 0.6 + 0.4 * np.sin(2 * np.pi * (ph[j] + f / 9.0))
        a = 255 * br[j] * tw * _fade(f, n_frames * 0.45, n_frames - 1)
        dustL.sprite(p, sz[j] * (0.75 + 0.25 * tw), _rgba(P1["lilac"] if lil[j] else P1["white"], a), rot=0.3 * f / 10, oz=oz * 0.6)
    return [burstL, glowL, petG, petL, cenL, dustL]


def p1_hit(T, kind):
    n, scale, petals, op, inner, light = {"hit": (30, 1.0, 6, 7, 0, False), "hit_finish": (46, 1.7, 6, 9, 5, True),
                                          "hit_guard": (18, 0.55, 5, 4, 0, False)}[kind]
    seed = {"hit": 2481, "hit_finish": 2482, "hit_guard": 2483}[kind]
    return [frame_rows(bloom_layers(T, f, n, scale, petals, op, -25.0 * scale, seed, inner, light)) for f in range(n)]


def p1_shed(T, k):
    """petals shed off the blade: 2 (variant 2: 3 smaller) glowing petals drifting down, swaying, turning, then fading; 48 frames.
    The element is spawned with a random yaw, so its drift goes a different way each time."""
    rng = np.random.default_rng(2490 + k)
    npet = 3 if k == 2 else 2
    n = 48
    p0 = rng.uniform(-4, 4, (npet, 3))
    v0 = rng.normal(size=(npet, 3)) * 0.35
    v0[:, 1] = rng.uniform(-0.5, -0.1, npet)  # a small lift first (y down)
    sway = rng.uniform(3, 6, npet)
    sw_f = rng.uniform(0.07, 0.12, npet)
    sw_ph = rng.uniform(0, 2 * np.pi, npet)
    rot0 = rng.uniform(0, 2 * np.pi, npet)
    spin = rng.uniform(-0.09, 0.09, npet)
    flip = rng.uniform(0.08, 0.16, npet)
    ln = rng.uniform(13, 16, npet) * (0.8 if k == 2 else 1.0)
    frames = []
    for f in range(n):
        g, p_, = Layer(T["glow"], 0), Layer(T["petal"], 1)
        fa = min(1.0, (f + 1) / 3) * _fade(f, n - 20, n - 1)
        for i in range(npet):
            k_ = (1 - 0.93 ** f) / 0.07
            pos = p0[i] + v0[i] * k_
            pos[1] += 0.5 * 0.012 * f * f if f < 30 else 0.5 * 0.012 * 900 + 0.36 * (f - 30)  # settles to a slow fall
            d = np.array((np.cos(sw_ph[i]), 0.0, np.sin(sw_ph[i])))
            pos = pos + d * sway[i] * np.sin(sw_f[i] * f * 2 * np.pi / 2 + sw_ph[i])
            rot = rot0[i] + spin[i] * f
            sx = 0.35 + 0.65 * abs(np.cos(flip[i] * f + sw_ph[i]))  # it flutters (turns edge-on now and then)
            g.sprite(pos, ln[i] * 1.7, _rgba(P1["blue"], 105 * fa), oz=-3)
            cr, sr = np.cos(rot), np.sin(rot)
            p_.quad(pos, (0.0, 0.0), (cr * ln[i] * 0.3 * sx, sr * ln[i] * 0.3 * sx), (-sr * ln[i] / 2, cr * ln[i] / 2),
                    [_rgba(_mix(P1["white"], P1["cyan"], 0.4), 235 * fa)] * 2 + [_rgba(P1["lilac"], 215 * fa)] * 2, oz=-3)
        frames.append(frame_rows([g, p_]))
    return frames


def splash_layers(T, f, n, scale, nblob, npuff, seed, oz):
    """Malice splash: black-purple goo blobs flung out and falling (alpha), each with a magenta rim light (additive), dark purple
    smoke puffs swelling up (alpha), a magenta flash at the start (additive)"""
    rng = np.random.default_rng(seed)
    vel, ph, br = dust_init(rng, nblob, 5.0 * scale, 0.6)
    vel[:, 2] *= 0.35  # (mostly across the view: a splash toward / away from the camera would not spread on screen)
    bs = rng.uniform(12, 22, nblob) * scale
    bc = rng.integers(0, 4, nblob)
    brot = rng.uniform(0, 2 * np.pi, nblob)
    pv = rng.normal(size=(npuff, 3)) * 0.5 * scale
    pv[:, 1] = -rng.uniform(0.3, 0.7, npuff) * scale
    pc = rng.integers(0, 4, npuff)
    smokeL, blobL, rimL, flashL = Layer(T["smoke"], 1), Layer(T["blob"], 1), Layer(T["ring"], 1), Layer(T["glow"], 0)
    if f < 7:
        k = 1 - f / 7
        flashL.sprite((0, 0, 0), (16 + 22 * (1 - k)) * scale, _rgba(C1["magenta"], 230 * k), oz=oz)
    for j in range(npuff):
        kk = f / n
        p = pv[j] * f * 0.9 ** (f / 6)
        a = 165 * _ease_out(f / 4) * _fade(f, n * 0.3, n - 1)
        smokeL.sprite(p, (22 + 26 * _ease_out(kk * 1.4)) * scale, _rgba(C1["smoke"], a), rot=0.4 * j + 0.01 * f, uv=cell_uv(int(pc[j])),
                      oz=oz * 0.5)
    pos = dust_pos(vel, f, 0.86, 0.16 * scale)
    for j in range(nblob):
        grow = 0.6 + 0.4 * _ease_out(f / 4)
        shrink = _fade(f, n * 0.45, n - 1)
        s = bs[j] * grow * (0.4 + 0.6 * shrink)
        a = 240 * min(1.0, shrink * 1.6)
        blobL.sprite(pos[j], s, _rgba(C1["black"], a), rot=brot[j], uv=cell_uv(int(bc[j])), oz=oz * 0.6)
        rimL.sprite(pos[j], s * 1.2, _rgba(C1["magenta"], 175 * _fade(f, n * 0.25, n * 0.8)), oz=oz * 0.6 + 0.5)
    return [smokeL, flashL, rimL, blobL], []  # (all behind the eye: the goo bursts out around it; nothing in front)


def eye_layers(T, f, scale, t_open, t_hold, t_close, oz):
    """the slit eye at the hit point: lids open over t_open frames (atlas cells 0..5), stay, close over t_close, gone; a soft
    pink-red glow behind it (additive) so it also reads on a dark night. Camera-facing, the pupil stays upright (camera y)."""
    cl, gl, ey = Layer(T["smoke"], 1), Layer(T["glow"], 0), Layer(T["eye"], 1)
    t1, t2 = t_open + t_hold, t_open + t_hold + t_close
    ac = 215 * _ease_out((f + 1) / 3) * _fade(f, t1, t2 + 6)
    if ac > 0:  # the eye sits in a dark Malice cloud (reads on a bright sky; the eye glows out of it at night)
        cl.sprite((0, 0, 0), 62 * scale, _rgba(C1["black"], ac), uv=cell_uv(1), oz=oz + 2, sx=1.25)
    if f >= t2:
        return [cl, gl, ey]
    if f < t_open:
        o = (f + 1) / t_open
    elif f < t1:
        o = 1.0
    else:
        o = 1 - (f - t1 + 1) / (t_close + 1)
    k = int(np.clip(round(o * (len(EYE_OPEN) - 1)), 0, len(EYE_OPEN) - 1))
    pop = 1 + 0.12 * _fade(f, t_open, t_open + 4) * (f >= t_open - 1)
    w, h = 40 * scale * pop, 20 * scale * pop
    gl.quad((0, 0, 0), (0, 0), (w * 0.85, 0), (0, h * 0.95), _rgba(C1["eye"], 170 * EYE_OPEN[k]), oz=oz + 1)
    ey.quad((0, 0, 0), (0, 0), (w / 2, 0), (0, h / 2), (255, 255, 255, 255), uv=eye_uv(k), oz=oz)
    return [cl, gl, ey]


def shock_layers(T, f, scale, oz, t_end=20):
    """the finisher's dark shockwave: a camera-facing ring swelling out, dark (alpha) with a thin magenta edge inside (additive)"""
    dk, mg = Layer(T["ringd"], 1), Layer(T["ringt"], 1)
    if f >= t_end:
        return [dk, mg]
    k = _ease_out(f / t_end)
    s = (30 + 230 * k) * scale
    dk.sprite((0, 0, 0), s, _rgba(C1["black"], 225 * _fade(f, t_end * 0.35, t_end)), oz=oz)
    mg.sprite((0, 0, 0), s * 0.9, _rgba(C1["magenta"], 200 * _fade(f, t_end * 0.25, t_end * 0.9)), oz=oz - 0.5)
    return [dk, mg]


def c1_hit(T, kind):
    n, scale, nblob, npuff, eye = {"hit": (32, 1.0, 8, 3, (4, 9, 5)), "hit_finish": (48, 1.6, 12, 4, (6, 16, 6)),
                                   "hit_guard": (18, 0.6, 4, 1, None)}[kind]
    seed = {"hit": 2484, "hit_finish": 2485, "hit_guard": 2486}[kind]
    out = []
    for f in range(n):
        ls = []
        if kind == "hit_finish":
            ls += shock_layers(T, f, scale, -20.0)
        back, front = splash_layers(T, f, n, scale, nblob, npuff, seed, -22.0 * scale)
        ls += back
        if eye:
            ls += eye_layers(T, f, scale, eye[0], eye[1], eye[2], -38.0 * scale)
        ls += front
        out.append(frame_rows(ls))
    return out


def c1_shed(T, k):
    """Malice embers off the blade: 3 (variant 2: 2 + a smoke wisp) magenta embers rising a little, flickering, dying; 36 frames.
    An ember = a magenta core (alpha: still reads on a bright sky) + a hot glow (additive) + a faint dark wisp (alpha)."""
    rng = np.random.default_rng(2495 + k)
    ne = 2 if k == 2 else 3
    n = 36
    p0 = rng.uniform(-4, 4, (ne, 3))
    v0 = rng.normal(size=(ne, 3)) * 0.25
    v0[:, 1] = -rng.uniform(0.45, 0.8, ne)  # rising (y down)
    life = rng.integers(22, 35, ne)
    fl = rng.uniform(0, 2 * np.pi, (ne, 3))
    sz = rng.uniform(5.0, 7.0, ne)
    frames = []
    for f in range(n):
        wisp, core, hot = Layer(T["smoke"], 1), Layer(T["glow"], 1), Layer(T["glow"], 0)
        for i in range(ne):
            if f >= life[i]:
                continue
            kk = (1 - 0.92 ** f) / 0.08
            pos = p0[i] + v0[i] * kk
            pos[0] += 1.5 * np.sin(0.21 * f + fl[i, 0])
            flick = 0.7 + 0.3 * np.sin(0.9 * f + fl[i, 1]) * np.sin(0.37 * f + fl[i, 2])
            fa = min(1.0, (f + 1) / 2) * _fade(f, life[i] * 0.5, life[i] - 1)
            s = sz[i] * (1 - 0.5 * f / life[i])
            wisp.sprite(pos + np.array((0, 3.0, 0)), (10 + 0.5 * f) * (1.4 if k == 2 else 1.0), _rgba(C1["smoke"], 95 * fa),
                        uv=cell_uv(i % 4), rot=0.1 * f, oz=-2)
            core.sprite(pos, s * 1.3, _rgba(C1["magenta"], 255 * fa * flick), oz=-3)
            hot.sprite(pos, s * 2.8, _rgba(C1["hot"], 130 * fa * flick), oz=-3.2)
        frames.append(frame_rows([wisp, core, hot]))
    return frames


def trail_elem(tref, blend):
    """a ribbon style element: the runtime reads only its first batch's texture + blend and first vertex's colour (white)"""
    L = Layer(tref, blend)
    L.sprite((0, 0, 0), 0.01, (255, 255, 255, 0))
    return [[(tref, 1, blend, 1, (0.0,) * 9, L.verts())]]


def s247_textures(tx):
    return {"petal": tx.add(petal_tex()), "glow": tx.add(glow_tex()), "ring": tx.add(ring_tex()), "ringd": tx.add(ring_tex(128, 0.76, 0.12)),
            "ringt": tx.add(ring_tex(128, 0.82, 0.035)),
            "spark": tx.add(sparkle_tex()), "blob": tx.add(blob_tex()), "smoke": tx.add(smoke_tex()), "eye": tx.add(eye_tex()),
            "rib_p1": tx.add(ribbon_tex_s247("p1")), "rib_c1": tx.add(ribbon_tex_s247("c1"))}


def elements_s247(tx, log):
    """(name, frames [(texref, flags, blend, clamp, upright, verts)], loops, pax, bone, fade) for every NEW_FX_S247 element, in order"""
    T = s247_textures(tx)
    out = []
    for k in KINDS:
        out.append(("kbp1_" + k, p1_hit(T, k), 0, 0, 0, 0))
    out.append(("kbp1_trail", trail_elem(T["rib_p1"], 0), 0, 0, 0, 0))
    for k in range(SHED_N):
        out.append(("kbp1_shed_%d" % k, p1_shed(T, k), 0, 0, 0, 0))
    for k in KINDS:
        out.append(("kbc1_" + k, c1_hit(T, k), 0, 0, 0, 0))
    out.append(("kbc1_trail", trail_elem(T["rib_c1"], 1), 0, 0, 0, 0))
    for k in range(SHED_N):
        out.append(("kbc1_shed_%d" % k, c1_shed(T, k), 0, 0, 0, 0))
    assert [e[0] for e in out] == NEW_FX_S247
    log("  s247: Silent Princess p1 + Calamity's Gaze c1, %d procedural elements, %d textures of ours" % (len(out), len(T)))
    return out


def build_fx(src_path, dst, log, part="s216"):
    pk = Pack(src_path)
    for pn, (nfx, _, _) in PARTS.items():  # (s247: each part once, in order: s216's rows must already be there for s247)
        have = [n for n in nfx if n in pk.eff]
        if pn == part and have:
            die("the pack already has %s (run on the pack BEFORE this step)" % have[0])
        if pn == "s247" and part == "s216" and have:
            die("the pack already has the s247 rows (%s): the s216 part must run before them" % have[0])
    if part == "s216":
        need = ["kb%s_%s" % (SRC[w], k) for w in SRC for k in KINDS + ["trail"]]
        miss = [n for n in need if n not in pk.eff]
        if miss:
            die("the pack has no %s (the s89 per-Keyblade sets of fxbake.py): cannot shape our sets" % ", ".join(miss))
    old_last = pk.ntex - 1
    last = pk.tex[old_last]
    if last[2] != max(t[2] for t in pk.tex) or last[2] + last[0] * last[1] * 4 != pk.pixb:
        die("the pack's last texture is not the ribbon gradient at the end of pix")
    bt_tex = np.frombuffer(pk.mm[pk.o_ba:pk.o_ba + 52 * pk.nba], np.uint8).reshape(pk.nba, 52)[:, :2].copy().view("<u2").ravel()
    if (bt_tex == old_last).any():
        die("an old batch uses the last texture (the gradient): it would need renumbering")

    tx = Textures(pk)
    elements = []  # (name, frames [(texref|None, flags, blend, clamp, upright, verts)], loops, pax, bone, fade)
    if part == "s247":
        elements = elements_s247(tx, log)
    else:
        def from_src(nm):
            return [[(r, bt[1], bt[2], bt[3], bt[9:18], v) for bt, r, v in fr] for fr in nm]

        for which in ("s1", "g1"):
            for k in KINDS:
                sname = "kb%s_%s" % (SRC[which], k)
                log("  kb%s_%s <- %s" % (which, k, sname))
                frs = resolve_frames(tx, recolour_element(pk, pk.element(sname), which, log))
                e = pk.eff[sname]
                elements.append(("kb%s_%s" % (which, k), from_src(frs), 0, e[3], e[4], e[5]))
            sname = "kb%s_trail" % SRC[which]
            src = pk.element(sname)
            bt, v = src[0][0]
            if bt[0] == 0xFFFF:
                die("%s has no texture" % sname)
            tref = tx.add(trail_tex(pk.pixels(bt[0]), which))
            frs = []
            for fr in src:
                nf = []
                for b, vv in fr:
                    vv = vv.copy()
                    vv["rgba"][:, :3] = 255
                    nf.append((tref, b[1], b[2], b[3], b[9:18], vv))
                frs.append(nf)
            e = pk.eff[sname]
            elements.append(("kb%s_trail" % which, frs, 0, e[3], e[4], e[5]))
            log("  kb%s_trail <- %s (texture %d -> our ribbon)" % (which, sname, bt[0]))
        band, spark = tx.add(band_tex()), tx.add(sparkle_tex())
        bf = beam_frames(band, spark)
        elements.append(("kbs1_beam", [[(t, fl, 0, 1, (0.0,) * 9, v) for t, fl, v in fr] for fr in bf], 1, 0, 0, 0))
        log("  kbs1_beam: procedural crescent, %d frames (loop)" % len(bf))
        fin = [e for e in elements if e[0] == "kbs1_hit_finish"][0]
        hf = []
        nfin = len(fin[1])
        for f, fr in enumerate(fin[1]):
            nf = []
            for t, fl, bl, cl, up, v in fr:
                v = v.copy()
                v["p"] *= 1.5
                v["off"] *= 1.5
                nf.append((t, fl, bl, cl, up, v))
            for rb in (ring_batch(f, 24, band, 20, 160, 22, (120, 170, 255), 220),
                       ring_batch(f, 30, band, 10, 120, 8, (255, 206, 96), 230, delay=3)):
                if rb:
                    nf.append((rb[0], 0, rb[1], 1, (0.0,) * 9, rb[2]))
            hf.append(nf)
        elements.append(("kbs1_beam_hit", hf, 0, fin[3], fin[4], fin[5]))
        log("  kbs1_beam_hit: kbs1_hit_finish x1.5 + 2 rings, %d frames" % nfin)
        for r, nm in zip(BEAM_ROLLS, BEAM_ROLL_FX):  # (s217: after every s216 row, so those keep their indices and bytes)
            bf = beam_flat_frames(band, spark, r)
            elements.append((nm, [[(t, fl, 0, 1, (0.0,) * 9, v) for t, fl, v in fr] for fr in bf], 1, 0, 0, 0))
        log("  kbs1_beam_r000..r%03d: the swing-plane crescent, %d rolls x %d frames (loop)" % (BEAM_ROLLS[-1], len(BEAM_ROLLS), len(bf)))

    # ---- the new rows
    k_new = len(tx.new)
    newb, newv, newf, newe = bytearray(), bytearray(), bytearray(), bytearray()
    nv_tot = 0
    for name, frs, loops, pax, bone, fade in elements:
        first_frame = pk.nfr + len(newf) // 8
        maxv = 0
        for fr in frs:
            first_batch = pk.nba + len(newb) // 52
            fv = 0
            for tref, fl, bl, cl, up, v in fr:
                t = 0xFFFF if tref is None else (tref[1] if tref[0] == "old" else old_last + tref[1])
                first_vert = pk.nve + nv_tot
                newb += OUT_BATCH.pack(t, fl, bl, cl, 0, 0, 0, first_vert, len(v), *up)
                newv += np.ascontiguousarray(v).tobytes()
                nv_tot += len(v)
                fv += len(v)
            newf += OUT_FRAME.pack(first_batch, len(fr))
            maxv = max(maxv, fv)
        newe += OUT_EFF.pack(name.encode(), first_frame, len(frs), loops, pax, bone, fade, maxv, 0)
        log("  + %-15s %3d frames%s, max %5d verts/frame, pax %#x bone %#x" % (name, len(frs), " (loop)" if loops else "", maxv, pax, bone))
    # ---- write (as chickfx.build_fx: new textures before the gradient)
    mm = pk.mm
    last_off = last[2]
    npix = bytearray()
    new_texrecs = []
    for (w, h, b) in tx.new:
        new_texrecs.append(OUT_TEX.pack(w, h, last_off + len(npix), 0))
        npix += b
    tex_tbl = mm[pk.o_tex:pk.o_tex + 16 * old_last] + b"".join(new_texrecs) + OUT_TEX.pack(last[0], last[1], last_off + len(npix), 0)
    ntex2, pix2 = pk.ntex + k_new, pk.pixb + len(npix)
    nfr2, nba2, nve2, neff2 = pk.nfr + len(newf) // 8, pk.nba + len(newb) // 52, pk.nve + len(newv) // 40, pk.neff + len(newe) // 64
    sizes = [ntex2 * 16, neff2 * 64, nfr2 * 8, nba2 * 52, nve2 * 40, pk.ntr * 12, pix2]
    offs, q = [], OUT_HDR.size
    for s in sizes:
        offs.append(q)
        q += s
        q += -q % 16
    if q > (1 << 32) - 1:
        die("the new pack passes the uint32 cap")
    hdr = OUT_HDR.pack(b"SKFX", 1, ntex2, neff2, nfr2, nba2, nve2, pk.ntr, *offs, pix2)

    def copy_range(fo, a, b):
        while a < b:
            n = min(b - a, 32 << 20)
            fo.write(mm[a:a + n])
            a += n
    tmp = dst + ".tmp"
    with open(tmp, "wb") as fo:
        fo.write(hdr)
        parts = [lambda: fo.write(tex_tbl),
                 lambda: (copy_range(fo, pk.o_eff, pk.o_eff + 64 * pk.neff), fo.write(newe)),
                 lambda: (copy_range(fo, pk.o_fr, pk.o_fr + 8 * pk.nfr), fo.write(newf)),
                 lambda: (copy_range(fo, pk.o_ba, pk.o_ba + 52 * pk.nba), fo.write(newb)),
                 lambda: (copy_range(fo, pk.o_ve, pk.o_ve + 40 * pk.nve), fo.write(newv)),
                 lambda: copy_range(fo, pk.o_tr, pk.o_tr + 12 * pk.ntr),
                 lambda: (copy_range(fo, pk.o_pix, pk.o_pix + last_off), fo.write(npix), copy_range(fo, pk.o_pix + last_off, pk.o_pix + pk.pixb))]
        for off_, fn in zip(offs, parts):
            fo.write(bytes(off_ - fo.tell()))
            fn()
    os.replace(tmp, dst)
    log("%s: %.1f MB (was %.1f MB): +%d elements, +%d frames, +%d batches, +%d verts, textures %d -> %d (%d new, gradient now #%d)" %
        (dst, os.path.getsize(dst) / 1e6, len(mm) / 1e6, len(newe) // 64, len(newf) // 8, len(newb) // 52, len(newv) // 40,
         pk.ntex, ntex2, k_new, ntex2 - 1))
    pk.close()
    verify_fx(src_path, dst, log, PARTS[part][0])


def verify_fx(old, new, log, new_fx=NEW_FX):
    po, pn = Pack(old), Pack(new)
    mo, mn = po.mm, pn.mm
    SZ = (16, 64, 8, 52, 40, 12)
    cnt_o = (po.ntex - 1, po.neff, po.nfr, po.nba, po.nve, po.ntr)
    oo = (po.o_tex, po.o_eff, po.o_fr, po.o_ba, po.o_ve, po.o_tr)
    on = (pn.o_tex, pn.o_eff, pn.o_fr, pn.o_ba, pn.o_ve, pn.o_tr)
    names = ("tex", "eff", "frame", "batch", "vert", "trail")
    for i in range(6):
        n, x = cnt_o[i] * SZ[i], 0
        while x < n:
            step = min(n - x, 64 << 20)
            if mo[oo[i] + x:oo[i] + x + step] != mn[on[i] + x:on[i] + x + step]:
                die("VERIFY FAILED: old %s rows differ" % names[i])
            x += step
    ol, nl = po.tex[-1], pn.tex[-1]
    if ol[:2] != nl[:2]:
        die("VERIFY FAILED: the gradient is not the last texture")
    x, n = 0, ol[2]
    while x < n:
        step = min(n - x, 64 << 20)
        if mo[po.o_pix + x:po.o_pix + x + step] != mn[pn.o_pix + x:pn.o_pix + x + step]:
            die("VERIFY FAILED: old pixels differ")
        x += step
    if mo[po.o_pix + ol[2]:po.o_pix + po.pixb] != mn[pn.o_pix + nl[2]:pn.o_pix + pn.pixb]:
        die("VERIFY FAILED: gradient pixels differ")
    if pn.ntr != po.ntr:
        die("VERIFY FAILED: trail window count changed")
    for b in range(po.nba, pn.nba):
        t, fl, bl, cl, _, _, _, fv, nv = struct.unpack_from("<HBBB3BII", mn, pn.o_ba + b * 52)
        if not (t == 0xFFFF or t < pn.ntex - 1) or fv + nv > pn.nve or bl > 2:
            die("VERIFY FAILED: new batch %d out of range" % b)
    for fi in range(po.nfr, pn.nfr):
        fb, n_ = struct.unpack_from("<II", mn, pn.o_fr + fi * 8)
        if fb + n_ > pn.nba:
            die("VERIFY FAILED: new frame %d out of range" % fi)
    for nm in new_fx:
        if nm not in pn.eff:
            die("VERIFY FAILED: %s missing" % nm)
        ff, nf = pn.eff[nm][0], pn.eff[nm][1]
        if nf < 1 or ff + nf > pn.nfr:
            die("VERIFY FAILED: %s frames out of range" % nm)
    if set(pn.eff) - set(po.eff) != set(new_fx):
        die("VERIFY FAILED: unexpected new names %s" % sorted(set(pn.eff) - set(po.eff) - set(new_fx)))
    log("VERIFIED fx: old tex (%d) / eff (%d) / frame (%d) / batch (%d) / vert (%d) / trail (%d) rows and all old pixels (%d B) "
        "byte-identical; the gradient is still the last texture; %d new elements by name, every new row in range" %
        (po.ntex - 1, po.neff, po.nfr, po.nba, po.nve, po.ntr, ol[2], len(new_fx)))
    po.close()
    pn.close()


# ================================================================================================================ sfx
def read_wav(path):
    if not os.path.isfile(path):
        die("missing our sound %s (tools/sorabuild/ours/kb/kbsynth.py makes it)" % path)
    try:
        with wave.open(path, "rb") as w:
            sw, rate, ch, n = w.getsampwidth(), w.getframerate(), w.getnchannels(), w.getnframes()
            fr = w.readframes(n)
    except (wave.Error, EOFError) as e:
        die("%s is not a readable WAV (%s)" % (path, e))
    if sw != 2 or ch not in (1, 2) or not 8000 <= rate <= 96000 or not 2 <= n <= rate * 10 or len(fr) != n * ch * 2:
        die("%s: want PCM16, 1-2 channels, 8-96 kHz, under 10 s (got %d B/sample, %d ch, %d Hz, %d frames)" % (path, sw, ch, rate, n))
    return rate, ch, n, fr


def build_sfx(src, dst, ours, log, part="s216"):
    _, SOUNDS, SFX_MARKER = PARTS[part]  # (s247: the part's sounds + marker)
    if not os.path.isfile(src):
        die("no sfx pack at %s" % src)
    d = open(src, "rb").read()
    if len(d) < 48:
        die("%s is too small for an SKSF pack" % src)
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = SK.unpack_from(d, 0)
    if magic != b"SKSF" or ver != 1:
        die("%s is not an SKSF v1 pack" % src)
    o, sec = 48, {}
    for nm, cnt, sz in (("sounds", ns, 72), ("trig", nt, 28), ("ts", nts, 4), ("vt", nv, 16), ("ch", nc, 8), ("names", nn, 36), ("hits", nh, 8)):
        sec[nm] = d[o:o + cnt * sz]
        o += cnt * sz
    if o > data_off or data_off + data_bytes > len(d):
        die("%s: sections overlap / cut short" % src)
    names = {sec["names"][i * 36:i * 36 + 32].split(b"\0")[0].decode("latin1") for i in range(nn)}
    if SFX_MARKER in names or any(s in names for s in SOUNDS):
        die("the sfx pack already has our Keyblade sounds %s (run on the pack BEFORE this step)" % SFX_MARKER)
    if part == "s216" and (SFX_MARKER_S247 in names):
        die("the sfx pack already has the s247 sounds: the s216 part must run before them")
    pcm = bytearray(d[data_off:data_off + data_bytes])
    new_s, new_names = bytearray(), []
    for i, nm in enumerate(SOUNDS):
        rate, ch, n, fr = read_wav(os.path.join(ours, nm + ".wav"))
        while len(pcm) % 4:
            pcm.append(0)
        new_s += struct.pack("<48sIIfIII", ("ours/kb/%s.wav" % nm).encode(), rate, ch, 1.0, 0, len(pcm), n)
        pcm += fr
        new_names.append((nm, ns + i))
        log("  + sound %-14s #%d: %.3f s, %d Hz, %d ch" % (nm, ns + i, n / rate, rate, ch))
    names_b = b"".join(struct.pack("<32sI", n.encode(), s) for n, s in new_names) + struct.pack("<32sI", SFX_MARKER.encode(), ns)
    body = bytearray(sec["sounds"] + bytes(new_s) + sec["trig"] + sec["ts"] + sec["vt"] + sec["ch"] + sec["names"] + names_b + sec["hits"])
    while (48 + len(body)) % 16:
        body.append(0)
    hdr = SK.pack(b"SKSF", 1, ns + len(SOUNDS), nt, nts, nv, nc, nn + len(new_names) + 1, nh, 48 + len(body), len(pcm), 0)
    tmp = dst + ".tmp"
    with open(tmp, "wb") as f:
        f.write(hdr + body + pcm)
    os.replace(tmp, dst)
    # verify: old rows + old PCM byte-identical, the new names resolve
    n_ = open(dst, "rb").read()
    h2 = SK.unpack_from(n_, 0)
    o2 = 48
    for nm, cnt_old, cnt_new, sz in (("sounds", ns, h2[2], 72), ("trig", nt, h2[3], 28), ("ts", nts, h2[4], 4), ("vt", nv, h2[5], 16),
                                     ("ch", nc, h2[6], 8), ("names", nn, h2[7], 36), ("hits", nh, h2[8], 8)):
        if n_[o2:o2 + cnt_old * sz] != sec[nm]:
            die("VERIFY FAILED: old sfx %s rows differ" % nm)
        o2 += cnt_new * sz
    if h2[3:7] != (nt, nts, nv, nc) or h2[8] != nh:
        die("VERIFY FAILED: trigger / voice / choice / hit counts changed")
    if n_[h2[9]:h2[9] + data_bytes] != d[data_off:data_off + data_bytes]:
        die("VERIFY FAILED: old PCM differs")
    nmap = {}
    base = 48 + 72 * h2[2] + 28 * h2[3] + 4 * h2[4] + 16 * h2[5] + 8 * h2[6]
    for i in range(h2[7]):
        a, s = struct.unpack_from("<32sI", n_, base + 36 * i)
        nmap.setdefault(a.split(b"\0")[0].decode("latin1"), s)
    for k, (nm, s) in enumerate(new_names):
        if nmap.get(nm) != s:
            die("VERIFY FAILED: name %s does not resolve to sound %d" % (nm, s))
    log("%s: %.1f MB (was %.1f MB), +%d sounds, +%d names" % (dst, len(n_) / 1e6, len(d) / 1e6, len(SOUNDS), len(new_names) + 1))
    log("VERIFIED sfx: old sound (%d) / trigger (%d) / trig-sound (%d) / vtbl (%d) / choice (%d) / name (%d) / hit (%d) rows and the "
        "old PCM (%d B) byte-identical; %s resolve by name" % (ns, nt, nts, nv, nc, nn, nh, data_bytes, ", ".join(SOUNDS)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fx-in", required=True)
    ap.add_argument("--sfx-in", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--ours", required=True, help="tools/sorabuild/ours/kb (our synthesised WAVs)")
    ap.add_argument("--no-fx", action="store_true")
    ap.add_argument("--no-sfx", action="store_true")
    ap.add_argument("--part", choices=sorted(PARTS), default="s216",
                    help="s216 = Sealing Blade + Guardian Core (+ the s217 beam rolls; step fxsfx_kbown), s247 = Silent Princess + "
                         "Calamity's Gaze (step fxsfx_kbown2, after the party steps)")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    def log(m):
        print(m, flush=True)
    if not a.no_sfx:  # (sfx first: cheap, and it checks our WAVs before the big fx write)
        build_sfx(a.sfx_in, os.path.join(a.out, "sfx.bin"), a.ours, log, a.part)
    if not a.no_fx:
        build_fx(a.fx_in, os.path.join(a.out, "fx.bin"), log, a.part)


if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        raise
    except Exception:  # never an unhandled crash: say what failed, exit 1
        traceback.print_exc()
        raise SystemExit("[kbownfx] failed (see above)")
