"""(s223) Savage Wind's own files for Legend of Kairi's GitHub release: the version check, the upgrade in place to the
version this release is built for ("U1": the same folder, the same game files copy, saves and settings kept; s254: from
0.5.3 = the Legend of Kairi 0.1.5 - 0.1.10 players, and from 0.4.1 = 0.1.4's) and stock Savage Wind's no-click setup.

MIT licence, like the rest of tools/installer. Python stdlib only (the kit's python runs it; melty-setup/build.py may use it).

Folder kinds (sw_kind):
  "0.6.0"      sw-manifest.txt says version 0.6.0-alpha.6 (READY: the version Legend of Kairi is built for)
  "0.5.3"      sw-manifest.txt says version 0.5.3-alpha.5 (Legend of Kairi 0.1.5 - 0.1.10's version: upgraded in place)
  "0.4.1"      README.md names SavageWind-0.4.1-alpha.4 and there is no sw-manifest.txt (0.1.4's version: upgraded in place;
               s254: straight to 0.6.0 with the same steps, NOT run on a real 0.4.1 folder since s223's 0.4.1 -> 0.5.3)
  "upgrading"  our journal .kairi-sw-upgrade.json is there: an upgrade that was cut off (it carries on)
  "other:<v>"  another Savage Wind version: refused, nothing is changed
  "unknown"    not a Savage Wind folder we can name: refused, nothing is changed

The upgrade: the stock zip (pinned size + sha256) -> backup savagewind.ini + save/ once -> journal (it keeps the old
version's file list) -> every file of the zip next to its target as <name>.kairi-new, then os.replace (sw-manifest.txt last:
the folder says the new version only once all of its files are in) -> the old version's program files that the new one no
longer has are removed (s254: 0.5.3's 43 swgen/_internal files) -> stock's own setup keeps the game files copy and builds
the new game code + shaders -> the game files state check -> journal removed.
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import zipfile

SW_WANT = "0.6.0-alpha.6"       # (s254) the Savage Wind version this release is built for
SW_PREV = "0.5.3-alpha.5"       # Legend of Kairi 0.1.5 - 0.1.10's version
SW_OLD = "0.4.1-alpha.4"        # Legend of Kairi 0.1.4's version
READY = "0.6.0"                 # sw_kind of a folder that has SW_WANT
UPGRADE_FROM = {"0.5.3": SW_PREV, "0.4.1": SW_OLD}      # the kinds this installer brings up to SW_WANT in place
# (s254) size + sha256 = GitHub's own digest of the release asset (gh release view v0.6.0-alpha.6) = the copy in sb/s244/dl
STOCK = {"name": "SavageWind-0.6.0-alpha.6-windows.zip", "size": 125936779,
         "sha256": "9bfc05217ed7e69001e02dee7ae9a6070a23b68cfbf946f23727f0767d4c28f8",
         "url": "https://github.com/Revivedx/SavageWindRecomp/releases/download/v0.6.0-alpha.6/SavageWind-0.6.0-alpha.6-windows.zip"}
# (s254) the game code ids stock SW_WANT's setup builds (extracted/swgame.dll.id: the first on a CPU with AVX2 + BMI2, the
# second = GAMELIB_ID_COMPAT). A release exe has to carry them: mkzip stops when it doesn't (an exe from a tree whose game
# code sources moved would fit no player's game code).
GAME_IDS = ("f63df6b1a6001541", "6b368760feeef2a0")
TOP = "Savage Wind/"            # the zip's one top folder
GAMELIB = "swgame.dll"          # the game code stock's setup builds into extracted/
JOURNAL = ".kairi-sw-upgrade.json"
MANIFEST = "sw-manifest.txt"
NEW = ".kairi-new"
SETUP_INI = ".kairi-setup.ini"  # (s255) the settings file stock's setup reads for its one run (setup_ini)
# (s255) the only places remove_stale deletes in: every file 0.5.3 has and 0.6.0 lacks is under swgen/ (43, all swgen/_internal)
STALE_ROOTS = ("swgen/",)
SETUP_MINUTES = 90              # stock's setup: code ~10 min, shaders more on a slow PC (melty-setup/build.py SETUP_MINUTES)
POLL_S = 0.5


class Fail(Exception):
    """a plain message for the player"""


def read_text(path, limit=4096):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read(limit)
    except OSError:
        return None


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""):
            h.update(b)
    return h.hexdigest()


# ------------------------------------------------------------------------------------------------ what folder is it?
def manifest_version(sw):
    """'0.6.0-alpha.6' from sw-manifest.txt line 2 ('version 0.6.0-alpha.6'); '' when there is none"""
    for line in (read_text(os.path.join(sw, MANIFEST), 1024) or "").splitlines()[:4]:
        if line.startswith("version "):
            return line[8:].strip()
    return ""


def readme_version(sw):
    """0.4.x releases name their zip in README.md ('SavageWind-0.4.1-alpha.4-windows.zip'); '' when it does not"""
    m = re.search(r"SavageWind-(\d+\.\d+\.\d+(?:-[0-9A-Za-z.]+?)?)-(?:windows|linux|macos)", read_text(os.path.join(sw, "README.md"), 1 << 20) or "")
    return m.group(1) if m else ""


def sw_kind(sw):
    v = manifest_version(sw)
    # (s226 review G1) our journal counts only while the folder is still ours to finish: no sw-manifest.txt yet (0.4.1 has
    # none, and it is unpacked last), the old version's (s254: an upgrade from 0.5.3 that was cut off mid-unpack, or an older
    # release's 0.4.1 -> 0.5.3 upgrade that never finished) or the new one's. A journal left behind in a folder that Savage
    # Wind has since updated to something else must not get our zip unpacked over it: that folder is "other" (refused,
    # nothing changed).
    if os.path.isfile(os.path.join(sw, JOURNAL)) and (not v or v in (SW_WANT, SW_PREV)):
        return "upgrading"
    if v:
        return READY if v == SW_WANT else "0.5.3" if v == SW_PREV else "other:" + v
    v = readme_version(sw)
    if v == SW_OLD:
        return "0.4.1"
    return "other:" + v if v else "unknown"


def manifest_names(sw):
    """the file names of sw-manifest.txt's 'f <sha256> <size> <name>' lines ([] when there is none)"""
    out = []
    for line in (read_text(os.path.join(sw, MANIFEST), 1 << 20) or "").splitlines():
        parts = line.split(" ", 3)
        if line.startswith("f ") and len(parts) == 4:
            out.append(parts[3])
    return out


