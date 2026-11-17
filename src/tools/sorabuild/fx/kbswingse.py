#!/usr/bin/env python3
"""s249 kbswing: each KH2 Keyblade's OWN swing whooshes + its OWN wall clank. Appends to an EXISTING sfx.bin (SKSF v1) without
rebuilding it: every older sound / trigger / trig-sound / vtbl / choice / name / hit row and the old PCM stay byte-identical
(checked at the end).

KH2 facts (KH2 FM exe, recon/s249/S249_HELPER_KBSWING.md):
  whoosh  the mset frame trigger 8 (play_sound p0 bank, p1 cue) runs FUN_14040dd70 case 8 -> FUN_1403c8280(the motion's object,
          p0 * 1000 + p1) -> FUN_1403dadf0(that object, se): bank 500 is re-banked to the OBJECT's own bank (a weapon: its own
          +0xBB0; a party body: its slot-0 weapon's). Every bank-500 play_sound of Sora's msets sits in a WEAPON mset (W_EX010,
          W_EX010_<form>_R / _L; the body msets P_EX100* have none, checked by this tool), so each hand's whooshes come from the
          bank of the Keyblade in that hand.
  wall    FUN_1403d1960 (attack vs BG) plays the fixed se 500010 through FUN_1403dadf0(the attack's source = the weapon) = the
          Keyblade's own bank, cue 10.
A Keyblade's bank = its obj/W_EX010_<xx>.a.us type-31 entry ("se/obj/se5NN"), read by kbhitse.set_banks.

How the runtime plays them (sora_sfx_motion in the exe plays a trigger SLOT; the DLL picks the slots, runtime/sora_kbfx.h):
  every trigger row of a slot s < 0x10000 (Sora + his forms: 0.. / 10000 * (form + 1) + ..; Limit's raw K4xx) is copied:
    ALT + s                          = the slot's rows WITHOUT its weapon whoosh rows (footsteps, voices, body SEs: unchanged)
    ALT + ((1 + 2k + h) << 16) + s   = only hand h's whoosh rows (h 0 = right / main blade, 1 = left / second blade), with the
                                       sounds of Keyblade set k's bank (k = the set's first character in base 36: "20" -> 2,
                                       "n0" -> 23; the Kingdom Key = k 0, its own se500 sounds)
  ALT = 0x1000000, above every other slot, so the table stays sorted by slot. The DLL plays ALT + s and the two hand slots instead
  of s (key on); an old DLL never asks for them.
  names  "kbswingse_v1" (marker), "kbsw<set>" (that set's rows are packed; -> its cue-35 sound), "kbwallse_v1" (marker),
         "kbwl<set>" -> that set's cue-10 sound (the wall clank).
Which pack row is which hand: matched by (slot, frame, cue) against the KH2 weapon msets (WEAPON_MSETS below); a se500 row that
matches no weapon trigger stays in the ALT + s copy (the Kingdom Key's, as today) and is counted in the log.
Sounds: recipe key se/se<bank>.win32.scd#<cue>, the wav + volume of the sound inputs (fx/sndsrc.py), as kbhitse / sfxpack. A
sound byte-identical (PCM, rate, channels, volume) to one already packed reuses it (none of the 26 banks repeats se500's today).
Strict with the recipe sounds (SB_SNDROOT): a missing sound stops the run; the old PHH cache skips that set with a NOTE.

Usage: python -E -P kbswingse.py <in sfx.bin> <out sfx.bin> [--kh2 <KH2 extract>] [--no-wall] [--no-swing]
"""
import argparse
import hashlib
import json
import os
import re
import struct
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(os.path.dirname(HERE), "sounds", "mine"))
import sndsrc  # noqa: E402
import kbhitse  # noqa: E402  (parse, set_banks, read_wav, die)
import kh2bin  # noqa: E402  (mset trigger reader)

