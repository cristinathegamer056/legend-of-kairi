"""(s223) host test for swsetup.py: the Savage Wind folder kinds, the pinned stock zip, the safe unpack (manifest last), the
upgrade journal (a cut-off upgrade carries on), backup once, the busy + other-version refusals (nothing changed), the
stock setup's result lines and (s254) the upgrade from 0.5.3: cut off mid-unpack, the old version's leftover files removed.
python tools/installer/test_swsetup.py"""
import hashlib
import io
import json
import os
import shutil
import sys
import tempfile
import urllib.request
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swsetup  # noqa: E402

GID = swsetup.GAME_IDS[0]      # the game code id of the version the release is for
GID_053 = "7689ce31c1953be6"
TMP = os.environ.get("LOK_TEST_TMP")


def tree_hash(d):
    h = hashlib.sha256()
    for r, ds, fs in sorted(os.walk(d)):
        ds.sort()
        for n in sorted(fs):
            p = os.path.join(r, n)
            h.update(os.path.relpath(p, d).encode() + b"\0" + open(p, "rb").read())
    return h.hexdigest()


def fake_zip(path, extra=None, version=swsetup.SW_WANT):
    """a tiny stock-like zip: savagewind.exe (carries GID), README.md, swgen/swgen.exe, sw-manifest.txt (with every f line)"""
    files = {"savagewind.exe": b"MZ stock new " + GID.encode() + b"\x00 end", "README.md": b"# Savage Wind\nno zip name here\n",
             "swgen/swgen.exe": b"swgen", "gamelib-src/a.c": b"int a;"}
    files.update(extra or {})
    man = "savagewind-update-manifest 1\nversion %s\n" % version + "".join(
        "f %s %d %s\n" % (hashlib.sha256(b).hexdigest(), len(b), n) for n, b in sorted(files.items()))
    with zipfile.ZipFile(path, "w") as z:
        z.writestr("Savage Wind/", "")
        for n, b in files.items():
            z.writestr("Savage Wind/" + n, b)
        z.writestr("Savage Wind/" + swsetup.MANIFEST, man)
    return path


def pin(path):
    swsetup.STOCK = dict(swsetup.STOCK, size=os.path.getsize(path), sha256=swsetup.sha256_file(path))


def make_041(sw):
    os.makedirs(os.path.join(sw, "extracted", "exefs"))
    os.makedirs(os.path.join(sw, "extracted", "romfs", "Pack"))
    os.makedirs(os.path.join(sw, "save", "save_0", "0"))
    for rel, b in {"savagewind.exe": b"MZ old 0.4.1 767a39f088e81e1a\x00", "README.md": b"get SavageWind-0.4.1-alpha.4-windows.zip",
                   "savagewind.ini": b"volume = 0.30\n", "save/save_0/0/game_data.sav": b"SAVE41",
                   "extracted/exefs/main": b"main", "extracted/romfs/Pack/Bootup.pack": b"boot",
                   "extracted/swgame.dll": b"old lib", "extracted/swgame.dll.id": b"767a39f088e81e1a\n"}.items():
        with open(os.path.join(sw, *rel.split("/")), "wb") as f:
            f.write(b)


def make_053(sw):
    """(s254) a set-up Savage Wind 0.5.3 folder: its own sw-manifest.txt (with files 0.6.0 no longer has), game code, a
    save, settings, and files of the player's own that no manifest lists"""
    make_041(sw)
    files = {"savagewind.exe": b"MZ old 0.5.3 " + GID_053.encode() + b"\x00", "README.md": b"# Savage Wind 0.5.3\n",
             "swgen/swgen.exe": b"old swgen", "swgen/_internal/ucrtbase.dll": b"old crt",
             "swgen/_internal/gone/api-ms-win-crt-time-l1-1-0.dll": b"old stub", "oldtool/only053.exe": b"x"}
    for rel, b in files.items():
        os.makedirs(os.path.dirname(os.path.join(sw, *rel.split("/"))), exist_ok=True)
        with open(os.path.join(sw, *rel.split("/")), "wb") as f:
            f.write(b)
    with open(os.path.join(sw, swsetup.MANIFEST), "w", newline="\n") as f:
        f.write("savagewind-update-manifest 1\nversion %s\n" % swsetup.SW_PREV + "".join(
            "f %s %d %s\n" % (hashlib.sha256(b).hexdigest(), len(b), n) for n, b in sorted(files.items())))
    for rel, b in {"extracted/swgame.dll.id": GID_053.encode() + b"\n", "cache/last-version.txt": swsetup.SW_PREV.encode() + b"\n",
                   "swgen/_internal/players_own.txt": b"mine", "mods/mine.txt": b"mine too"}.items():
        os.makedirs(os.path.dirname(os.path.join(sw, *rel.split("/"))), exist_ok=True)
        with open(os.path.join(sw, *rel.split("/")), "wb") as f:
            f.write(b)


