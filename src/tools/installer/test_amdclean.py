"""Tests for gameside.remove_amd_tests (s241): an update / install takes the "AMD test 1 - 4b" files (GitHub issue 2, sent by DM) out of
the install folder, exact names only; the player's own files, logs/ and anything behind a link stay.
Run: python tools/installer/test_amdclean.py      (Windows for the junction + in-use cases; they are skipped elsewhere)
"""
import json
import os
import shutil
import stat
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gameside  # noqa: E402

BACKUP = gameside.AMD_BACKUP
T4A_MAN = {"release": "Legend of Kairi", "version": "0.1.8-alpha", "source_commit": "AMD test 4a (issue 2): exe=a6a68eb dll=a6a68eb"}
ORIG_MAN = {"release": "Legend of Kairi", "version": "0.1.8-alpha", "source_commit": "exe=34f4f3e dll=34f4f3e(NO_MELTY) pkg=34f4f3e"}
T2_LAUNCHERS = ["AMD test 2 - A (depth view).bat", "AMD test 2 - B (second view).bat", "AMD test 2 - C (guard on).bat",
                "AMD test 2 - D (depth copy on).bat"]
T3_LAUNCHERS = ["AMD test 3 - A (fix try 1).bat", "AMD test 3 - B (fix try 2).bat", "AMD test 3 - C (fix try 3).bat",
                "AMD test 3 - D (fix try 4).bat", "AMD test 3 - E (guard on).bat"]
T4A_LAUNCHERS = ["AMD test 4a - A (fix try 5).bat", "AMD test 4a - DBG (colour bands).bat", "AMD test 4a - E (guard on).bat"]
T4B_LAUNCHERS = ["AMD test 4b - F (own set per draw).bat", "AMD test 4b - F DBG (colour bands).bat"]


def w(p, t="x"):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8", newline="") as f:
        f.write(t if isinstance(t, str) else json.dumps(t))


def r(p):
    with open(p, encoding="utf-8") as f:
        return f.read()


def payload(d, launchers=(), man=T4A_MAN, orig=True):
    """an AMD test zip's files/ folder"""
    w(os.path.join(d, "bin", "savagewind.exe"), "TESTEXE")
    w(os.path.join(d, "bin", "sora_mod.dll"), "TESTDLL")
    w(os.path.join(d, "manifest.json"), man)
    for n in launchers:
        w(os.path.join(d, "launchers", n), "@echo off\r\n")
    if orig:
        w(os.path.join(d, "orig", "manifest.json"), ORIG_MAN)


def zip_folder(d, test):
    """an extracted "Legend of Kairi AMD test <n>" folder (the shape of sb/s231 - s239 out/)"""
    w(os.path.join(d, "READ ME.txt"), 'Legend of Kairi - AMD test %s (for the "Roxas / Sora is not drawn" problem)\r\n\r\nmore\r\n' % test)
    if test == "4b":
        for n in T4B_LAUNCHERS:
            w(os.path.join(d, n), "@echo off\r\n")
        return
    sfx = "" if test == "1" else " " + test
    w(os.path.join(d, "Install AMD test%s.bat" % sfx), "@echo off\r\n")
    w(os.path.join(d, "Undo AMD test%s.bat" % sfx), "@echo off\r\n")
    w(os.path.join(d, "amdtest%s.ps1" % ("" if test == "1" else test)), "# ps\r\n")
    payload(os.path.join(d, "files"), {"2": T2_LAUNCHERS, "3": T3_LAUNCHERS, "4a": T4A_LAUNCHERS}.get(test, ()), orig=test != "1",
            man=dict(T4A_MAN, source_commit="AMD test %s (issue 2): exe=1 dll=1" % test))


def make_zip(p, test, extra=None):
    """a test's download: "Legend-of-Kairi-0.1.8-AMD-test-<n>.zip" (one top folder, as sb/s231 - s239 out/)"""
    import zipfile
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with zipfile.ZipFile(p, "w") as z:
        z.writestr("Legend of Kairi AMD test %s/READ ME.txt" % test, "Legend of Kairi - AMD test %s\r\n" % test)
        z.writestr("Legend of Kairi AMD test %s/files/bin/savagewind.exe" % test, "TESTEXE")
        if extra:
            z.writestr(extra, "MINE")