ALT = 0x1000000
MARK_SW, MARK_WL = "kbswingse_v1", "kbwallse_v1"
BASE_BANK = 500
WALL_CUE = 10
# (pack slot range, KH2 weapon msets per hand) = sfxpack.py's slot layout: base Sora 0.. (Limit's K4xx >= 844 raw, from KH1F),
# a form at 10000 * (actor - 9) + slot (Valor 1, Wisdom 2, Limit 3, Master 4, Final 5; Anti 6 has no weapon)
WEAPON_MSETS = [
    (0, lambda s: s < 844, ["W_EX010"]),
    (0, lambda s: s >= 844, ["W_EX010_KH1F"]),
    (10000, None, ["W_EX010_BTLF_R", "W_EX010_BTLF_L"]),
    (20000, None, ["W_EX010_MAGF"]),
    (30000, lambda s: s < 844, ["W_EX010_KH1F"]),
    (40000, None, ["W_EX010_TRIF_R", "W_EX010_TRIF_L"]),
    (50000, None, ["W_EX010_ULTF_R", "W_EX010_ULTF_L"]),
]
BODY_MSETS = ["P_EX100", "P_EX100_BTLF", "P_EX100_MAGF", "P_EX100_KH1F", "P_EX100_TRIF", "P_EX100_ULTF"]
def die(msg):
    raise SystemExit("[kbswingse] " + msg)


def set_k(st):
    """the slot index of a set: its first character in base 36 ("00" = the Kingdom Key = 0)"""
    if not re.fullmatch(r"[0-9a-z]0", st):
        die("set %r is not a KH2 Keyblade set name (<char>0)" % st)
    return int(st[0], 36)


def weapon_triggers(kh2):
    """{(pack slot, frame, cue): [hand, ...]} of every bank-500 play_sound frame trigger of Sora's weapon msets"""
    k = kh2bin.Kh2(kh2)
    for m in BODY_MSETS:  # (the KH2 fact the hand split rests on: no bank-500 play_sound in a body mset)
        for i, nm, mo, tr in kh2bin.mset(k.read("obj/%s.mset" % m)):
            if tr and any(ty == 8 and p and p[0] == BASE_BANK for ty, f, p in kh2bin.triggers(tr)[1]):
                die("obj/%s.mset entry %d has a bank-500 play_sound: the body plays whooshes too (not handled)" % (m, i))
    out = {}
    for base, keep, msets in WEAPON_MSETS:
        for h, m in enumerate(msets):
            for i, nm, mo, tr in kh2bin.mset(k.read("obj/%s.mset" % m)):
                if not tr or (keep and not keep(i)):
                    continue
                for ty, f, p in kh2bin.triggers(tr)[1]:
                    if ty == 8 and len(p) >= 2 and p[0] == BASE_BANK:
                        if len(p) > 3 and p[3]:
                            die("obj/%s.mset entry %d: a conditional play_sound (p3 %d): not handled" % (m, i, p[3]))
                        out.setdefault((base + i, f, p[1]), []).append(h)
    return out