def our_exe(d):
    p = os.path.join(d, "bin", "savagewind.exe")
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "wb") as f:
        f.write(b"MZ kairi " + GID.encode() + b"\x00")
    return p


def setup_ok(sw, out, log, on_proc, alive):
    """stands in for stock's setup: builds the new game code + the version mark"""
    with open(os.path.join(sw, "extracted", "swgame.dll"), "wb") as f:
        f.write(b"new lib")
    with open(os.path.join(sw, "extracted", "swgame.dll.id"), "w") as f:
        f.write(GID + "\n")
    os.makedirs(os.path.join(sw, "cache"), exist_ok=True)
    with open(os.path.join(sw, "cache", "last-version.txt"), "w") as f:
        f.write(swsetup.SW_WANT + "\n")
    return "ready", ""


def no_busy(sw):
    return ""


def t_kinds(d):
    sw = os.path.join(d, "k")
    os.makedirs(sw)
    assert swsetup.sw_kind(sw) == "unknown"
    open(os.path.join(sw, "README.md"), "w").write("SavageWind-0.4.1-alpha.4-windows.zip")
    assert swsetup.sw_kind(sw) == "0.4.1"
    open(os.path.join(sw, "README.md"), "w").write("SavageWind-0.3.0-alpha.2-windows.zip")
    assert swsetup.sw_kind(sw) == "other:0.3.0-alpha.2", swsetup.sw_kind(sw)
    open(os.path.join(sw, swsetup.MANIFEST), "w").write("savagewind-update-manifest 1\nversion 0.6.0-alpha.6\n")
    assert swsetup.sw_kind(sw) == "0.6.0" == swsetup.READY and swsetup.SW_WANT == "0.6.0-alpha.6"
    open(os.path.join(sw, swsetup.MANIFEST), "w").write("savagewind-update-manifest 1\nversion 0.5.3-alpha.5\n")
    assert swsetup.sw_kind(sw) == "0.5.3"       # (s254) Legend of Kairi 0.1.5 - 0.1.10's: upgraded in place
    open(os.path.join(sw, swsetup.MANIFEST), "w").write("savagewind-update-manifest 1\nversion 0.6.1-alpha.1\n")
    assert swsetup.sw_kind(sw) == "other:0.6.1-alpha.1"
    assert "Savage Wind 0.6.1-alpha.1" in swsetup.kind_message("other:0.6.1-alpha.1")
    assert "0.1.4" in swsetup.kind_message("unknown") and "Nothing was changed" in swsetup.kind_message("unknown")
    # (s226, s254) an older 0.5.x / 0.6.0 alpha that no release of ours ran on: the new zip over the folder (never "press
    # Update": Savage Wind's own updater goes to upstream's newest version)
    for k in ("other:0.5.0-alpha.2", "other:0.5.1-alpha.1", "other:0.5.2-alpha.3", "other:0.5.4-alpha.1", "other:0.5.10",
              "other:0.6.0-alpha.1", "other:0.6.0-alpha.5"):
        m = swsetup.kind_message(k)
        assert "extract its zip over" in m and "press Update" not in m and "Nothing was changed" in m and "0.1.4" in m, m
        assert "Savage Wind 0.6.0-alpha.6" in m, m
    for k in ("other:0.6.1-alpha.1", "other:0.6.0-alpha.7", "other:0.6.0-alpha.16", "other:0.6.0", "other:0.7.0-alpha.1",
              "other:0.3.0-alpha.2", "other:1.0.5.3", "unknown"):
        m = swsetup.kind_message(k)
        assert "press Update" not in m and "extract its zip" not in m and "choose another folder" in m, m
        assert "0.5.3-alpha.5 or 0.4.1-alpha.4 folder up to 0.6.0-alpha.6" in m, m
    open(os.path.join(sw, swsetup.JOURNAL), "w").write("{}")
    # (s226 review G1) a stale journal in a folder of another version: refused, never "upgrading"
    assert swsetup.sw_kind(sw) == "other:0.6.1-alpha.1", swsetup.sw_kind(sw)
    open(os.path.join(sw, swsetup.MANIFEST), "w").write("savagewind-update-manifest 1\nversion 0.6.0-alpha.6\n")
    assert swsetup.sw_kind(sw) == "upgrading"   # unpacked to the end, stock's setup still to run
    open(os.path.join(sw, swsetup.MANIFEST), "w").write("savagewind-update-manifest 1\nversion 0.5.3-alpha.5\n")
    assert swsetup.sw_kind(sw) == "upgrading"   # (s254) from 0.5.3, cut off mid-unpack: its manifest is replaced last
    os.remove(os.path.join(sw, swsetup.MANIFEST))
    assert swsetup.sw_kind(sw) == "upgrading"   # from 0.4.1 (no manifest), cut off mid-unpack
    for k in ("0.6.0", "0.5.3", "0.4.1", "upgrading"):
        assert swsetup.kind_message(k) == ""


