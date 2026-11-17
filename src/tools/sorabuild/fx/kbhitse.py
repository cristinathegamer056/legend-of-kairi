#!/usr/bin/env python3
"""s247 kbse: each KH2 Keyblade's OWN hit sounds. Appends to an EXISTING sfx.bin (SKSF v1) without rebuilding it: every older
sound / trigger / vtbl / choice / name / hit row and the old PCM stay byte-identical (checked at the end).

KH2 fact (KH2 FM exe, recon/s247/S247_HELPER_KBSE.md): an atkp row's hit SE (+0x1c) of bank 500 (500000..500999) is played by
FUN_1403dadf0 from the sound bank of the attack's SOURCE object = the weapon whose mset ran the attack window (Valor's left blade =
its own weapon object); bank 500 + cue -> <that weapon's bank> * 1000 + cue. A weapon's bank is the one its own effect file
obj/W_EX010_<xx>.a.us loads (BAR entry type 31: "se/obj/se5NN"). The mod's hit table (fx/sfxpack.py) maps the atkp rows to se500
(the Kingdom Key's bank); this tool adds the same cues from every other Keyblade's bank.

What it packs (all decided from the input pack + the player's own KH2 files, nothing hard-coded but the name scheme):
  cues     = the se500 cues the pack's hit table already uses (rows whose sound key is se/se500.win32.scd#<cue>; 0..9 + 12 today)
  sets     = every obj/W_EX010_<xx>.a.us with a two-character set name (the per-Keyblade effect files), bank from its type-31 entry;
             bank 500 (the Kingdom Key's, already packed) is skipped; two sets with the same bank (Y0 / Z0 = Oblivion's se502)
             share the sounds
  sounds   = recipe key se/se<bank>.win32.scd#<cue>, the wav + volume of the sound inputs (fx/sndsrc.py: the recipe tree that
             sounds_build decoded from the player's SCD files, exactly like sfxpack.py's se500 hits)
  names    = "kbh<set>_<atkp>" -> that set's sound for that hit row's cue (set lower case, as runtime/sora_kbfx.h KBFX_TAB), for
             every se500 hit row; marker "kbhitse_v1" (the DLL turns the feature on only when the pack has it)
Order: sets sorted by name, cues ascending (deterministic: the same input gives the same bytes).
With the recipe sounds (SB_SNDROOT: every player's build, and the miner's trace) a missing sound STOPS the run; the old PHH cache
(dev only) skips that set with a NOTE (its Keyblade then keeps the Kingdom Key's hits).

Usage: python -E -P kbhitse.py <in sfx.bin> <out sfx.bin> [--kh2 <KH2 extract>]   (--kh2 default: $SB_KH2)
"""
import argparse
import json
import os
import re
import struct
import sys
import traceback
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sndsrc  # noqa: E402  (sound inputs: the recipe tree)

MARKER = "kbhitse_v1"
NAME = "kbh%s_%d"            # kbh<set>_<atkp>  (runtime/sora_kbfx.h kbhitse_id builds the same)
BASE_BANK = 500              # the Kingdom Key's bank: the hit table's own sounds
PKG_ORDER = ["kh2_first", "kh2_second", "kh2_third", "kh2_fourth", "kh2_fifth", "kh2_sixth"]
SK = struct.Struct("<4s11I")


def die(msg):
    raise SystemExit("[kbhitse] " + msg)


def kh2_obj(kh2, name):
    """obj/<name>: original/ file, last package wins (fxrec's rule)"""
    hit = None
    for p in PKG_ORDER:
        c = os.path.join(kh2, p, "original", "obj", name)
        if os.path.isfile(c):
            hit = c
    return hit


def set_banks(kh2):
    """{set (lower case): bank} of every obj/W_EX010_<x>0.a.us in the extract (type-31 entry: "se/obj/se<bank>"). The per-Keyblade
    sets all end in 0 (10 .. r0, Struggle u0 v0 w0, y0 z0); the Kingdom Key's world copies (_NM / _TR / _WI: se500) are not read,
    so a full extract and kh2pick's file list see the same sets."""
    names = set()
    for p in PKG_ORDER:
        d = os.path.join(kh2, p, "original", "obj")
        if os.path.isdir(d):
            names.update(f for f in os.listdir(d) if re.fullmatch(r"W_EX010_[0-9A-Z]0\.a\.us", f))
    if not names:
        die("no obj/W_EX010_<xx>.a.us in the KH2 extract %s" % kh2)
    out = {}
    for nm in sorted(names):
        d = open(kh2_obj(kh2, nm), "rb").read()
        if d[:4] != b"BAR\x01":
            die("%s is not a BAR file" % nm)
        bank = None
        for i in range(struct.unpack_from("<I", d, 4)[0]):
            t, _, _, off, sz = struct.unpack_from("<HH4sII", d, 16 + 16 * i)
            if t == 31:
                m = re.match(rb"se/obj/se(\d{3})\0", d[off + 16:off + sz])
                if not m:
                    die("%s: its type-31 entry names no se/obj/seNNN bank" % nm)
                bank = int(m.group(1))
                if struct.unpack_from("<H", d, off + 12)[0] != bank:
                    die("%s: its type-31 entry's bank fields disagree" % nm)
        if bank is None:
            die("%s has no type-31 (sound bank) entry" % nm)
        out[nm[8:10].lower()] = bank
    return out