def build(src, dst, kh2, log=print, wall=True, swing=True):
    if not os.path.isfile(src):
        die("no sfx pack at %s" % src)
    d = open(src, "rb").read()
    (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes), sec = kbhitse.parse(d, src)
    snd = [struct.unpack_from("<48sIIfIII", sec["sounds"], 72 * i) for i in range(ns)]
    keys = [s[0].split(b"\0")[0].decode("latin1") for s in snd]
    names = {}
    for i in range(nn):
        a, s = struct.unpack_from("<32sI", sec["names"], 36 * i)
        names.setdefault(a.split(b"\0")[0].decode("latin1"), s)
    if MARK_SW in names or MARK_WL in names or any(re.fullmatch(r"kb(sw|wl)[0-9a-z]0", n) for n in names):
        die("the sfx pack already has the per-Keyblade whooshes / wall clank (%s): run on the pack BEFORE this step" % MARK_SW)
    trig = [struct.unpack_from("<7I", sec["trig"], 28 * i) for i in range(nt)]
    ts = list(struct.unpack_from("<%dI" % nts, sec["ts"], 0)) if nts else []
    if any(trig[i][0] > trig[i + 1][0] for i in range(nt - 1)):
        die("the pack's trigger table is not sorted by slot")
    if trig and trig[-1][0] >= ALT:
        die("the pack has trigger slots >= 0x%x already" % ALT)
    key_id = {}
    for i, kk in enumerate(keys):
        key_id.setdefault(kk, i)
    pcm = bytearray(d[data_off:data_off + data_bytes])
    by_hash = {}  # (rate, ch, volume, pcm sha1) -> sound id (old se500 sounds first: a byte copy reuses them)

    def h_of(rate, ch, vol, fr):
        return (rate, ch, round(float(vol), 6), hashlib.sha1(fr).hexdigest())
    banks = kbhitse.set_banks(kh2)
    if "00" in banks:
        die("an obj/W_EX010_00.a.us exists: its set name collides with the Kingdom Key's slot index 0")
    # 1. the trigger rows: which are the weapon whooshes, which hand
    wt = weapon_triggers(kh2) if swing else {}
    hand_rows, body_rows, unmatched, cues = {0: [], 1: []}, [], 0, set()
    for i, t in enumerate(trig):
        slot, frame, kind, vtbl, cyc, cnt, first = t
        if slot >= 0x10000:
            continue
        c = None
        if swing and kind == 0 and cnt == 1:
            m = re.fullmatch(r"se/se%03d\.win32\.scd#(\d+)" % BASE_BANK, keys[ts[first]] if ts[first] < ns else "")
            if m:
                c = int(m.group(1))
        hs = wt.get((slot, frame, c)) if c is not None else None
        if hs:
            hand_rows[hs.pop(0)].append((t, c))
            cues.add(c)
        else:
            if c is not None:
                unmatched += 1
            body_rows.append(t)
    left = sum(len(v) for v in wt.values())
    log("trigger rows: %d of slots < 0x10000; whoosh rows matched to a KH2 weapon trigger: right %d, left %d (cues %s); se500 rows "
        "kept as the Kingdom Key's (no KH2 weapon trigger): %d; KH2 weapon triggers without a pack row: %d"
        % (len(body_rows) + len(hand_rows[0]) + len(hand_rows[1]), len(hand_rows[0]), len(hand_rows[1]), sorted(cues), unmatched, left))
    if swing and not (hand_rows[0] or hand_rows[1]):
        die("no whoosh row in the pack matched a KH2 weapon trigger (not the mod's sfx.bin?)")
    want = sorted(cues | ({WALL_CUE} if wall else set()))
    for c in want:  # the Kingdom Key's own sounds: already in the pack
        kk = "se/se%03d.win32.scd#%d" % (BASE_BANK, c)
        if kk not in key_id:
            die("the pack has no %s (the Kingdom Key's own cue)" % kk)
        s = snd[key_id[kk]]
        by_hash.setdefault(h_of(s[1], s[2], s[3], bytes(pcm[s[5]:s[5] + s[6] * s[2] * 2])), key_id[kk])
    # 2. the sounds of every set's bank at those cues
    sj = sndsrc.sounds_json()
    if not os.path.isfile(sj):
        die("no sound inputs at %s (sorabuild step sounds_build / the SB_SNDROOT tree)" % sj)
    js = json.load(open(sj, encoding="utf-8"))["sounds"]
    new_s, ids_of_bank, skipped, reused = bytearray(), {}, [], 0
    for st in sorted(banks):
        bank = banks[st]
        if bank == BASE_BANK or bank in ids_of_bank:
            continue
        got, miss = {}, []
        for c in want:
            key = "se/se%03d.win32.scd#%d" % (bank, c)
            s = js.get(key)
            w = kbhitse.read_wav(sndsrc.wavfile(s)) if s else None
            if w is None:
                miss.append(key)
            else:
                got[c] = (key, s, w)
        if miss:
            if os.environ.get("SB_SNDROOT"):
                die("set %s (bank se%d): %d sound(s) missing from the sound inputs: %s" % (st, bank, len(miss), ", ".join(miss[:4])))
            log("  NOTE set %s (se%d): %d sound(s) not in the sound inputs (%s ...): skipped, it keeps the Kingdom Key's" % (
                st, bank, len(miss), miss[0]))
            skipped.append(st)
            continue
        ids = {}
        for c in want:
            key, s, (rate, ch, n, fr) = got[c]
            if (rate, ch) != (s["rate"], s["channels"]):
                die("%s: the wav (%d Hz, %d ch) is not the recipe's (%d Hz, %d ch)" % (key, rate, ch, s["rate"], s["channels"]))
            vol = float(s.get("volume", 1.0))
            hk = h_of(rate, ch, vol, fr)
            if hk in by_hash:
                ids[c] = by_hash[hk]
                reused += 1
                continue
            while len(pcm) % 4:
                pcm.append(0)
            ids[c] = by_hash[hk] = ns + len(new_s) // 72
            new_s += struct.pack("<48sIIfIII", key.encode()[:47], rate, ch, vol, 0, len(pcm), n)
            pcm += fr
        ids_of_bank[bank] = ids
    kk_ids = {c: key_id["se/se%03d.win32.scd#%d" % (BASE_BANK, c)] for c in want}
    sets = [("00", kk_ids)] + [(st, ids_of_bank[banks[st]]) for st in sorted(banks) if banks[st] in ids_of_bank]
    sets.sort(key=lambda x: set_k(x[0]))
    # 3. the new trigger rows (sorted by slot: ALT + s, then the (set, hand) blocks in order)
    new_t, new_ts = [], []

    def add(slot, t, sounds):
        new_t.append((slot,) + tuple(t[1:5]) + (len(sounds), nts + len(new_ts)))
        new_ts.extend(sounds)
    if swing:
        for t in body_rows:
            if t[2] == 0:
                add(ALT + t[0], t, ts[t[6]:t[6] + t[5]])
            else:  # (a voice row: kind 1 reads its vtbl, count / first stay as they were)
                new_t.append((ALT + t[0],) + tuple(t[1:]))
        for st, ids in sets:
            k = set_k(st)
            for h in (0, 1):
                for t, c in hand_rows[h]:
                    add(ALT + ((1 + 2 * k + h) << 16) + t[0], t, [ids[c]])
    sl = [r[0] for r in new_t]
    if any(sl[i] > sl[i + 1] for i in range(len(sl) - 1)):
        die("internal: the new trigger rows are not sorted")
    # 4. names
    new_names = []
    if swing:
        new_names.append((MARK_SW, kk_ids[min(cues)]))
        new_names += [("kbsw" + st, ids[35 if 35 in ids else min(cues)]) for st, ids in sets if st != "00"]
    if wall:
        new_names.append((MARK_WL, kk_ids[WALL_CUE]))
        new_names += [("kbwl" + st, ids[WALL_CUE]) for st, ids in sets if st != "00"]
    if not new_names:
        die("nothing to add (--no-wall and --no-swing)")
    nsn = len(new_s) // 72
    names_b = b"".join(struct.pack("<32sI", n.encode(), s) for n, s in new_names)
    trig_b = b"".join(struct.pack("<7I", *r) for r in new_t)
    ts_b = struct.pack("<%dI" % len(new_ts), *new_ts) if new_ts else b""
    body = bytearray(sec["sounds"] + bytes(new_s) + sec["trig"] + trig_b + sec["ts"] + ts_b + sec["vt"] + sec["ch"] + sec["names"]
                     + names_b + sec["hits"])
    while (48 + len(body)) % 16:
        body.append(0)
    if 48 + len(body) + len(pcm) > (1 << 32) - 1:
        die("the pack would pass the uint32 cap")
    hdr = kbhitse.SK.pack(b"SKSF", 1, ns + nsn, nt + len(new_t), nts + len(new_ts), nv, nc, nn + len(new_names), nh, 48 + len(body),
                          len(pcm), 0)
    tmp = dst + ".tmp"
    with open(tmp, "wb") as f:
        f.write(hdr + body + pcm)
    verify(d, tmp, (ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes), sec, new_names, new_t, new_ts, nsn, log)  # (s249 review) before the swap: a failed check leaves dst alone
    os.replace(tmp, dst)
    log("%s: %d B (was %d B, +%.1f MB PCM), +%d sounds (%d banks; %d reused), +%d trigger rows, +%d names (sets: %s)%s" % (
        dst, os.path.getsize(dst), len(d), (len(pcm) - data_bytes) / 1e6, nsn, len(ids_of_bank), reused, len(new_t), len(new_names),
        " ".join(st for st, _ in sets), "; skipped sets: " + " ".join(skipped) if skipped else ""))