def t_names():
    ok = ["Savage Wind/a.dll", "Savage Wind/zig/lib/x.h", "Savage Wind/SW Update.exe"]
    bad = ["Savage Wind/../x", "Other/x", "Savage Wind/a\\b", "Savage Wind/C:x", "Savage Wind/", "Savage Wind//x",
           "Savage Wind/./x", "Savage Wind/x.kairi-new", "Savage Wind/x. ", "Savage Wind/dir./x", "/Savage Wind/x"]
    for n in ok:
        assert swsetup.safe_name(n), n
    for n in bad:
        assert swsetup.safe_name(n) is None, n


def t_fetch(d):
    z = fake_zip(os.path.join(d, "stock.zip"))
    pin(z)
    assert swsetup.fetch_stock(os.path.join(d, "dl0"), z, log=lambda s: None) == z
    assert not os.path.exists(os.path.join(d, "dl0")), "a local zip is never copied"
    swsetup.STOCK["sha256"] = "0" * 64
    try:
        swsetup.fetch_stock(os.path.join(d, "dl0"), z, log=lambda s: None)
        raise AssertionError("a wrong sha must be refused")
    except swsetup.Fail as e:
        assert "not the official one" in str(e)
    pin(z)
    data = open(z, "rb").read()

    class Op:
        def __init__(self, body):
            self.body = body

        def open(self, req, timeout=0):
            assert req.full_url.startswith("https://")
            return io.BytesIO(self.body)
    dl = os.path.join(d, "dl")
    p = swsetup.fetch_stock(dl, None, log=lambda s: None, opener=Op(data))
    assert p == os.path.join(dl, swsetup.STOCK["name"]) and open(p, "rb").read() == data and not os.path.exists(p + ".part")
    assert swsetup.fetch_stock(dl, None, log=lambda s: None, opener=Op(b"")) == p, "a complete earlier download is reused"
    os.remove(p)
    for body, frag in ((data[:-5], "did not check out"), (data + b"xx", "bigger than")):
        try:
            swsetup.fetch_stock(dl, None, log=lambda s: None, opener=Op(body))
            raise AssertionError(frag)
        except swsetup.Fail as e:
            assert frag in str(e), e
        assert not os.path.exists(p)
    h = swsetup._HttpsOnly()
    try:
        h.redirect_request(urllib.request.Request("https://x/a"), None, 302, "", {}, "http://evil/a")
        raise AssertionError("an http redirect must be refused")
    except swsetup.Fail:
        pass


def t_unpack(d):
    z = fake_zip(os.path.join(d, "u.zip"))
    sw = os.path.join(d, "u")
    make_041(sw)
    order = []
    real = os.replace

    def rec(a, b):
        order.append(os.path.basename(b))
        real(a, b)
    swsetup.os.replace = rec
    try:
        n = swsetup.unpack_stock(z, sw, log=lambda s: None)
    finally:
        swsetup.os.replace = real
    assert n == 5 and order[-1] == swsetup.MANIFEST, order
    assert swsetup.program_files_ok(sw) == "" and swsetup.sw_kind(sw) == swsetup.READY
    assert open(os.path.join(sw, "save", "save_0", "0", "game_data.sav"), "rb").read() == b"SAVE41"
    assert open(os.path.join(sw, "savagewind.ini"), "rb").read() == b"volume = 0.30\n"
    assert not any(f.endswith(swsetup.NEW) for _, _, fs in os.walk(sw) for f in fs)
    with open(os.path.join(sw, "swgen", "swgen.exe"), "ab") as f:
        f.write(b"x")
    assert swsetup.program_files_ok(sw) == "swgen/swgen.exe"
    # an odd name anywhere in the zip: nothing at all is written
    bad = os.path.join(d, "bad.zip")
    with zipfile.ZipFile(bad, "w") as zz:
        zz.writestr("Savage Wind/README.md", "new")
        zz.writestr("Savage Wind/../evil.txt", "x")
        zz.writestr("Savage Wind/" + swsetup.MANIFEST, "version x\n")
    sw2 = os.path.join(d, "u2")
    make_041(sw2)
    before = tree_hash(sw2)
    try:
        swsetup.unpack_stock(bad, sw2, log=lambda s: None)
        raise AssertionError("odd name")
    except swsetup.Fail:
        pass
    assert tree_hash(sw2) == before and not os.path.exists(os.path.join(d, "evil.txt"))