def read_wav(path):
    if not os.path.isfile(path):
        return None
    with wave.open(path, "rb") as w:
        sw, rate, ch, n = w.getsampwidth(), w.getframerate(), w.getnchannels(), w.getnframes()
        fr = w.readframes(n)
    if sw != 2 or ch not in (1, 2) or len(fr) != n * ch * 2 or n < 2:
        die("%s: want PCM16, 1-2 channels (got %d B/sample, %d ch, %d frames)" % (path, sw, ch, n))
    return rate, ch, n, fr


def parse(d, src):
    if len(d) < 48:
        die("%s is too small for an SKSF pack" % src)
    magic, ver, ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes, _ = SK.unpack_from(d, 0)
    if magic != b"SKSF" or ver != 1:
        die("%s is not an SKSF v1 pack" % src)
    o, sec = 48, {}
    for nm, cnt, sz in (("sounds", ns, 72), ("trig", nt, 28), ("ts", nts, 4), ("vt", nv, 16), ("ch", nc, 8), ("names", nn, 36),
                        ("hits", nh, 8)):
        sec[nm] = d[o:o + cnt * sz]
        o += cnt * sz
    if o > data_off or data_off + data_bytes > len(d):
        die("%s: sections overlap / cut short" % src)
    return (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes), sec


def build(src, dst, kh2, log=print):
    if not os.path.isfile(src):
        die("no sfx pack at %s" % src)
    d = open(src, "rb").read()
    (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes), sec = parse(d, src)
    keys = [sec["sounds"][i * 72:i * 72 + 48].split(b"\0")[0].decode("latin1") for i in range(ns)]
    names = {sec["names"][i * 36:i * 36 + 32].split(b"\0")[0].decode("latin1") for i in range(nn)}
    if MARKER in names or any(re.fullmatch(r"kbh[0-9a-z]{2}_\d+", n) for n in names):
        die("the sfx pack already has the per-Keyblade hit sounds (%s): run on the pack BEFORE this step" % MARKER)
    # the hit rows that play a se500 cue
    rows = {}
    for i in range(nh):
        atkp, s = struct.unpack_from("<II", sec["hits"], 8 * i)
        m = re.fullmatch(r"se/se%03d\.win32\.scd#(\d+)" % BASE_BANK, keys[s] if s < ns else "")
        if m:
            rows[atkp] = int(m.group(1))
    if not rows:
        die("the pack's hit table has no se%03d row (not the mod's sfx.bin?)" % BASE_BANK)
    cues = sorted(set(rows.values()))
    banks = set_banks(kh2)
    log("hit rows on se%d: %d (cues %s); Keyblade sets: %d (%s)" % (BASE_BANK, len(rows), cues, len(banks),
        " ".join("%s=se%d" % kv for kv in sorted(banks.items()))))
    sj = sndsrc.sounds_json()
    if not os.path.isfile(sj):
        die("no sound inputs at %s (sorabuild step sounds_build / the SB_SNDROOT tree)" % sj)
    js = json.load(open(sj, encoding="utf-8"))["sounds"]
    pcm = bytearray(d[data_off:data_off + data_bytes])
    new_s, new_names, have, skipped = bytearray(), [], {}, []
    for st in sorted(banks):
        bank = banks[st]
        if bank == BASE_BANK:
            continue
        want = {c: "se/se%03d.win32.scd#%d" % (bank, c) for c in cues}
        if bank not in have:
            got, miss = {}, []
            for c, key in want.items():
                s = js.get(key)
                w = read_wav(sndsrc.wavfile(s)) if s else None
                if w is None:
                    miss.append(key)
                else:
                    got[c] = (key, s, w)
            if miss:
                if os.environ.get("SB_SNDROOT"):  # (the recipe sounds: strict)
                    die("set %s (bank se%d): %d sound(s) missing from the sound inputs: %s" % (st, bank, len(miss), ", ".join(miss[:4])))
                log("  NOTE set %s (se%d): %d sound(s) not in the sound inputs (%s ...): skipped, it keeps the Kingdom Key's hits"
                    % (st, bank, len(miss), miss[0]))
                skipped.append(st)
                continue
            ids = {}
            for c in cues:
                key, s, (rate, ch, n, fr) = got[c]
                if (rate, ch) != (s["rate"], s["channels"]):
                    die("%s: the wav (%d Hz, %d ch) is not the recipe's (%d Hz, %d ch)" % (key, rate, ch, s["rate"], s["channels"]))
                while len(pcm) % 4:
                    pcm.append(0)
                ids[c] = ns + len(new_s) // 72
                new_s += struct.pack("<48sIIfIII", key.encode()[:47], rate, ch, float(s.get("volume", 1.0)), 0, len(pcm), n)
                pcm += fr
            have[bank] = ids
        for atkp in sorted(rows):
            new_names.append((NAME % (st, atkp), have[bank][rows[atkp]]))
    if not new_names:
        die("no Keyblade set could be packed")
    first = ns
    names_b = b"".join(struct.pack("<32sI", n.encode(), s) for n, s in new_names) + struct.pack("<32sI", MARKER.encode(), first)
    nsn = len(new_s) // 72
    body = bytearray(sec["sounds"] + bytes(new_s) + sec["trig"] + sec["ts"] + sec["vt"] + sec["ch"] + sec["names"] + names_b + sec["hits"])
    while (48 + len(body)) % 16:
        body.append(0)
    if 48 + len(body) + len(pcm) > (1 << 32) - 1:
        die("the pack would pass the uint32 cap")
    hdr = SK.pack(b"SKSF", 1, ns + nsn, nt, nts, nv, nc, nn + len(new_names) + 1, nh, 48 + len(body), len(pcm), 0)
    tmp = dst + ".tmp"
    with open(tmp, "wb") as f:
        f.write(hdr + body + pcm)
    os.replace(tmp, dst)
    verify(d, dst, (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes), sec, new_names, nsn, log)
    log("%s: %d B (was %d B, +%.1f MB PCM), +%d sounds (%d banks x %d cues), +%d names%s" % (
        dst, os.path.getsize(dst), len(d), (len(pcm) - data_bytes) / 1e6, nsn, len(have), len(cues), len(new_names) + 1,
        "; skipped sets: " + " ".join(skipped) if skipped else ""))