def inside(base):
    """realpath(base) with ONE separator at its end, for startswith checks (s255, review finding 7: a folder that is a drive
    root already ends with one)"""
    return os.path.realpath(base).rstrip(os.sep) + os.sep


def journal_from(sw, kind, j):
    """the version an upgrade of this folder starts / started from: SW_PREV or SW_OLD and nothing else (s255, review finding
    1: the answer names the backup folder, so a journal's own text never goes into a path). The kind says it; a cut-off
    upgrade's journal does; with a journal that can't be read or says something else, a manifest still there is the old
    version's (0.5.3), none = 0.4.1."""
    frm = UPGRADE_FROM.get(kind) or (j.get("from") if isinstance(j, dict) else None)
    if frm not in (SW_PREV, SW_OLD):
        frm = SW_PREV if manifest_version(sw) == SW_PREV else SW_OLD
    return frm


def read_journal(sw):
    """the upgrade journal of this folder as a dict ({} = none, or not readable)"""
    try:
        with open(os.path.join(sw, JOURNAL), encoding="utf-8") as f:
            j = json.load(f)
    except (OSError, ValueError):
        return {}
    return j if isinstance(j, dict) else {}


def remove_stale(sw, old_names, log=print):
    """(s254) the old version's program files that the new sw-manifest.txt no longer lists (a fresh folder of the new version
    lacks them too). Only names the OLD manifest listed, only under STALE_ROOTS (s255, review finding 2: the list comes from
    the journal, a file in the player's folder; saves, settings, extracted/ and cache/ can never be named by it), only files
    inside sw, never a file the new version has; folders left empty by it go too. -> how many were removed"""
    new = {n.lower() for n in manifest_names(sw)}
    if not new:
        return 0
    base = os.path.realpath(sw)
    n = 0
    for name in old_names:
        parts = safe_name(TOP + name) if isinstance(name, str) else None
        if parts is None or name.lower() in new or name.lower() == MANIFEST or not name.lower().startswith(STALE_ROOTS):
            continue
        p = os.path.join(sw, *parts)
        if not os.path.realpath(p).startswith(inside(sw)) or not os.path.isfile(p):
            continue
        try:
            os.remove(p)
            n += 1
        except OSError:
            continue
        d = os.path.dirname(p)
        while os.path.realpath(d) != base:
            try:
                os.rmdir(d)        # only when empty
            except OSError:
                break
            d = os.path.dirname(d)
    if n:
        log("[sw] removed %d files of the old Savage Wind version that %s no longer has" % (n, SW_WANT))
    return n


