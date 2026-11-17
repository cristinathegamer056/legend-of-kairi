"""Host test for fx/kbhitse.py (s247 kbse): a synthetic sfx.bin + sound tree + KH2 obj dir (no game bytes).

  python -I tools/sorabuild/test_kbhitse.py        (temp files: the --tmp dir, default a new folder in the TMP folder)
Checks: the names per (set, se500 hit row), the right bank + cue per name, sets sharing a bank share the sounds, the Kingdom Key's
world copies / bank 500 / non-se500 hit rows add nothing, old rows + old PCM byte-identical, the same input -> the same bytes, a
second run refused, a missing sound stops a recipe build (SB_SNDROOT) and only skips that set without it.
"""
import argparse
import contextlib
import io
import json
import os
import shutil
import struct
import sys
import tempfile
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
FAILS = []


def check(cond, what):
    print("%s  %s" % ("ok  " if cond else "FAIL", what))
    if not cond:
        FAILS.append(what)


def wav(path, frames, val, ch=2, rate=48000):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(ch), w.setsampwidth(2), w.setframerate(rate)
        w.writeframes(struct.pack("<h", val) * frames * ch)


def aus(path, bank):
    """a BAR with one type-31 entry as KH2's: "ORIGIN\\0\\0", u16 bank, u16 n, u16 bank, u16 0, "se/obj/se<bank>\\0" ..."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    ent = b"ORIGIN\0\0" + struct.pack("<HHHH", bank, 61, bank, 0) + ("se/obj/se%03d" % bank).encode().ljust(32, b"\0")
    hdr = b"BAR\x01" + struct.pack("<III", 1, 0, 0) + struct.pack("<HH4sII", 31, 0, b"se50", 32, len(ent))
    open(path, "wb").write(hdr + ent)


def pack(path, sounds, names, hits, pcm):
    """SKSF v1 with no triggers / vtbl rows (sfxpack.py layout)"""
    body = b"".join(struct.pack("<48sIIfIII", k.encode(), 48000, 2, v, 0, off, n) for k, v, off, n in sounds)
    body += b"".join(struct.pack("<32sI", n.encode(), s) for n, s in names) + b"".join(struct.pack("<II", a, s) for a, s in hits)
    while (48 + len(body)) % 16:
        body += b"\0"
    open(path, "wb").write(struct.pack("<4s11I", b"SKSF", 1, len(sounds), 0, 0, 0, 0, len(names), len(hits), 48 + len(body),
                                       len(pcm), 0) + body + pcm)


def read(path):
    d = open(path, "rb").read()
    h = struct.unpack_from("<4s11I", d, 0)
    ns, nn, nh = h[2], h[7], h[8]
    keys = [struct.unpack_from("<48s", d, 48 + 72 * i)[0].split(b"\0")[0].decode() for i in range(ns)]
    o = 48 + 72 * ns
    names = dict((struct.unpack_from("<32s", d, o + 36 * i)[0].split(b"\0")[0].decode(), struct.unpack_from("<I", d, o + 36 * i + 32)[0])
                 for i in range(nn))
    return d, h, keys, names


def run(tool, *args, env=None):
    old = dict(os.environ)
    os.environ.clear(), os.environ.update(env or {})
    out = io.StringIO()
    try:
        with contextlib.redirect_stdout(out):
            tool.build(*args, log=lambda m: print(m))
        return 0, out.getvalue()
    except SystemExit as e:
        return 1, str(e)
    finally:
        os.environ.clear(), os.environ.update(old)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tmp")
    a = ap.parse_args()
    tmp = a.tmp or tempfile.mkdtemp(prefix="kbhitse_test_")
    shutil.rmtree(tmp, ignore_errors=True)
    kh2, root = os.path.join(tmp, "kh2"), os.path.join(tmp, "snd")
    # KH2 obj: Oathkeeper 10 = se501, Oblivion 20 + Detection Saber Y0 = se502 (shared), the Kingdom Key's world copy NM = se500
    aus(os.path.join(kh2, "kh2_sixth/original/obj/W_EX010_10.a.us"), 501)
    aus(os.path.join(kh2, "kh2_sixth/original/obj/W_EX010_20.a.us"), 502)
    aus(os.path.join(kh2, "kh2_sixth/original/obj/W_EX010_Y0.a.us"), 502)
    aus(os.path.join(kh2, "kh2_sixth/original/obj/W_EX010_NM.a.us"), 500)
    aus(os.path.join(kh2, "kh2_first/original/obj/W_EX010_U0.a.us"), 506)
    # the sound tree: the banks' cues 0 / 5 (each a different constant sample value)
    snd = {}
    for b, base in ((501, 1000), (502, 2000), (506, 3000)):
        for c in (0, 5):
            rel = "se/se%03d/%03d.wav" % (b, c)
            wav(os.path.join(root, "kh2_sfx", rel), 100 + c, base + c)
            snd["se/se%03d.win32.scd#%d" % (b, c)] = {"file": rel, "rate": 48000, "channels": 2, "volume": 0.8 + c / 100}
    os.makedirs(os.path.join(root, "mined"))
    json.dump({"sounds": snd}, open(os.path.join(root, "mined", "kh2_sounds.kh.json"), "w"))
    # the input pack: se500 #0 / #5 + another bank's sound; hit rows 126 -> #0, 131 -> #5, 1857 -> #0, 1494 -> se593 (not se500)
    pcm = struct.pack("<h", 7) * 400
    src = os.path.join(tmp, "in.bin")
    pack(src, [("se/se500.win32.scd#0", 1.0, 0, 50), ("se/se500.win32.scd#5", 1.0, 200, 50), ("se/se593.win32.scd#0", 0.7, 400, 50)],
         [("hit", 0), ("wall_hit", 1)], [(126, 0), (131, 1), (1857, 0), (1494, 2)], pcm)
    sys.path.insert(0, os.path.join(HERE, "fx"))
    import kbhitse  # noqa: E402
    env = {"SB_SNDROOT": root, "SB_KH2": kh2}
    out = os.path.join(tmp, "out.bin")
    rc, log = run(kbhitse, src, out, kh2, env=env)
    check(rc == 0 and "VERIFIED sfx" in log, "builds and verifies (%s)" % log.strip().splitlines()[-1][:100])
    d, h, keys, names = read(out)
    kbh = sorted(n for n in names if n.startswith("kbh") and n != "kbhitse_v1")
    want = sorted("kbh%s_%d" % (s, r) for s in ("10", "20", "y0", "u0") for r in (126, 131, 1857))
    check(kbh == want, "names = 4 sets x the 3 se500 rows (no nm / bank 500, no se593 row 1494): %d" % len(kbh))
    check("kbhitse_v1" in names and names["kbhitse_v1"] == 3, "marker -> the first new sound")
    check(h[2] == 3 + 6, "6 new sounds (3 banks x 2 cues; y0 shares se502)")
    check(keys[names["kbh20_126"]] == "se/se502.win32.scd#0" and keys[names["kbh20_131"]] == "se/se502.win32.scd#5", "Oblivion -> se502 cues")
    check(names["kbhy0_131"] == names["kbh20_131"], "Detection Saber shares Oblivion's sounds")
    check(keys[names["kbh10_1857"]] == "se/se501.win32.scd#0" and keys[names["kbhu0_126"]] == "se/se506.win32.scd#0", "Oathkeeper / Struggle")
    s = names["kbh10_131"]
    _, rate, ch, vol, loop, off, fr = struct.unpack_from("<48sIIfIII", d, 48 + 72 * s)
    sample = struct.unpack_from("<h", d, h[9] + off)[0]
    check((rate, ch, round(vol, 2), loop, fr, sample) == (48000, 2, 0.85, 0, 105, 1005), "the sound row: rate / ch / volume / frames / PCM")
    check(d[h[9]:h[9] + len(pcm)] == pcm and names["hit"] == 0 and names["wall_hit"] == 1, "old PCM + old names unchanged")
    out2 = os.path.join(tmp, "out2.bin")
    run(kbhitse, src, out2, kh2, env=env)
    check(open(out, "rb").read() == open(out2, "rb").read(), "deterministic")
    rc, log = run(kbhitse, out, out2, kh2, env=env)
    check(rc == 1 and "already has" in log, "a second run is refused")
    os.remove(os.path.join(root, "kh2_sfx", "se/se506/005.wav"))
    rc, log = run(kbhitse, src, out2, kh2, env=env)
    check(rc == 1 and "se506" in log, "recipe build: a missing sound stops the run")
    rc, log = run(kbhitse, src, out2, kh2, env={"SB_FXCACHE": root, "SB_KH2": kh2})
    _, h2, _, names2 = read(out2)
    check(rc == 0 and "NOTE set u0" in log and "kbhu0_126" not in names2 and "kbh20_126" in names2 and h2[2] == 3 + 4,
          "without the recipe (old cache): that set is skipped, the others packed")
    rc, log = run(kbhitse, src, out2, os.path.join(tmp, "nokh2"), env=env)
    check(rc == 1 and "W_EX010" in log, "no KH2 a.us files: a clear stop")
    if not a.tmp:
        shutil.rmtree(tmp, ignore_errors=True)
    print("test_kbhitse: %s (%d failed)" % ("ALL OK" if not FAILS else "FAILED", len(FAILS)))
    sys.exit(1 if FAILS else 0)


if __name__ == "__main__":
    main()