def verify(d, dst, counts, sec, new_names, nsn, log):
    ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes = counts
    n_ = open(dst, "rb").read()
    h2 = SK.unpack_from(n_, 0)
    if h2[2] != ns + nsn or h2[3:7] != (nt, nts, nv, nc) or h2[7] != nn + len(new_names) + 1 or h2[8] != nh:
        die("VERIFY FAILED: section counts")
    o2 = 48
    for nm, cnt_old, cnt_new, sz in (("sounds", ns, h2[2], 72), ("trig", nt, h2[3], 28), ("ts", nts, h2[4], 4), ("vt", nv, h2[5], 16),
                                     ("ch", nc, h2[6], 8), ("names", nn, h2[7], 36), ("hits", nh, h2[8], 8)):
        if n_[o2:o2 + cnt_old * sz] != sec[nm]:
            die("VERIFY FAILED: old sfx %s rows differ" % nm)
        o2 += cnt_new * sz
    if n_[h2[9]:h2[9] + data_bytes] != d[data_off:data_off + data_bytes]:
        die("VERIFY FAILED: old PCM differs")
    base = 48 + 72 * h2[2] + 28 * h2[3] + 4 * h2[4] + 16 * h2[5] + 8 * h2[6]
    nmap = {}
    for i in range(h2[7]):
        a, s = struct.unpack_from("<32sI", n_, base + 36 * i)
        nmap.setdefault(a.split(b"\0")[0].decode("latin1"), s)
    for nm, s in new_names + [(MARKER, ns)]:
        if nmap.get(nm) != s:
            die("VERIFY FAILED: name %s does not resolve to sound %d" % (nm, s))
    for i in range(ns, ns + nsn):  # every new sound's PCM inside the data (sora_sfx.c pack_load checks the same)
        _, _, ch, _, _, off, fr = struct.unpack_from("<48sIIfIII", n_, 48 + 72 * i)
        if off + fr * ch * 2 > h2[10]:
            die("VERIFY FAILED: new sound %d out of range" % i)
    log("VERIFIED sfx: old sound (%d) / trigger (%d) / trig-sound (%d) / vtbl (%d) / choice (%d) / name (%d) / hit (%d) rows and the old "
        "PCM (%d B) byte-identical; %d new names + %s resolve" % (ns, nt, nts, nv, nc, nn, nh, data_bytes, len(new_names), MARKER))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sfx_in")
    ap.add_argument("sfx_out")
    ap.add_argument("--kh2", default=os.environ.get("SB_KH2"))
    a = ap.parse_args()
    if not a.kh2:
        die("give --kh2 or set SB_KH2 (the KH2 extract: the Keyblades' obj/W_EX010_<xx>.a.us)")
    build(a.sfx_in, a.sfx_out, a.kh2, log=lambda m: print(m, flush=True))


if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        raise
    except Exception:  # never an unhandled crash: say what failed, exit 1
        traceback.print_exc()
        raise SystemExit("[kbhitse] failed (see above)")