def program_files_ok(sw):
    """-> '' when every 'f <sha256> <size> <name>' line of sw-manifest.txt matches the file, else the first that doesn't"""
    txt = read_text(os.path.join(sw, MANIFEST), 1 << 20)
    if not txt:
        return MANIFEST
    n = 0
    for line in txt.splitlines():
        if not line.startswith("f "):
            continue
        parts = line.split(" ", 3)
        if len(parts) != 4:
            return line[:80]
        _, sha, size, name = parts
        p = os.path.join(sw, *name.split("/"))
        try:
            if os.path.getsize(p) != int(size) or sha256_file(p) != sha:
                return name
        except (OSError, ValueError):
            return name
        n += 1
    return "" if n else MANIFEST


# ------------------------------------------------------------------------------------------------ the stock zip
class _HttpsOnly(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urllib.parse.urlparse(newurl).scheme != "https":
            raise Fail("The Savage Wind download was sent to a non-https address. Try again later.")
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def zip_ok(path):
    try:
        return os.path.getsize(path) == STOCK["size"] and sha256_file(path) == STOCK["sha256"]
    except OSError:
        return False


def fetch_stock(dest_dir, local=None, log=print, opener=None):
    """-> the path of the checked stock zip (STOCK). local = a copy on this PC (tests / dev; checked the same way, never copied).
    Else <dest_dir>/<name> when an earlier run left it complete, else a download (https only, size cap, sha256 pin) into
    <name>.part, renamed when it checks out."""
    if local:
        if not zip_ok(local):
            raise Fail("The Savage Wind %s zip at %s is not the official one (size or checksum differ)." % (SW_WANT, local))
        log("[sw] using %s" % local)
        return local
    os.makedirs(dest_dir, exist_ok=True)
    final = os.path.join(dest_dir, STOCK["name"])
    if zip_ok(final):
        log("[sw] %s already downloaded" % STOCK["name"])
        return final
    part = final + ".part"
    log("[sw] downloading %s (%.0f MB)" % (STOCK["url"], STOCK["size"] / 1e6))
    op = opener or urllib.request.build_opener(_HttpsOnly)
    h = hashlib.sha256()
    got = 0
    try:
        with op.open(urllib.request.Request(STOCK["url"], headers={"User-Agent": "LegendOfKairi-installer"}), timeout=60) as r, \
                open(part, "wb") as f:
            while True:
                b = r.read(1 << 20)
                if not b:
                    break
                got += len(b)
                if got > STOCK["size"]:
                    raise Fail("The Savage Wind download is bigger than the official zip. Try again later.")
                h.update(b)
                f.write(b)
    except Fail:
        raise
    except OSError as e:
        raise Fail("Could not download Savage Wind %s (%s). Check your internet connection and press the button again." % (SW_WANT, e))
    if got != STOCK["size"] or h.hexdigest() != STOCK["sha256"]:
        try:
            os.remove(part)
        except OSError:
            pass
        raise Fail("The Savage Wind download did not check out (it was cut off or changed). Press the button again.")
    os.replace(part, final)
    return final


def safe_name(name):
    """a zip member under 'Savage Wind/' -> its relative path parts, or None (dirs, '..', absolute, drive letters, odd chars)"""
    if not name.startswith(TOP) or name.endswith("/") or "\\" in name or ":" in name:
        return None
    parts = name[len(TOP):].split("/")
    if not parts or any(p in ("", ".", "..") or p != p.strip() or p.endswith(".") for p in parts):
        return None
    if any(p.lower().endswith(NEW) for p in parts):
        return None
    return parts


def unpack_stock(zpath, sw, log=print):
    """every file of the zip into sw, each through <file>.kairi-new + os.replace; sw-manifest.txt last.
    Files of the folder that the zip doesn't have (saves, settings, extracted/, cache/) are never touched."""
    with zipfile.ZipFile(zpath) as z:
        items = []
        for zi in z.infolist():
            if zi.is_dir():
                continue
            parts = safe_name(zi.filename)
            if parts is None:
                raise Fail("The Savage Wind zip has an odd file name (%s): nothing more was unpacked." % zi.filename)
            items.append((parts, zi))
        if not any(p == [MANIFEST] for p, _ in items):
            raise Fail("The Savage Wind zip has no %s." % MANIFEST)
        items.sort(key=lambda it: it[0] == [MANIFEST])      # the manifest last
        base = inside(sw)
        for parts, zi in items:
            dst = os.path.join(sw, *parts)
            if not os.path.realpath(dst).startswith(base):
                raise Fail("The Savage Wind zip points outside its folder (%s)." % zi.filename)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            tmp = dst + NEW
            with z.open(zi) as src, open(tmp, "wb") as f:
                shutil.copyfileobj(src, f, 1 << 20)
            try:
                os.replace(tmp, dst)
            except PermissionError:
                try:
                    os.remove(tmp)
                except OSError:
                    pass
                raise Fail("Windows would not let the installer replace %s (a program from that folder is running?). Close "
                           "Savage Wind and press the button again." % os.path.join(sw, *parts))
    log("[sw] unpacked %d files of Savage Wind %s" % (len(items), SW_WANT))
    return len(items)


# ------------------------------------------------------------------------------------------------ the old version's settings + saves
def backup_once(sw, dest, log=print):
    """savagewind.ini + save/ -> dest (once: a retry never overwrites the first, pre-upgrade copy). -> dest"""
    if os.path.isfile(os.path.join(dest, "done.txt")):
        return dest
    tmp = dest + ".tmp"
    try:
        shutil.rmtree(tmp, ignore_errors=True)
        os.makedirs(tmp)
        if os.path.isfile(os.path.join(sw, "savagewind.ini")):
            shutil.copy2(os.path.join(sw, "savagewind.ini"), tmp)
        if os.path.isdir(os.path.join(sw, "save")):
            shutil.copytree(os.path.join(sw, "save"), os.path.join(tmp, "save"))
        with open(os.path.join(tmp, "done.txt"), "w", encoding="utf-8") as f:
            f.write("Savage Wind's settings and saves before the upgrade to %s (%s)\n" % (SW_WANT, time.strftime("%Y-%m-%d %H:%M")))
        shutil.rmtree(dest, ignore_errors=True)
        os.replace(tmp, dest)
    except (OSError, shutil.Error) as e:        # (s255, review finding 5) a plain message, not Python's error list
        if isinstance(e, shutil.Error) and e.args and isinstance(e.args[0], list) and e.args[0]:
            e = e.args[0][0][-1]                # copytree: [(from, to, why), ...] -> the first why
        raise Fail("Could not save a copy of your saves and settings before the upgrade (%s). Nothing was changed in your "
                   "Savage Wind folder. Free some disk space or close programs that use those files, then press the button "
                   "again. The copy goes to %s" % (e, dest))
    log("[sw] saved a copy of the settings and saves in %s" % dest)
    return dest


# ------------------------------------------------------------------------------------------------ stock's setup
def exe_has_id(exe, gid):
    """does this program carry that game library id (its GAMELIB_ID / GAMELIB_ID_COMPAT string, runtime/gamelib.c)?"""
    with open(exe, "rb") as f:
        return re.search(rb"(?<![0-9a-f])" + gid.encode("ascii") + rb"\x00", f.read()) is not None


def game_id(sw):
    """the id of the game code in sw/extracted ('' when there is none)"""
    gid = (read_text(os.path.join(sw, "extracted", GAMELIB + ".id"), 128) or "").strip()
    return gid if re.fullmatch(r"[0-9a-f]{16}", gid) else ""


def game_files_state(sw, our_exe):
    """-> '' when the game files in sw are set up for our_exe (Legend of Kairi's savagewind.exe) and stock SW_WANT, else what
    is missing (plain words). As melty-setup/build.py game_files_state with the data folder = the Savage Wind folder."""
    ext = os.path.join(sw, "extracted")
    if not os.path.isfile(os.path.join(ext, "exefs", "main")):
        return "not set up yet"
    if not os.path.isfile(os.path.join(ext, "romfs", "Pack", "Bootup.pack")):
        return "the copy of the game files is incomplete"
    gid = game_id(sw)
    if not os.path.isfile(os.path.join(ext, GAMELIB)) or not gid:
        return "the game code is not built"
    if not os.path.isfile(our_exe):
        raise Fail("Legend of Kairi's download is incomplete (bin\\savagewind.exe is missing). Download it again.")
    if not exe_has_id(our_exe, gid) or not exe_has_id(os.path.join(sw, "savagewind.exe"), gid):
        return "the game code is another Savage Wind version's (%s)" % gid
    ver = manifest_version(sw)
    if ver and (read_text(os.path.join(sw, "cache", "last-version.txt"), 128) or "").strip() != ver:
        return "Savage Wind %s's setup has not finished" % ver
    return ""


def setup_ini(sw):
    """(s255, the user's answer) stock's setup downloads NVIDIA's DLSS library (59 MB) into the Savage Wind folder on a PC
    with an NVIDIA driver unless its settings say `dlss_download = 0` (stock launcher.mm: after the game files check;
    settings.c: the last line of a key counts). So the setup run reads a copy of the player's savagewind.ini with that one
    line added, through SAVAGEWIND_SETTINGS; the hidden setup never writes its settings file, and the player's own
    savagewind.ini is not changed. -> the file's name (it lies in sw, the setup's starting folder, so no path has to pass
    through the environment)."""
    try:
        with open(os.path.join(sw, "savagewind.ini"), "rb") as f:
            b = f.read()
    except OSError:
        b = b""
    if b and not b.endswith(b"\n"):
        b += b"\r\n"
    with open(os.path.join(sw, SETUP_INI), "wb") as f:
        f.write(b + b"dlss_download = 0\r\n")
    return SETUP_INI


def run_stock_setup(sw, out, log=print, on_proc=None, alive=None, minutes=SETUP_MINUTES, exe=None):
    """ONE run of stock savagewind.exe's no-click setup that keeps the game files copy in sw/extracted
    (SAVAGEWIND_LAUNCHER_EXTRACT="": extract_thread's "copied already" branch builds the game code + shaders + the version
    mark). Its log goes to out. on_proc(p): the caller ties it to its job. alive(): False = stop now.
    -> ("ready" | "refused" | "failed" | "stopped", why). refused = NOT READY before building any game code."""
    exe = exe or os.path.join(sw, "savagewind.exe")
    env = {k: v for k, v in os.environ.items() if not k.upper().startswith(("SAVAGEWIND_", "SORA_"))}
    env.update(SAVAGEWIND_DATA_ROOT=sw, SAVAGEWIND_LAUNCHER_EXTRACT="", SAVAGEWIND_NO_UPDATE_CHECK="1", SAVAGEWIND_HIDDEN="1",
               SAVAGEWIND_SETTINGS=setup_ini(sw))
    kw = {}
    if sys.platform == "win32":
        kw["creationflags"] = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "wb") as f:
        p = subprocess.Popen([exe, "--no-launch-flags"], cwd=sw, env=env, stdout=f, stderr=subprocess.STDOUT,
                             stdin=subprocess.DEVNULL, **kw)
    if on_proc:
        on_proc(p)
    t0, seen, text, stopped = time.time(), 0, "", False
    try:
        while True:
            ended = p.poll() is not None
            text = read_text(out, 64 << 20) or ""
            lines = text.split("\n")
            if not ended:
                lines = lines[:-1]
            for line in lines[seen:]:
                line = line.rstrip("\r")
                if line.startswith(("[launcher]", "[gamebuild]", "[update]")) or (
                        line.startswith("[precompile]") and "translation failed" not in line):
                    log("[sw] " + line)
            seen = max(seen, len(lines))
            if ended:
                break
            if alive is not None and not alive():
                stopped = True
                break
            if time.time() - t0 > minutes * 60:
                raise Fail("Savage Wind's setup took more than %d minutes. Press the button again to go on with it. Its log: %s"
                           % (minutes, out))
            time.sleep(POLL_S)
    finally:
        if p.poll() is None:
            p.terminate()
            try:
                p.wait(15)
            except subprocess.TimeoutExpired:
                p.kill()
        try:
            os.remove(os.path.join(sw, SETUP_INI))
        except OSError:
            pass
    if stopped:
        return "stopped", ""
    res = re.findall(r"^\[launcher\] setup: (.*) -> (READY|NOT READY)\r?$", text, re.M)
    failed = [m.split(" | ")[0] for m in re.findall(r"^\[launcher\] FAILED: (.*?)\r?$", text, re.M)]
    if res and res[-1][1] == "READY":
        log("[sw] Savage Wind's setup finished in %.0f s" % (time.time() - t0))
        return "ready", ""
    why = failed[-1] if failed else (res[-1][0] if res else "it ended early (exit %s)" % p.returncode)
    if res and "[gamebuild]" not in text and "disk space" not in why.lower():
        return "refused", why
    return "failed", why