def verify(d, dst, counts, sec, new_names, new_t, new_ts, nsn, log):
    ns, nt, nts, nv, nc, nn, nh, data_off, data_bytes = counts
    n_ = open(dst, "rb").read()
    h2 = kbhitse.SK.unpack_from(n_, 0)
    if (h2[2], h2[3], h2[4], h2[5], h2[6], h2[7], h2[8]) != (ns + nsn, nt + len(new_t), nts + len(new_ts), nv, nc, nn + len(new_names), nh):
        die("VERIFY FAILED: section counts")
    o2, at = 48, {}
    for nm, cnt_old, cnt_new, sz in (("sounds", ns, h2[2], 72), ("trig", nt, h2[3], 28), ("ts", nts, h2[4], 4), ("vt", nv, h2[5], 16),
                                     ("ch", nc, h2[6], 8), ("names", nn, h2[7], 36), ("hits", nh, h2[8], 8)):
        if n_[o2:o2 + cnt_old * sz] != sec[nm]:
            die("VERIFY FAILED: old sfx %s rows differ" % nm)
        at[nm] = o2
        o2 += cnt_new * sz
    if n_[h2[9]:h2[9] + data_bytes] != d[data_off:data_off + data_bytes]:
        die("VERIFY FAILED: old PCM differs")
    trig = [struct.unpack_from("<7I", n_, at["trig"] + 28 * i) for i in range(h2[3])]
    if any(trig[i][0] > trig[i + 1][0] for i in range(len(trig) - 1)):
        die("VERIFY FAILED: the trigger table is not sorted by slot")
    for t in trig:
        if t[2] == 0 and (t[6] + t[5] > h2[4] or any(struct.unpack_from("<I", n_, at["ts"] + 4 * (t[6] + j))[0] >= h2[2] for j in range(t[5]))):
            die("VERIFY FAILED: a trigger row's sounds are out of range")
    nmap = {}
    for i in range(h2[7]):
        a, s = struct.unpack_from("<32sI", n_, at["names"] + 36 * i)
        nmap.setdefault(a.split(b"\0")[0].decode("latin1"), s)
    for nm, s in new_names:
        if nmap.get(nm) != s or s >= h2[2]:
            die("VERIFY FAILED: name %s does not resolve to sound %d" % (nm, s))
    for i in range(h2[2]):
        _, _, ch, _, _, off, fr = struct.unpack_from("<48sIIfIII", n_, 48 + 72 * i)
        if off + fr * ch * 2 > h2[10]:
            die("VERIFY FAILED: sound %d out of range" % i)
    log("VERIFIED sfx: old sound (%d) / trigger (%d) / trig-sound (%d) / vtbl (%d) / choice (%d) / name (%d) / hit (%d) rows and the old "
        "PCM (%d B) byte-identical; +%d trigger rows sorted, every trigger sound in range; %d new names resolve" % (
            ns, nt, nts, nv, nc, nn, nh, data_bytes, len(new_t), len(new_names)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sfx_in")
    ap.add_argument("sfx_out")
    ap.add_argument("--kh2", default=os.environ.get("SB_KH2"))
    ap.add_argument("--no-wall", action="store_true", help="leave out the wall clank (cue 10)")
    ap.add_argument("--no-swing", action="store_true", help="leave out the whooshes")
    a = ap.parse_args()
    if not a.kh2:
        die("give --kh2 or set SB_KH2 (the KH2 extract: the Keyblades' obj/W_EX010_<xx>.a.us + Sora's msets)")
    build(a.sfx_in, a.sfx_out, a.kh2, log=lambda m: print(m, flush=True), wall=not a.no_wall, swing=not a.no_swing)


if __name__ == "__main__":
    try:
        main()
    except SystemExit:
        raise
    except Exception:  # never an unhandled crash: say what failed, exit 1
        traceback.print_exc()
        raise SystemExit("[kbswingse] failed (see above)")