def t_upgrade(d):
    z = fake_zip(os.path.join(d, "up.zip"))
    pin(z)
    sw, lok = os.path.join(d, "sw"), os.path.join(d, "lok")
    make_041(sw)
    exe = our_exe(lok)
    work = os.path.join(lok, "work")
    # 1. stock's setup fails part-way: the journal stays, the folder says "upgrading", the backup is the 0.4.1 one
    try:
        swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy,
                             run_setup=lambda *a: ("failed", "cut off"))
        raise AssertionError("setup failure must raise")
    except swsetup.Fail as e:
        assert "did not finish" in str(e)
    assert swsetup.sw_kind(sw) == "upgrading"
    bk = os.path.join(work, "sw-upgrade", "backup-" + swsetup.SW_OLD)
    assert open(os.path.join(bk, "save", "save_0", "0", "game_data.sav"), "rb").read() == b"SAVE41"
    # the player plays on meanwhile (or the save changes): a retry never overwrites the first copy
    with open(os.path.join(sw, "save", "save_0", "0", "game_data.sav"), "wb") as f:
        f.write(b"LATER")
    # 2. cut off in the middle of the unpack: half the files are new, the manifest is not there yet
    os.remove(os.path.join(sw, swsetup.MANIFEST))
    with open(os.path.join(sw, "README.md"), "w") as f:
        f.write("SavageWind-0.4.1-alpha.4-windows.zip")
    assert swsetup.sw_kind(sw) == "upgrading", "the journal, not the files, tells a cut-off upgrade"
    # 3. again: carries on, sets up, done
    r = swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok)
    assert r == "upgraded", r
    assert swsetup.sw_kind(sw) == swsetup.READY and not os.path.exists(os.path.join(sw, swsetup.JOURNAL))
    assert open(os.path.join(bk, "save", "save_0", "0", "game_data.sav"), "rb").read() == b"SAVE41"
    assert open(os.path.join(sw, "save", "save_0", "0", "game_data.sav"), "rb").read() == b"LATER", "saves are never replaced"
    assert swsetup.game_files_state(sw, exe) == ""
    # 4. once more: nothing to do
    assert swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy,
                                run_setup=lambda *a: (_ for _ in ()).throw(AssertionError("no setup"))) == "ready"
    # 5. Savage Wind moved its game code on (its own updater): the state says so, a setup runs again
    with open(os.path.join(sw, "extracted", "swgame.dll.id"), "w") as f:
        f.write("1111111111111111\n")
    assert "another Savage Wind version" in swsetup.game_files_state(sw, exe)
    assert swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok) == "set up"
    # 6. last-version mark missing = its setup never finished
    os.remove(os.path.join(sw, "cache", "last-version.txt"))
    assert "has not finished" in swsetup.game_files_state(sw, exe)