# ------------------------------------------------------------------------------------------------ the whole step
def kind_message(kind):
    """the plain refusal for a folder Legend of Kairi can't use ('' for the kinds it can)"""
    if kind in (READY, "upgrading") or kind in UPGRADE_FROM:
        return ""
    found = "Savage Wind %s" % kind[6:] if kind.startswith("other:") else "a Savage Wind version it can't tell"
    coop = "(Playing co-op? Stay on Legend of Kairi 0.1.4 with Savage Wind %s.)" % SW_OLD
    v = kind[6:] if kind.startswith("other:") else ""
    # (s226, s254) an OLDER 0.5.x / 0.6.0 alpha that none of our releases ran on (0.5.0 - 0.5.2, 0.6.0-alpha.1 - 5): upstream's
    # own way up is the new zip over the folder (its release notes). Savage Wind Configuration's Update is not named any
    # more: it goes to upstream's NEWEST version, which may be past the one this release is built for. We don't upgrade
    # these ourselves (only the folders our own releases ran on).
    m = re.fullmatch(r"0[.]6[.]0-alpha[.](\d+)", v)
    if re.match(r"0[.]5[.]\d+(-|$)", v) or (m and int(m.group(1)) < int(SW_WANT.rsplit(".", 1)[1])):
        return ("This folder has %s. This Legend of Kairi needs Savage Wind %s. Download Savage Wind %s and extract its zip over "
                "that folder (your game files, saves and settings stay), start Savage Wind once and let it finish its setup (if "
                "it offers a newer version, say no), then run this installer again. Nothing was changed. %s"
                % (found, SW_WANT, SW_WANT, coop))
    return ("This folder has %s. This Legend of Kairi works with Savage Wind %s (it can also bring a %s or %s folder up to %s "
            "for you). Download Savage Wind %s, or choose another folder. Nothing was changed. %s"
            % (found, SW_WANT, SW_PREV, SW_OLD, SW_WANT, SW_WANT, coop))