def install_root(base, name="root"):
    """a normal 0.1.8 install folder (what must always stay)"""
    root = os.path.join(base, name)
    for p, t in (("Play Legend of Kairi.bat", "PLAY"), ("Make bug report.bat", "BUG"), ("make_bug_report.ps1", "PS"),
                 ("Update Legend of Kairi.bat", "UPD"), ("manifest.json", T4A_MAN), ("bin/savagewind.exe", "EXE"), ("bin/sora_mod.dll", "DLL"),
                 ("data/sora_model.bin", "M"), ("work/done/step_a", "d"), ("logs/last.log", "L"), ("logs/prev.log", "P"),
                 ("logs/amdtest4a run 1.log", "R1"), ("logs/amdtest4b run 2.log", "R2"), ("logs/amdtest2 run 1.log", "R0"),
                 ("save/slot0.sav", "S"), ("Legend of Kairi.lnk", "LNK")):
        w(os.path.join(root, p), t)
    return root


KEPT = ["Play Legend of Kairi.bat", "Make bug report.bat", "make_bug_report.ps1", "Update Legend of Kairi.bat", "manifest.json",
        "bin/savagewind.exe", "bin/sora_mod.dll", "data/sora_model.bin", "work/done/step_a", "logs/last.log", "logs/prev.log",
        "logs/amdtest4a run 1.log", "logs/amdtest4b run 2.log", "logs/amdtest2 run 1.log", "save/slot0.sav", "Legend of Kairi.lnk"]


def her_folder(base):
    """the GitHub player's folder as her 2026-11-16 bug report shows it (+ what the tests made at run time + older tests' leftovers)"""
    root = install_root(base)
    for n in T4A_LAUNCHERS + T4B_LAUNCHERS + ["AMD test 4b - play copy.bat", "Install AMD test 4a.bat", "Undo AMD test 4a.bat",
                                              "amdtest.ps1", "amdtest2.ps1", "amdtest3.ps1", "amdtest4a.ps1", "amdtest4b_live.txt",
                                              "AMD test 2 - C (guard on).bat", "AMD test 3 - E (guard on).bat", "Install AMD test.bat",
                                              "Undo AMD test 3.bat", "manifest.json.amdtest-new"]:
        w(os.path.join(root, n), "@echo off\r\n")
    w(os.path.join(root, "bin", "sora_mod.dll.amdtest-new"), "half")
    w(os.path.join(root, BACKUP, "bin", "savagewind.exe"), "OLDEXE")
    w(os.path.join(root, BACKUP, "bin", "sora_mod.dll"), "OLDDLL")
    w(os.path.join(root, BACKUP, "manifest.json"), ORIG_MAN)
    payload(os.path.join(root, "files"), T2_LAUNCHERS + T3_LAUNCHERS + T4A_LAUNCHERS)          # 2, 3, 4a extracted over each other
    w(os.path.join(root, "READ ME.txt"), 'Legend of Kairi - AMD test 4b  (for the "no character bodies on AMD" problem)\r\n')
    for t in ("1", "2", "3", "4a", "4b"):
        zip_folder(os.path.join(root, "Legend of Kairi AMD test %s" % t), t)
    # (s242) Windows "Extract All" inside the install folder: "<zip name>/<the zip's top folder>/", and the downloads saved there
    for t in ("1", "2", "3", "4a", "4b"):
        zip_folder(os.path.join(root, "Legend-of-Kairi-0.1.8-AMD-test-%s" % t, "Legend of Kairi AMD test %s" % t), t)
        make_zip(os.path.join(root, "Legend-of-Kairi-0.1.8-AMD-test-%s.zip" % t), t)
    os.chmod(os.path.join(root, "Undo AMD test 4a.bat"), stat.S_IREAD)                         # read-only: still removed
    os.chmod(os.path.join(root, BACKUP, "bin", "sora_mod.dll"), stat.S_IREAD)
    return root


def test_her_folder(base):
    root = her_folder(base)
    before = set(os.listdir(root))
    logs = []
    gone = gameside.remove_amd_tests(root, logs.append)
    left = set(os.listdir(root))
    assert left == {"Play Legend of Kairi.bat", "Make bug report.bat", "make_bug_report.ps1", "Update Legend of Kairi.bat", "manifest.json",
                    "bin", "data", "work", "logs", "save", "Legend of Kairi.lnk"}, sorted(left)
    assert sorted(os.listdir(os.path.join(root, "bin"))) == ["savagewind.exe", "sora_mod.dll"]
    for k in KEPT:
        assert os.path.isfile(os.path.join(root, k)), k
    assert r(os.path.join(root, "bin", "savagewind.exe")) == "EXE"
    assert len(gone) == len(before - left) + 1, (gone, before - left)          # + bin/sora_mod.dll.amdtest-new
    assert len(logs) == 1 and logs[0].startswith("[game] removed %d AMD test files (the fix is in this release): " % len(gone)), logs
    assert BACKUP in logs[0] and "files" in gone and "READ ME.txt" in gone
    assert "Legend-of-Kairi-0.1.8-AMD-test-4a" in gone and "Legend-of-Kairi-0.1.8-AMD-test-4b.zip" in gone, gone
    # twice in a row: nothing left to do, nothing logged
    logs = []
    assert gameside.remove_amd_tests(root, logs.append) == [] and logs == [], logs
    print("OK   her folder: %d AMD test items removed, the install + logs/ kept, a second run is a no-op" % len(gone))