def t_upgrade_053(d):
    """(s254) the Legend of Kairi 0.1.5 - 0.1.10 players' folder: Savage Wind 0.5.3 -> the new version in place"""
    z = fake_zip(os.path.join(d, "up.zip"))
    pin(z)
    sw, lok = os.path.join(d, "sw"), os.path.join(d, "lok")
    make_053(sw)
    exe = our_exe(lok)
    work = os.path.join(lok, "work")
    assert swsetup.sw_kind(sw) == "0.5.3"
    assert "another Savage Wind version" in swsetup.game_files_state(sw, exe)      # 0.5.3's game code, our exe is the new one's
    # 1. cut off in the middle of the unpack (some files new, sw-manifest.txt still 0.5.3's): the journal says "upgrading"
    real_unpack = swsetup.unpack_stock

    def half(zp, swd, log=print):
        with open(os.path.join(swd, "savagewind.exe"), "wb") as f:
            f.write(b"MZ stock new " + GID.encode() + b"\x00 end")
        raise swsetup.Fail("cut off")
    swsetup.unpack_stock = half
    try:
        swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok)
        raise AssertionError("the cut-off unpack must raise")
    except swsetup.Fail as e:
        assert "cut off" in str(e)
    finally:
        swsetup.unpack_stock = real_unpack
    assert swsetup.manifest_version(sw) == swsetup.SW_PREV and swsetup.sw_kind(sw) == "upgrading"
    j = json.load(open(os.path.join(sw, swsetup.JOURNAL), encoding="utf-8"))
    assert j["from"] == swsetup.SW_PREV and j["to"] == swsetup.SW_WANT and "swgen/_internal/ucrtbase.dll" in j["old_files"], j
    bk = os.path.join(work, "sw-upgrade", "backup-" + swsetup.SW_PREV)
    assert open(os.path.join(bk, "save", "save_0", "0", "game_data.sav"), "rb").read() == b"SAVE41"
    assert open(os.path.join(bk, "savagewind.ini"), "rb").read() == b"volume = 0.30\n"
    for rel in ("swgen/_internal/ucrtbase.dll", "oldtool/only053.exe"):     # nothing is removed before the new files are all in
        assert os.path.isfile(os.path.join(sw, *rel.split("/"))), rel
    # 2. again: carries on (the old file list comes from the journal), stock's setup, done
    got = []
    r = swsetup.ensure_ready(sw, exe, work, log=got.append, local_zip=z, busy_check=no_busy, run_setup=setup_ok)
    assert r == "upgraded" and swsetup.sw_kind(sw) == swsetup.READY, (r, swsetup.sw_kind(sw))
    assert not os.path.exists(os.path.join(sw, swsetup.JOURNAL)) and swsetup.game_files_state(sw, exe) == ""
    assert any("removed 2 files of the old Savage Wind version" in s for s in got), got
    for rel in ("swgen/_internal/ucrtbase.dll", "swgen/_internal/gone/api-ms-win-crt-time-l1-1-0.dll", "swgen/_internal/gone"):
        assert not os.path.exists(os.path.join(sw, *rel.split("/"))), rel   # 0.5.3's swgen files the new version lacks + their folder
    for rel, b in {"swgen/_internal/players_own.txt": b"mine", "mods/mine.txt": b"mine too", "savagewind.ini": b"volume = 0.30\n",
                   "save/save_0/0/game_data.sav": b"SAVE41", "extracted/exefs/main": b"main", "swgen/swgen.exe": b"swgen",
                   "extracted/romfs/Pack/Bootup.pack": b"boot",
                   "oldtool/only053.exe": b"x"}.items():      # the player's own files, saves, game files; new files; (s255) an
        assert open(os.path.join(sw, *rel.split("/")), "rb").read() == b, rel      # old file outside swgen/ is left alone
    assert swsetup.program_files_ok(sw) == ""
    # 3. once more: nothing to do
    assert swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy,
                                run_setup=lambda *a: (_ for _ in ()).throw(AssertionError("no setup"))) == "ready"
    # remove_stale by itself: only names the old list has, never a file of the new version, never outside the folder
    open(os.path.join(d, "outside.txt"), "w").write("x")
    open(os.path.join(sw, "loose.txt"), "w").write("x")
    open(os.path.join(sw, "swgen", "loose.txt"), "w").write("x")
    before = tree_hash(os.path.join(sw, "save")), tree_hash(os.path.join(sw, "extracted")), tree_hash(os.path.join(sw, "cache"))
    # (s255, review finding 2) the list is the journal's, a file in the player's folder: whatever it names, only files under
    # swgen/ can go, never a save, the settings, the game files or the cache
    n = swsetup.remove_stale(sw, ["../outside.txt", "savagewind.exe", "SWGEN/swgen.exe", "nope.txt", "save", 7, "loose.txt",
                                  swsetup.MANIFEST, "a\\b", "C:x", "swgen/loose.txt", "swgen/../loose.txt", "swgen/../save/save_0/0/game_data.sav",
                                  "save/save_0/0/game_data.sav", "savagewind.ini", "EXTRACTED/exefs/main", "cache/last-version.txt",
                                  "extracted/swgame.dll.id", "mods/mine.txt"], log=lambda s: None)
    assert n == 1 and not os.path.exists(os.path.join(sw, "swgen", "loose.txt")) and os.path.isfile(os.path.join(d, "outside.txt"))
    assert os.path.isfile(os.path.join(sw, "loose.txt")) and os.path.isfile(os.path.join(sw, "mods", "mine.txt"))
    assert open(os.path.join(sw, "savagewind.ini"), "rb").read() == b"volume = 0.30\n"
    assert before == (tree_hash(os.path.join(sw, "save")), tree_hash(os.path.join(sw, "extracted")), tree_hash(os.path.join(sw, "cache")))
    assert swsetup.program_files_ok(sw) == "" and os.path.isdir(os.path.join(sw, "save"))
    # a folder with no new manifest (never the case after a good unpack): nothing is removed
    bare = os.path.join(d, "bare")
    os.makedirs(bare)
    open(os.path.join(bare, "x.dll"), "w").write("x")
    assert swsetup.remove_stale(bare, ["x.dll"], log=lambda s: None) == 0 and os.path.isfile(os.path.join(bare, "x.dll"))