def busy(sw):
    """'' unless a program from the Savage Wind folder runs (the stock game, its configuration tool, its updater)"""
    try:
        import update
        r = update.running_from(sw)
    except Exception:
        r = []
    return ("Savage Wind is running from that folder (%s). Close it first, then press the button again." % os.path.basename(r[0])) if r else ""


def ensure_ready(sw, our_exe, work, log=print, local_zip=None, on_proc=None, alive=None, busy_check=busy, run_setup=run_stock_setup):
    """the Savage Wind folder ready for Legend of Kairi: an upgrade from 0.5.3 / 0.4.1 when needed, stock's setup when its game code
    isn't SW_WANT's. work = <Legend of Kairi>/work (download + backup + setup log under work/sw-upgrade). Raises Fail.
    -> "ready" (nothing to do) | "upgraded" | "set up" | "stopped" """
    kind = sw_kind(sw)
    msg = kind_message(kind)
    if msg:
        raise Fail(msg)
    up = os.path.join(work, "sw-upgrade")
    did = "ready"
    if kind == "upgrading" or kind in UPGRADE_FROM:
        b = busy_check(sw)
        if b:
            raise Fail(b)
        z = fetch_stock(up, local_zip, log)          # before the first write into the folder: a failed download changes nothing
        if alive is not None and not alive():
            return "stopped"
        jp = os.path.join(sw, JOURNAL)
        j = read_journal(sw)
        # (s254) what the folder had (journal_from; only the backup folder's name hangs on it)
        frm = journal_from(sw, kind, j)
        backup_once(sw, os.path.join(up, "backup-" + frm), log)
        fresh = write = not j
        if fresh:
            # old_files: read now, while the old sw-manifest.txt is still there (it is replaced last; a retry no longer has it)
            j = {"from": frm, "to": SW_WANT, "started": time.strftime("%Y-%m-%d %H:%M:%S"),
                 "old_files": manifest_names(sw) if manifest_version(sw) != SW_WANT else []}
        elif not isinstance(j.get("old_files"), list) and manifest_version(sw) == SW_PREV:
            # (s255, review finding 3) a journal of Legend of Kairi 0.1.5 - 0.1.10 (their 0.4.1 -> 0.5.3 upgrade, cut off after
            # its unpack) has no list: the 0.5.3 manifest that is still here gives it, and the journal keeps it for a retry
            j["old_files"], j["to"], write = manifest_names(sw), SW_WANT, True
        if write:
            with open(jp + ".tmp", "w", encoding="utf-8") as f:
                json.dump(j, f)
            os.replace(jp + ".tmp", jp)
        if fresh:
            log("[sw] bringing Savage Wind %s up to %s in place (your saves and settings stay)" % (frm, SW_WANT))
        else:
            log("[sw] carrying on with the upgrade to %s" % SW_WANT)
        unpack_stock(z, sw, log)
        bad = program_files_ok(sw)
        if bad:
            raise Fail("Savage Wind %s's files did not come out right (%s). Press the button again." % (SW_WANT, bad))
        old = j.get("old_files")
        remove_stale(sw, old if isinstance(old, list) else [], log)
        did = "upgraded"
    elif not os.path.isfile(os.path.join(sw, "extracted", "exefs", "main")):
        raise Fail("Savage Wind %s is here, but its game files are not set up yet. Start Savage Wind once, choose your game "
                   "files folder and let it finish, then come back." % SW_WANT)
    why = game_files_state(sw, our_exe)
    if why:
        b = busy_check(sw)
        if b:
            raise Fail(b)
        out = os.path.join(up, "stock-setup.log")
        log("[sw] game files: %s; Savage Wind's own setup builds the %s game code (keeps the game files copy; about 10-20 "
            "minutes)" % (why, SW_WANT))
        res, reason = run_setup(sw, out, log, on_proc, alive)
        if res == "stopped":
            return "stopped"
        if res == "refused":
            raise Fail("Savage Wind %s turned down the game files in that folder (%s). Start savagewind.exe in your Savage Wind "
                       "folder once, choose your game files folder and let it finish, then press the button again. Your saves "
                       "and settings are kept." % (SW_WANT, reason))
        if res != "ready":
            raise Fail("Savage Wind's setup did not finish (%s). Press the button again to go on with it. Its log: %s" % (reason, out))
        why = game_files_state(sw, our_exe)
        if why:
            raise Fail("Savage Wind's setup ended, but %s: it doesn't fit this Legend of Kairi release. Its log: %s" % (why, out))
        did = "set up" if did == "ready" else did
    try:
        os.remove(os.path.join(sw, JOURNAL))
    except FileNotFoundError:
        pass
    log("[sw] Savage Wind %s is ready" % SW_WANT)
    return did