def test_fresh(base):
    root = install_root(base, "fresh")
    logs = []
    assert gameside.remove_amd_tests(root, logs.append) == [] and logs == []
    for k in KEPT:
        assert os.path.isfile(os.path.join(root, k)), k
    empty = os.path.join(base, "empty")
    os.makedirs(empty)
    assert gameside.remove_amd_tests(empty, logs.append) == [] and logs == []
    assert gameside.remove_amd_tests(os.path.join(base, "no such folder"), logs.append) == [] and logs == []
    print("OK   fresh / empty / missing folder: nothing removed, nothing logged")


def test_players_own(base):
    """names like ours that are the player's: they stay"""
    root = install_root(base, "own")
    mine = ["AMD test notes.txt", "AMD test 4a - A (fix try 5) - Copy.bat", "amdtest5.ps1", "files/photos/a.png", "READ ME.txt",
            "Legend of Kairi AMD test 2/my notes.txt", "Legend of Kairi AMD test 3/READ ME.txt", "AMD test backup (mine)/x.txt"]
    for n in mine:
        w(os.path.join(root, n), "MINE")
    zip_folder(os.path.join(root, "Legend of Kairi AMD test 2"), "2")       # ours + the player's own file in it: kept whole
    logs = []
    assert gameside.remove_amd_tests(root, logs.append) == [], logs
    for n in mine:
        assert r(os.path.join(root, n)) == "MINE", n
    assert os.path.isfile(os.path.join(root, "Legend of Kairi AMD test 2", "Install AMD test 2.bat"))
    assert len(logs) == 1 and "kept" in logs[0] and "files" in logs[0], logs        # the one line about files/
    # a payload-like files/ with one extra file of the player's: kept
    shutil.rmtree(os.path.join(root, "files"))
    payload(os.path.join(root, "files"), T4A_LAUNCHERS)
    w(os.path.join(root, "files", "bin", "mine.txt"), "MINE")
    logs = []
    assert gameside.remove_amd_tests(root, logs.append) == [] and os.path.isfile(os.path.join(root, "files", "bin", "mine.txt")), logs
    # a files/ whose manifest is not an AMD test one (a normal release manifest): kept
    os.remove(os.path.join(root, "files", "bin", "mine.txt"))
    w(os.path.join(root, "files", "manifest.json"), ORIG_MAN)
    assert gameside.remove_amd_tests(root, logs.append) == [] and os.path.isdir(os.path.join(root, "files"))
    w(os.path.join(root, "files", "manifest.json"), "not json {")
    assert gameside.remove_amd_tests(root, logs.append) == [] and os.path.isdir(os.path.join(root, "files"))
    # ... and with the test manifest back it is ours
    w(os.path.join(root, "files", "manifest.json"), T4A_MAN)
    logs = []
    assert gameside.remove_amd_tests(root, logs.append) == ["files"], logs
    print("OK   the player's own look-alike files / folders stay")


