"""(s263) Green Tunic outfit: data/outfits/gt_sora_model.bin = the player's own base Sora (data/sora_model.bin) with ONLY the
texels of body textures 0 and 1 recoloured to Link's green tunic colours (green tunic, brown leather + boots, cream shirt, tan
front panel). Face, hair, skin and every Keyblade texture stay byte-identical; geometry / groups / bones are untouched, so the
file has the same size and layout as sora_model.bin. data/ragmap/sora.bin (bone parents + bind heights) finds the shoes and
gloves. The colours are the user's approved v3 (s263, sb/s263/tunic/recolor.py): this is that script without its pictures.

  python -E -P tunic.py <sora_model.bin> <ragmap/sora.bin> <out gt_sora_model.bin>

Deterministic on every PC: the kit's own numpy 2.5.3 + Pillow 12.3.0 (the dev PC's too) and only IEEE basic float32 / float64
arithmetic (+ - * /, min / max, clip, fmod in colorsys' `% 1.0`), all correctly rounded: no sin / cos / exp / pow, no
library math whose last bit could differ between machines. The shoe / glove masks = Pillow polygon fill (integer vertices,
fixed C code) + a 4:1 BOX resize (fixed-point integers). Same input bytes -> same output bytes (b0032965.. from this PC's
sora_model.bin 597f1854.. + ragmap/sora.bin a1a891de..).
"""
import colorsys
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw

# the palette (by eye from BotW's green tunic, tuned from the renders; approved by the user in s263: do not retune)
GREEN = np.array([0.33, 0.56, 0.24])
BROWN = np.array([0.50, 0.31, 0.15])
BOOT = np.array([0.40, 0.25, 0.13])
CREAM = np.array([0.93, 0.89, 0.78])
GOLD = np.array([0.86, 0.66, 0.22])
TAN = np.array([0.80, 0.68, 0.47])  # the shorts' front panel = Link's trouser tan
FOOT_H = 16.0  # cm: bones at / under the ankle = the shoes
S = 4  # mask supersample