def t_journal(d):
    """(s255, the s254 review's findings 1, 3, 5, 7) a journal is a file in the player's folder: its text never becomes a path;
    an older release's journal (no file list) still cleans up; a failed backup is a plain message; a folder at a drive root"""
    z = fake_zip(os.path.join(d, "j.zip"))
    pin(z)
    lok = os.path.join(d, "lok")
    exe = our_exe(lok)
    work = os.path.join(lok, "work")
    # 1. "from" with a path in it (no manifest: the 0.4.1 kind's cut-off upgrade), a number, a list: the backup folder is one
    # of the two names we know, and a folder next to the install is not touched
    victim = os.path.join(lok, "victim")
    os.makedirs(victim)
    open(os.path.join(victim, "precious.txt"), "w").write("keep")
    for i, frm in enumerate(("x/../../../victim", 5, ["a"], "..", swsetup.SW_WANT)):
        sw = os.path.join(d, "f%d" % i)
        make_041(sw)
        with open(os.path.join(sw, swsetup.JOURNAL), "w") as f:
            json.dump({"from": frm, "to": "x", "old_files": []}, f)
        assert swsetup.sw_kind(sw) == "upgrading"
        w = os.path.join(lok, "work%d" % i)
        assert swsetup.ensure_ready(sw, exe, w, log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok) == "upgraded"
        assert sorted(n for n in os.listdir(os.path.join(w, "sw-upgrade")) if n.startswith("backup")) == ["backup-" + swsetup.SW_OLD]
        assert open(os.path.join(victim, "precious.txt")).read() == "keep" and os.listdir(victim) == ["precious.txt"]
    assert swsetup.journal_from("nowhere", "upgrading", {"from": swsetup.SW_PREV}) == swsetup.SW_PREV
    assert swsetup.journal_from("nowhere", "0.5.3", {"from": "zzz"}) == swsetup.SW_PREV
    assert swsetup.journal_from("nowhere", "upgrading", "text") == swsetup.SW_OLD
    # 3. a journal Legend of Kairi 0.1.5 - 0.1.10 left (0.4.1 -> 0.5.3, cut off after its unpack: 0.5.3's manifest, no list)
    sw = os.path.join(d, "oldj")
    make_053(sw)
    with open(os.path.join(sw, swsetup.JOURNAL), "w") as f:
        json.dump({"from": swsetup.SW_OLD, "to": swsetup.SW_PREV, "started": "2026-11-13 10:00:00"}, f)
    assert swsetup.sw_kind(sw) == "upgrading"
    real_unpack = swsetup.unpack_stock

    def cut(zp, swd, log=print):
        raise swsetup.Fail("cut off")
    swsetup.unpack_stock = cut
    try:
        swsetup.ensure_ready(sw, exe, work, log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok)
        raise AssertionError("the cut-off unpack must raise")
    except swsetup.Fail:
        pass
    finally:
        swsetup.unpack_stock = real_unpack
    j = json.load(open(os.path.join(sw, swsetup.JOURNAL), encoding="utf-8"))       # the list is kept for the retry
    assert j["from"] == swsetup.SW_OLD and j["to"] == swsetup.SW_WANT and "swgen/_internal/ucrtbase.dll" in j["old_files"], j
    got = []
    assert swsetup.ensure_ready(sw, exe, work, log=got.append, local_zip=z, busy_check=no_busy, run_setup=setup_ok) == "upgraded"
    assert any("removed 2 files of the old Savage Wind version" in s for s in got), got
    assert not os.path.exists(os.path.join(sw, "swgen", "_internal", "ucrtbase.dll"))
    assert os.path.isdir(os.path.join(work, "sw-upgrade", "backup-" + swsetup.SW_OLD))      # as the journal said
    assert open(os.path.join(sw, "save", "save_0", "0", "game_data.sav"), "rb").read() == b"SAVE41"
    # 5. the backup can't be written: a plain message, and nothing in the Savage Wind folder changed (no journal yet)
    sw = os.path.join(d, "nobk")
    make_053(sw)
    before = tree_hash(sw)
    real_copytree = shutil.copytree

    def full(*a, **k):
        raise shutil.Error([("a", "b", "[Errno 28] No space left on device")])
    shutil.copytree = full
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "workbk"), log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok)
        raise AssertionError("a failed backup must raise")
    except swsetup.Fail as e:
        assert "Could not save a copy of your saves and settings" in str(e) and "No space left" in str(e) and "[(" not in str(e), e
    finally:
        shutil.copytree = real_copytree
    assert tree_hash(sw) == before and swsetup.sw_kind(sw) == "0.5.3"
    assert swsetup.ensure_ready(sw, exe, os.path.join(lok, "workbk"), log=lambda s: None, local_zip=z, busy_check=no_busy,
                                run_setup=setup_ok) == "upgraded"
    # 7. a folder that is a drive root: one separator at the end, so a file in it counts as inside
    if os.name == "nt":
        root = os.path.splitdrive(os.path.realpath(d))[0] + os.sep
        assert swsetup.inside(root) == root and os.path.realpath(os.path.join(root, "a.txt")).startswith(swsetup.inside(root))
    assert swsetup.inside(d) == os.path.realpath(d) + os.sep