def test_wrappers(base):
    """(s242) the "Extract All" folder and the saved zip: only when nothing of the player's is in them"""
    root = install_root(base, "wrap")
    W = "Legend-of-Kairi-0.1.8-AMD-test-%s"
    I = "Legend of Kairi AMD test %s"
    zip_folder(os.path.join(root, W % "4a", I % "4a"), "4a")                 # ours
    zip_folder(os.path.join(root, W % "3", I % "3"), "3")                    # ours + the player's file next to the inner folder: kept
    w(os.path.join(root, W % "3", "my notes.txt"), "MINE")
    zip_folder(os.path.join(root, W % "2", I % "2"), "2")                    # the player's file INSIDE the inner folder: kept
    w(os.path.join(root, W % "2", I % "2", "shot.png"), "MINE")
    zip_folder(os.path.join(root, W % "4b", I % "4a"), "4a")                 # another test's folder under this name: kept
    w(os.path.join(root, W % "1", "x.txt"), "MINE")                          # no inner folder at all: kept
    zip_folder(os.path.join(root, W % "4a" + " (1)", I % "4a"), "4a")        # not an exact name: kept
    make_zip(os.path.join(root, W % "4a" + ".zip"), "4a")                    # ours
    make_zip(os.path.join(root, W % "3" + ".zip"), "3", extra="my save/slot0.sav")      # something else inside: kept
    make_zip(os.path.join(root, W % "4b" + ".zip"), "4b", extra=I % "4b" + "/shot.png")  # the player's file under the top folder: kept
    w(os.path.join(root, W % "2" + ".zip"), "not a zip")                     # kept
    os.makedirs(os.path.join(root, W % "1" + ".zip"))                        # a folder with a zip's name: kept
    logs = []
    gone = gameside.remove_amd_tests(root, logs.append)
    assert sorted(gone) == [W % "4a", W % "4a" + ".zip"], gone
    assert r(os.path.join(root, W % "3", "my notes.txt")) == "MINE" and os.path.isfile(os.path.join(root, W % "3", I % "3", "READ ME.txt"))
    assert r(os.path.join(root, W % "2", I % "2", "shot.png")) == "MINE"
    assert os.path.isdir(os.path.join(root, W % "4b", I % "4a")) and r(os.path.join(root, W % "1", "x.txt")) == "MINE"
    assert os.path.isdir(os.path.join(root, W % "4a" + " (1)", I % "4a"))
    assert os.path.isfile(os.path.join(root, W % "3" + ".zip")) and r(os.path.join(root, W % "2" + ".zip")) == "not a zip"
    assert os.path.isfile(os.path.join(root, W % "4b" + ".zip"))
    assert os.path.isdir(os.path.join(root, W % "1" + ".zip"))
    for k in KEPT:
        assert os.path.isfile(os.path.join(root, k)), k
    if sys.platform == "win32":                                              # a wrapper that is a junction: left, never followed
        out = os.path.join(base, "wout")
        zip_folder(os.path.join(out, I % "4a"), "4a")
        if junction(os.path.join(root, W % "4a"), out):
            assert gameside.remove_amd_tests(root, logs.append) == [] and os.path.isfile(os.path.join(out, I % "4a", "READ ME.txt"))
    print("OK   the Extract All folder + the saved zip go; anything of the player's in them keeps them")


def test_rm_carries_on(base):
    """(s242) a backup whose first file cannot be deleted still loses the rest (so Undo finds it damaged, not intact)"""
    if sys.platform != "win32":
        print("skip carry-on test (Windows only)")
        return
    root = her_folder(os.path.join(base, "carry"))
    held = os.path.join(root, BACKUP, "bin", "savagewind.exe")
    logs = []
    with open(held, "rb"):
        gone = gameside.remove_amd_tests(root, logs.append)
    assert BACKUP not in gone and os.path.isfile(held), gone
    assert not os.path.lexists(os.path.join(root, BACKUP, "bin", "sora_mod.dll")), "the DLL of the backup goes although the exe is held"
    assert not os.path.lexists(os.path.join(root, BACKUP, "manifest.json"))
    assert len([l for l in logs if "could not remove" in l and BACKUP in l]) == 1, logs
    assert gameside.remove_amd_tests(root, logs.append) == [BACKUP]
    # ... but only in the backup: a held file in an extracted test folder stops there, the folder still looks ours and goes next time
    root = install_root(os.path.join(base, "carry2"))
    W, I = "Legend-of-Kairi-0.1.8-AMD-test-4a", "Legend of Kairi AMD test 4a"
    zip_folder(os.path.join(root, W, I), "4a")
    payload(os.path.join(root, "files"), T4A_LAUNCHERS)
    held = [os.path.join(root, W, I, "files", "bin", "savagewind.exe"), os.path.join(root, "files", "bin", "savagewind.exe")]
    logs = []
    with open(held[0], "rb"), open(held[1], "rb"):
        assert gameside.remove_amd_tests(root, logs.append) == [], logs
    assert len([l for l in logs if "could not remove" in l]) == 2 and not any("kept the folder" in l for l in logs), logs
    logs = []
    assert sorted(gameside.remove_amd_tests(root, logs.append)) == [W, "files"] and len(logs) == 1, logs
    print("OK   a held file: the backup still loses the rest; a test folder is left whole enough to go on the next run")


def junction(link, target):
    rc = subprocess.run(["cmd", "/c", "mklink", "/J", link, target], capture_output=True)
    return rc.returncode == 0 and os.path.isdir(link)


