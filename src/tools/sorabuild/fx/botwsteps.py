#!/usr/bin/env python3
"""(kh2steps) BotW's own per-surface footstep sounds for Sora's KH2 steps: append them to an existing sfx.bin.

Sora keeps KH2's step TIMING (his mset footstep triggers, se705 #16..#26); the exe (runtime/sora_sfx.c,
sora_sfx_step_surface) swaps the sound for the surface under his feet, which the DLL reads from BotW's ground ray.

Source: the player's own BotW romfs, Sound/Resource:
  M_Mt_<Material>.bars   one per surface (Grass, GrassMoss, StoneLight, Wood, Sand, Snow, Metal, ... 31 banks)
                         entries Mt_<Mat>_Pl_Move{Slow,Normal}_{Heel,Toe}{L,R}{00,01}, Mt_<Mat>_Pl_Jump_0N, Mt_<Mat>_Pl_LandWeak_0N
  M_SceneStatic.bars     Water_Pl_Move{Shallow,MdlDepth,Deep}{Slow,Normal}NN (wading splashes)
BARS = 'BARS', u32 count @0x0C, u32 CRC table, then (u32 amta, u32 audio) offset pairs; the entry name = the AMTA's 'STRG'
+ 8; the audio = an FWAV (size @+0x0C), DSP ADPCM, decoded with vgmstream-cli.

Packed (keys / names, the runtime looks them up by these strings):
  sounds  "bs/<bank>/<c>/<k>"   c: s = walk (BotW MoveSlow), n = run (MoveNormal), j = jump push-off, l = landing (LandWeak)
          walk / run = heel + toe mixed into one step (toe TOE_DELAY later), k in the order L00 R00 L01 R01 (cycling = feet alternate)
          LEVEL: BotW's raw recordings are 2x (run) .. 6x (walk) louder than Sora's KH2 steps (user: "really loud"). Each class gets
          ONE volume (Sound.volume) = the KH2 class's loudness (se705 #16-19 run, #20-23 walk, #25 jump, #26 land, as packed in
          the input with their own volume) / the BotW class's mean over the land banks; so every surface sits at KH2's step level
          and the surfaces keep BotW's differences between them (water louder than grass ...). Loudness = the loudest 50 ms RMS.
  names   "bs/<bank>/<c>" -> the class's first sound (its variants follow, consecutive);  "bs_mark" -> the first BotW step sound
  banks   the M_Mt_ name without "M_Mt_" (Grass, StoneDgnHeavy, ...); water: WaterShallow, WaterMdlDepth, WaterDeep (s / n only)

  python botwsteps.py <in sfx.bin> <out sfx.bin> <romfs> <vgmstream-cli> <tmp dir>
"""
import glob
import os
import re
import shutil
import struct
import subprocess
import sys
import wave

TOE_DELAY = {"s": 0.09, "n": 0.06}  # GUESS: heel -> toe gap of a BotW step (s); tune by ear
RATE = 48000
FOOT = [("L", "00"), ("R", "00"), ("L", "01"), ("R", "01")]


def bars_entries(path):
    """{entry name: FWAV bytes}"""
    d = open(path, "rb").read()
    if d[:4] != b"BARS":
        return {}
    n = struct.unpack_from("<I", d, 0x0C)[0]
    tab = 0x10 + 4 * n
    out = {}
    for i in range(n):
        amta, aud = struct.unpack_from("<II", d, tab + 8 * i)
        s = d.find(b"STRG", amta, amta + 0x400)
        if s < 0 or not aud or d[aud:aud + 4] != b"FWAV":
            continue
        name = d[s + 8:d.index(b"\0", s + 8)].decode("ascii", "replace")
        out[name] = d[aud:aud + struct.unpack_from("<I", d, aud + 0x0C)[0]]
    return out