def t_refusals(d):
    z = fake_zip(os.path.join(d, "r.zip"))
    pin(z)
    lok = os.path.join(d, "rlok")
    exe = our_exe(lok)
    # busy
    sw = os.path.join(d, "busy")
    make_041(sw)
    before = tree_hash(sw)
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "work"), log=lambda s: None, local_zip=z,
                             busy_check=lambda s: "Savage Wind is running from that folder (savagewind.exe).", run_setup=setup_ok)
        raise AssertionError("busy")
    except swsetup.Fail as e:
        assert "running" in str(e)
    assert tree_hash(sw) == before and not os.path.exists(os.path.join(lok, "work", "sw-upgrade"))
    # another version
    sw = os.path.join(d, "other")
    make_041(sw)
    with open(os.path.join(sw, swsetup.MANIFEST), "w") as f:
        f.write("savagewind-update-manifest 1\nversion 0.5.1-alpha.1\n")
    before = tree_hash(sw)
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "work"), log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok)
        raise AssertionError("other")
    except swsetup.Fail as e:
        assert "Savage Wind 0.5.1-alpha.1" in str(e) and "Nothing was changed" in str(e), e
    assert tree_hash(sw) == before
    # a download that fails: nothing changed
    sw = os.path.join(d, "nodl")
    make_041(sw)
    before = tree_hash(sw)
    swsetup.STOCK["sha256"] = "1" * 64
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "work"), log=lambda s: None, local_zip=z, busy_check=no_busy, run_setup=setup_ok)
        raise AssertionError("bad zip")
    except swsetup.Fail:
        pass
    assert tree_hash(sw) == before and swsetup.sw_kind(sw) == "0.4.1"
    pin(z)
    # a fresh folder of the new version whose own setup never ran: plain message
    sw = os.path.join(d, "fresh")
    os.makedirs(sw)
    swsetup.unpack_stock(z, sw, log=lambda s: None)
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "work"), log=lambda s: None, busy_check=no_busy, run_setup=setup_ok)
        raise AssertionError("not set up")
    except swsetup.Fail as e:
        assert "not set up yet" in str(e)
    # stock turned the copy down
    sw = os.path.join(d, "refused")
    make_041(sw)
    try:
        swsetup.ensure_ready(sw, exe, os.path.join(lok, "work2"), log=lambda s: None, local_zip=z, busy_check=no_busy,
                             run_setup=lambda *a: ("refused", "no romfs"))
        raise AssertionError("refused")
    except swsetup.Fail as e:
        assert "turned down" in str(e) and "saves" in str(e)