def main(src, rag, out):
    d = bytearray(open(src, 'rb').read())
    if d[:4] != b'SKRG':
        raise SystemExit("tunic: %s is not a baked model (SKRG)" % src)
    size0 = len(d)
    nb, ng, nt, nv, ni, nx = struct.unpack_from('<6I', d, 8)
    t0 = 32
    g0 = t0 + 16 * nt
    v0 = g0 + 16 * ng
    i0 = v0 + 16 * nv
    x0 = i0 + 20 * ni
    tex = [struct.unpack_from('<4I', d, t0 + 16 * i) for i in range(nt)]
    grp = [struct.unpack_from('<4I', d, g0 + 16 * i) for i in range(ng)]
    vert = np.frombuffer(bytes(d[v0:v0 + 16 * nv]), np.dtype([('first', '<u4'), ('n', '<u4'), ('u', '<f4'), ('v', '<f4')]))
    infl = np.frombuffer(bytes(d[i0:i0 + 20 * ni]), np.dtype([('bone', '<u4'), ('p', '<f4', 4)]))
    idx = np.frombuffer(bytes(d[x0:x0 + 4 * nx]), '<u4')

    r = open(rag, 'rb').read()
    if r[:4] != b'SRAG':
        raise SystemExit("tunic: %s is not a ragmap file (SRAG)" % rag)
    par = np.frombuffer(r, '<i2', 240, 16)
    bind = np.frombuffer(r, '<f4', 720, 16 + 480).reshape(240, 3)
    up = -1.0 if abs(bind[:, 1].min()) > abs(bind[:, 1].max()) else 1.0  # (KH: down = +)
    hgt = bind[:, 1] * up
    hgt = hgt - hgt.min()

    def under(b, root):
        while b >= 0:
            if b == root:
                return True
            b = par[b]
        return False

    hand = np.array([under(b, 172) or under(b, 80) for b in range(240)])
    vb = infl['bone'][np.minimum(vert['first'], ni - 1)]
    vb = np.minimum(vb, 239)
    masks = {}
    for ti in (0, 1):
        w, h = tex[ti][0], tex[ti][1]
        foot = Image.new('L', (w * S, h * S), 0)
        hnd = Image.new('L', (w * S, h * S), 0)
        df, dh = ImageDraw.Draw(foot), ImageDraw.Draw(hnd)
        for (gt, attr, first, n) in grp:
            if gt != ti:
                continue
            tri = idx[first:first + n].reshape(-1, 3)
            for a, b, c in tri:
                bs = vb[[a, b, c]]
                poly = [((vert['u'][k] % 1.0) * w * S, (vert['v'][k] % 1.0) * h * S) for k in (a, b, c)]
                if (hgt[bs] < FOOT_H).all():
                    df.polygon(poly, fill=255, outline=255)
                if hand[bs].all():
                    dh.polygon(poly, fill=255, outline=255)
        masks[ti] = (np.asarray(foot.resize((w, h), Image.BOX)) > 96, np.asarray(hnd.resize((w, h), Image.BOX)) > 96)
        print('tex', ti, 'foot texels', masks[ti][0].sum(), 'hand texels', masks[ti][1].sum())

    def recolor(ti):
        w, h, off, _ = tex[ti]
        a = np.frombuffer(bytes(d[off:off + w * h * 4]), np.uint8).reshape(h, w, 4).astype(np.float32) / 255.0
        rgb = a[:, :, [2, 1, 0]].copy()
        mx, mn = rgb.max(2), rgb.min(2)
        val = mx
        sat = np.where(mx > 1e-5, (mx - mn) / np.maximum(mx, 1e-5), 0)
        hue = np.zeros_like(mx)
        for y in range(h):
            for x in range(w):
                hue[y, x] = colorsys.rgb_to_hsv(*rgb[y, x])[0] * 360.0
        foot, hnd = masks[ti]
        out = rgb.copy()
        lum = (0.30 * rgb[:, :, 0] + 0.59 * rgb[:, :, 1] + 0.11 * rgb[:, :, 2])
        cloth = np.ones((h, w), bool)
        if ti == 1:  # texture 1 = face + hair + skin; the clothes on it are the red pouches and the dark / navy hood + shirt
            cloth = ((hue < 12) | (hue > 345)) & (sat > 0.55)  # the red
            cloth |= (hue > 200) & (hue < 290) & (sat > 0.15) & (val < 0.45)  # navy / dark purple cloth (the eyes are bright blue)
            cloth |= (val < 0.20) & (sat < 0.6)  # black cloth
            yy, xx = np.mgrid[0:h, 0:w]
            cloth &= (yy > 150) & (xx > 100)  # (the face, ears and the hair's shadows sit above / left of this)
        dark = cloth & (sat < 0.45) & (val < 0.42)  # black leather / cloth
        navy = cloth & (hue > 195) & (hue < 290) & (sat >= 0.20) & (val < 0.75)
        yel = cloth & (hue > 18) & (hue < 62) & (sat > 0.45) & (val > 0.25)
        red = cloth & ((hue < 14) | (hue > 345)) & (sat > 0.5)

        def tint(mask, col, lo, gain):
            k = np.clip(lo + lum * gain, 0, 1.25)
            out[mask] = np.clip(col[None, :] * k[mask][:, None], 0, 1)

        tint(dark & ~foot, GREEN, 0.34, 2.3)   # the black jacket / trousers -> the tunic's green (shadows lifted)
        tint(dark & foot, BOOT, 0.40, 2.2)     # the shoes -> brown boots
        tint(dark & hnd, BROWN, 0.40, 2.2)     # the gloves -> brown leather
        if ti == 0:  # texture 0's navy = the shorts' front panel (+ the glove's blue, cream)
            tint(navy & ~hnd, TAN, 0.42, 1.9)
            tint(navy & hnd, CREAM, 0.30, 1.9)
        else:
            tint(navy, CREAM, 0.30, 1.9)       # navy cloth -> the cream shirt
        tint(yel & ~foot, BROWN, 0.25, 1.25)   # the yellow straps -> brown leather belts
        tint(yel & foot, GOLD, 0.25, 1.1)      # the shoes' yellow -> gold trim
        tint(red, BROWN, 0.35, 1.6)            # the red pouches -> brown pouches
        res = a.copy()
        res[:, :, [2, 1, 0]] = out
        b = (res * 255.0 + 0.5).astype(np.uint8)
        d[off:off + w * h * 4] = b.tobytes()
        print('tex', ti, 'changed texels', int((np.abs(out - rgb).max(2) > 0.004).sum()), 'of', w * h)

    for ti in (0, 1):
        recolor(ti)
    if len(d) != size0:
        raise SystemExit("tunic: size changed (%d -> %d)" % (size0, len(d)))
    with open(out, 'wb') as f:
        f.write(d)
    print('wrote', out, len(d), 'bytes')


if __name__ == '__main__':
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    main(*sys.argv[1:])