class Dec:
    def __init__(self, vgm, tmp):
        self.vgm, self.tmp, self.n = vgm, tmp, 0

    def pcm(self, fwav):
        """FWAV bytes -> list of int16 mono samples at RATE"""
        self.n += 1
        src = os.path.join(self.tmp, "%d.bfwav" % self.n)
        dst = os.path.join(self.tmp, "%d.wav" % self.n)
        open(src, "wb").write(fwav)
        r = subprocess.run([self.vgm, "-o", dst, src], capture_output=True, text=True)
        if r.returncode or not os.path.exists(dst):
            raise SystemExit("[botwsteps] vgmstream failed on an FWAV: %s" % r.stderr[-300:])
        with wave.open(dst, "rb") as w:
            ch, rate, fr = w.getnchannels(), w.getframerate(), w.readframes(w.getnframes())
        os.remove(src), os.remove(dst)
        s = list(struct.unpack("<%dh" % (len(fr) // 2), fr))
        if ch == 2:
            s = [(s[i] + s[i + 1]) // 2 for i in range(0, len(s), 2)]
        if rate != RATE:
            raise SystemExit("[botwsteps] unexpected rate %d" % rate)
        return s


def mix(a, b, delay):
    off = int(delay * RATE)
    out = list(a) + [0] * max(0, off + len(b) - len(a))
    for i, v in enumerate(b):
        out[off + i] = max(-32768, min(32767, out[off + i] + v))
    return out


def loud(s, rate=RATE, ch=1):
    """the loudest 50 ms window's RMS of interleaved int16 samples"""
    w = max(1, int(0.05 * rate) * ch)
    best = 0.0
    for a in range(0, max(1, len(s) - w + 1), max(1, w // 4)):
        seg = s[a:a + w]
        best = max(best, (sum(x * x for x in seg) / len(seg)) ** 0.5)
    return best


KH2_CLASS = {16: "n", 17: "n", 18: "n", 19: "n", 20: "s", 21: "s", 22: "s", 23: "s", 25: "j", 26: "l"}


def main():
    src, dst, romfs, vgm, tmp = sys.argv[1:6]
    if os.path.abspath(src) == os.path.abspath(dst):
        sys.exit("refusing to overwrite the input")
    res = os.path.join(romfs, "Sound", "Resource")
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    dec = Dec(vgm, tmp)
    classes = []  # (bank, c, [pcm, ...])
    for f in sorted(glob.glob(os.path.join(res, "M_Mt_*.bars"))):
        bank = os.path.basename(f)[5:-5]
        if bank == "Static":
            continue
        e = bars_entries(f)
        pre = "Mt_%s_Pl_" % bank
        for c, mv in (("s", "MoveSlow"), ("n", "MoveNormal")):
            v = []
            for side, k in FOOT:
                h, t = e.get("%s%s_Heel%s%s" % (pre, mv, side, k)), e.get("%s%s_Toe%s%s" % (pre, mv, side, k))
                if h and t:
                    v.append(mix(dec.pcm(h), dec.pcm(t), TOE_DELAY[c]))
                elif h:
                    v.append(dec.pcm(h))
            if v:
                classes.append((bank, c, v))
        for c, what in (("j", "Jump"), ("l", "LandWeak")):
            v = [dec.pcm(e[k]) for k in sorted(e) if k.startswith("%s%s_" % (pre, what))]
            if v:
                classes.append((bank, c, v))
    e = bars_entries(os.path.join(res, "M_SceneStatic.bars"))
    for tier in ("Shallow", "MdlDepth", "Deep"):
        for c, sp in (("s", "Slow"), ("n", "Normal")):
            ks = sorted(k for k in e if re.fullmatch(r"Water_Pl_Move%s%s\d+" % (tier, sp), k))
            if ks:
                classes.append(("Water" + tier, c, [dec.pcm(e[k]) for k in ks]))
    shutil.rmtree(tmp, ignore_errors=True)
    if not classes:
        raise SystemExit("[botwsteps] no BotW step sounds found under %s" % res)

    d = open(src, "rb").read()
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = struct.unpack_from("<4s11I", d, 0)
    assert magic == b"SKSF" and ver == 1
    o = 48
    sounds = d[o:o + 72 * ns]; o += 72 * ns
    mid = d[o:o + 28 * nt + 4 * nts + 16 * nv + 8 * nc]; o += len(mid)
    names = d[o:o + 36 * nn]; o += 36 * nn
    hits = d[o:o + 8 * nh]
    haven = {names[i * 36:i * 36 + 32].split(b"\0")[0] for i in range(nn)}
    if b"bs_mark" in haven:
        sys.exit("[botwsteps] the input already has the BotW steps")
    pcm = bytearray(d[data_off:data_off + data_bytes])
    # the KH2 step level per class (from the input pack: se705's packed step sounds x their volume)
    kh2 = {}
    for i in range(ns):
        key, rate, ch, vol, _, off, fr = struct.unpack_from("<48sIIfIII", sounds, 72 * i)
        key = key.split(bytes(1))[0].decode()
        if key.startswith("se/se705.win32.scd#") and int(key[19:]) in KH2_CLASS:
            smp = struct.unpack_from("<%dh" % (fr * ch), pcm, off)
            kh2.setdefault(KH2_CLASS[int(key[19:])], []).append(loud(smp, rate, ch) * vol)
    botw = {}
    for bank, c, v in classes:
        if not bank.startswith("Water"):
            botw.setdefault(c, []).extend(loud(x) for x in v)
    gain = {}
    for c in "snjl":
        if kh2.get(c) and botw.get(c):
            gain[c] = (sum(kh2[c]) / len(kh2[c])) / (sum(botw[c]) / len(botw[c]))
        else:
            gain[c] = 0.3  # (no KH2 reference in the input: a safe quiet level)
    print("[botwsteps] class volumes (KH2 level / BotW level): " + ", ".join("%s %.3f" % (c, gain[c]) for c in "snjl"))
    new_s, new_n = bytearray(), bytearray()
    first = ns
    for bank, c, v in classes:
        new_n += struct.pack("<32sI", ("bs/%s/%s" % (bank, c)).encode()[:31], ns + len(new_s) // 72)
        for k, s in enumerate(v):
            while len(pcm) % 4:
                pcm.append(0)
            new_s += struct.pack("<48sIIfIII", ("bs/%s/%s/%d" % (bank, c, k)).encode()[:47], RATE, 1, gain[c], 0, len(pcm), len(s))
            pcm += struct.pack("<%dh" % len(s), *s)
    new_n += struct.pack("<32sI", b"bs_mark", first)
    ns2, nn2 = ns + len(new_s) // 72, nn + len(new_n) // 36
    body = bytearray(sounds + bytes(new_s) + mid + names + bytes(new_n) + hits)
    while (48 + len(body)) % 16:
        body.append(0)
    hdr = struct.pack("<4s11I", b"SKSF", 1, ns2, nt, nts, nv, nc, nn2, nh, 48 + len(body), len(pcm), 0)
    open(dst, "wb").write(hdr + body + pcm)
    print("[botwsteps] %d step classes (%d banks), %d sounds -> %s (%.1f MB PCM)" % (
        len(classes), len({b for b, _, _ in classes}), ns2 - ns, dst, len(pcm) / 1e6))


if __name__ == "__main__":
    main()