def t_stock_lines(d):
    """run_stock_setup reads stock's own result lines (a .cmd stands in for savagewind.exe)"""
    if os.name != "nt":
        return
    cases = [("[launcher] setup: copied already -> READY", "ready"),
             ("[launcher] setup: no game files -> NOT READY", "refused"),
             ("[gamebuild] building\r\n[launcher] FAILED: zig crashed | x\r\n[launcher] setup: code -> NOT READY", "failed"),
             ("nothing", "failed")]
    for i, (lines, want) in enumerate(cases):
        sw = os.path.join(d, "st%d" % i)
        os.makedirs(sw)
        cmd = os.path.join(sw, "fake.cmd")
        with open(cmd, "w", newline="\r\n") as f:
            f.write("@echo off\n")
            for ln in lines.split("\r\n"):
                f.write("echo %s\n" % ln.replace("|", "^|").replace(">", "^>"))
            f.write("echo root=%SAVAGEWIND_DATA_ROOT% upd=%SAVAGEWIND_NO_UPDATE_CHECK% hid=%SAVAGEWIND_HIDDEN% arg=%1\n")
        got = []
        res, why = swsetup.run_stock_setup(sw, os.path.join(sw, "o.log"), log=got.append, exe=cmd)
        assert res == want, (lines, res, why)
        txt = open(os.path.join(sw, "o.log"), encoding="utf-8", errors="replace").read()
        assert "root=%s upd=1 hid=1 arg=--no-launch-flags" % sw in txt, txt
    assert why == "it ended early (exit 0)", why
    # an outside SAVAGEWIND_* setting never reaches stock's setup
    os.environ["SAVAGEWIND_GAME_LIB"] = "x"
    sw = os.path.join(d, "env")
    os.makedirs(sw)
    cmd = os.path.join(sw, "fake.cmd")
    with open(cmd, "w", newline="\r\n") as f:
        f.write("@echo off\necho lib=[%SAVAGEWIND_GAME_LIB%]\n")
    swsetup.run_stock_setup(sw, os.path.join(sw, "o.log"), log=lambda s: None, exe=cmd)
    del os.environ["SAVAGEWIND_GAME_LIB"]
    assert "lib=[]" in open(os.path.join(sw, "o.log")).read()
    # (s255) the setup run reads a copy of the player's settings with NVIDIA's DLSS download switched off; the player's own
    # file stays byte for byte, and the copy is gone afterwards (also when there was no settings file at all)
    for i, mine in enumerate((b"volume = 0.30\r\ndlss_download = 1", b"volume = 0.30\n", None)):
        sw = os.path.join(d, "ini%d" % i)
        os.makedirs(sw)
        if mine is not None:
            with open(os.path.join(sw, "savagewind.ini"), "wb") as f:
                f.write(mine)
        cmd = os.path.join(sw, "fake.cmd")
        with open(cmd, "w", newline="\r\n") as f:
            f.write('@echo off\necho ini=[%SAVAGEWIND_SETTINGS%]\ncopy /b "%SAVAGEWIND_SETTINGS%" seen.ini >nul\n')
        swsetup.run_stock_setup(sw, os.path.join(sw, "o.log"), log=lambda s: None, exe=cmd)
        assert "ini=[%s]" % swsetup.SETUP_INI in open(os.path.join(sw, "o.log")).read()
        seen = open(os.path.join(sw, "seen.ini"), "rb").read()
        assert seen.splitlines()[-1] == b"dlss_download = 0", seen
        assert seen.startswith(mine or b"") and seen.count(b"volume") == (mine is not None), seen
        assert not os.path.exists(os.path.join(sw, swsetup.SETUP_INI))
        if mine is None:
            assert not os.path.exists(os.path.join(sw, "savagewind.ini"))
        else:
            assert open(os.path.join(sw, "savagewind.ini"), "rb").read() == mine


def main():
    keep = dict(swsetup.STOCK)
    n = 0
    with tempfile.TemporaryDirectory(dir=TMP) as d:
        for t in (t_kinds, t_names, t_fetch, t_unpack, t_upgrade, t_upgrade_053, t_journal, t_refusals, t_stock_lines):
            sub = os.path.join(d, t.__name__)
            os.makedirs(sub)
            t(sub) if t is not t_names else t()
            swsetup.STOCK = dict(keep)
            n += 1
    assert swsetup.STOCK["sha256"] == "9bfc05217ed7e69001e02dee7ae9a6070a23b68cfbf946f23727f0767d4c28f8"
    assert swsetup.STOCK["size"] == 125936779 and swsetup.STOCK["name"] == "SavageWind-0.6.0-alpha.6-windows.zip"
    assert swsetup.STOCK["url"].endswith("/v0.6.0-alpha.6/SavageWind-0.6.0-alpha.6-windows.zip")
    print("ok (%d groups)" % n)


main()
