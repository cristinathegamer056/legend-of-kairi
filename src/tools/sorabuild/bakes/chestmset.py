"""(chestkb) Sora's KH2 treasure-chest open motions into a plain mset that partybake reads like P_EX100.mset.
KH2 keeps them in the chest objects, not in Sora's mset: F_EX030.mset (small chest) / F_EX040.mset (big chest) entry 252
'AZ10' / 'AZ50' is a nested motion set (BAR type 20) whose entry '0010' is Sora's motion (228 body bones, as P_EX100; '0011' /
'0012' are the chest lid's, 12 bones). Slots: base = the small chest, base + 4 = the big chest (id*4: Sora's mot_of(id) finds
var 0 only). The weapon mset has only slot 0 = W_EX010's own slot 0 (the grip at rest), the pose of a slot with no weapon motion.
  python -E -P chestmset.py <base> <out body.mset> <out weapon.mset>      (SORABAKE_KH2 = the KH2 extract)"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # (run with -E -P: no script dir on sys.path)
from barls import entries
from mkmset import bar

CHESTS = (("F_EX030", "AZ10"), ("F_EX040", "AZ50"))   # small, big


def sora_motion(obj, nested):
    m = open(os.path.join(os.environ["SORABAKE_KH2"], "kh2_fifth/original/obj/%s.mset" % obj), "rb").read()
    ent = [e for e in entries(m) if e[2] == nested]
    if len(ent) != 1:
        raise SystemExit("chestmset: %s.mset has %d '%s' entries (1 expected)" % (obj, len(ent), nested))
    sub = m[ent[0][3]:ent[0][3] + ent[0][4]]
    got = [sub[e[3]:e[3] + e[4]] for e in entries(sub) if e[2] == "0010" and e[4]]
    if len(got) != 1:
        raise SystemExit("chestmset: %s.mset %s has %d non-empty '0010' motions (1 expected)" % (obj, nested, len(got)))
    return got[0]


if __name__ == "__main__":
    base, ob, ow = int(sys.argv[1]), sys.argv[2], sys.argv[3]
    body = {base + 4 * k: sora_motion(o, n) for k, (o, n) in enumerate(CHESTS)}
    open(ob, "wb").write(bar(body, base + 4 * len(CHESTS)))
    w = open(os.path.join(os.environ["SORABAKE_KH2"], "kh2_sixth/original/obj/W_EX010.mset"), "rb").read()
    e0 = entries(w)[0]
    open(ow, "wb").write(bar({0: w[e0[3]:e0[3] + e0[4]]}, base + 4 * len(CHESTS)))
    print(ob, sorted(body), [len(v) for v in body.values()])