def test_junctions(base):
    if sys.platform != "win32":
        print("skip junction test (Windows only)")
        return
    root = install_root(base, "jroot")
    outside = os.path.join(base, "outside")
    w(os.path.join(outside, "bin", "savagewind.exe"), "PRECIOUS")
    w(os.path.join(outside, "manifest.json"), "PRECIOUS")
    if not junction(os.path.join(root, BACKUP), outside):
        print("skip junction test (mklink /J failed)")
        return
    # a junction INSIDE a real extracted test folder + one named like a launcher's folder payload
    out2 = os.path.join(base, "outside2")
    payload(out2, T4A_LAUNCHERS)
    assert junction(os.path.join(root, "files"), out2)
    zip_folder(os.path.join(root, "Legend of Kairi AMD test 4a"), "4a")
    out3 = os.path.join(base, "outside3")
    w(os.path.join(out3, "keep.txt"), "PRECIOUS")
    shutil.rmtree(os.path.join(root, "Legend of Kairi AMD test 4a", "files", "orig"))
    assert junction(os.path.join(root, "Legend of Kairi AMD test 4a", "files", "orig"), out3)
    logs = []
    gone = gameside.remove_amd_tests(root, logs.append)
    assert not os.path.lexists(os.path.join(root, BACKUP)), "the backup link itself goes"
    assert r(os.path.join(outside, "bin", "savagewind.exe")) == "PRECIOUS" and r(os.path.join(outside, "manifest.json")) == "PRECIOUS"
    assert os.path.lexists(os.path.join(root, "files")) and os.path.isfile(os.path.join(out2, "bin", "savagewind.exe")), "a files/ link is left"
    assert os.path.isdir(os.path.join(root, "Legend of Kairi AMD test 4a")), "a test folder with a link inside is left"
    assert r(os.path.join(out3, "keep.txt")) == "PRECIOUS"
    assert gone == [BACKUP], gone
    print("OK   junctions: the backup link is removed itself, never followed; folders holding links are left")


def test_in_use(base):
    """a file that cannot be removed (open in another program): one plain line, the rest goes, never raises"""
    root = her_folder(os.path.join(base, "busy"))
    held = os.path.join(root, "amdtest4b_live.txt")
    logs = []
    with open(held, "rb"):        # CPython opens without FILE_SHARE_DELETE: Windows refuses the delete while it is open
        gone = gameside.remove_amd_tests(root, logs.append)
    if sys.platform == "win32":
        assert os.path.isfile(held) and "amdtest4b_live.txt" not in gone, gone
        bad = [l for l in logs if "could not remove" in l]
        assert len(bad) == 1 and "amdtest4b_live.txt" in bad[0] and len(bad[0].splitlines()) == 1, logs
    assert not os.path.lexists(os.path.join(root, BACKUP)) and not os.path.lexists(os.path.join(root, "Undo AMD test 4a.bat"))
    logs = []
    assert gameside.remove_amd_tests(root, logs.append) == (["amdtest4b_live.txt"] if sys.platform == "win32" else []), logs
    # an exception from deep inside never leaves the function
    old = gameside._amd_rm
    gameside._amd_rm = lambda p: 1 / 0
    try:
        w(held, "x")
        logs = []
        assert gameside.remove_amd_tests(root, logs.append) == [] and any("could not remove" in l for l in logs), logs
    finally:
        gameside._amd_rm = old
    print("OK   a file in use is logged in one line and never fails the install")


def test_install_game_side(base):
    """install_game_side runs it and returns what it removed"""
    rel = her_folder(os.path.join(base, "igs"))
    sw = os.path.join(base, "igs", "sw")
    os.makedirs(sw)
    old = gameside.POWERSHELL
    gameside.POWERSHELL = os.path.join(base, "no", "powershell.exe")         # (no shortcuts here: that step only logs)
    try:
        logs = []
        res = gameside.install_game_side(rel, sw, logs.append, release=rel)
    finally:
        gameside.POWERSHELL = old
    assert BACKUP in res["amd_removed"] and not os.path.lexists(os.path.join(rel, BACKUP)), res
    assert any(l.startswith("[game] removed %d AMD test files" % len(res["amd_removed"])) for l in logs), logs
    assert os.path.isfile(res["solo"])
    print("OK   install_game_side removes them (result key amd_removed)")


def main():
    base = tempfile.mkdtemp(prefix="loktest_amd_", dir=os.environ.get("LOK_TEST_TMP"))
    try:
        for t in (test_her_folder, test_fresh, test_players_own, test_wrappers, test_rm_carries_on, test_junctions, test_in_use,
                  test_install_game_side):
            d = os.path.join(base, t.__name__)
            os.makedirs(d)
            t(d)
    finally:
        if "--keep" not in sys.argv:
            def unro(f, p, e):
                os.chmod(p, stat.S_IWRITE)
                f(p)
            shutil.rmtree(base, onerror=unro)
    print("test_amdclean: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
