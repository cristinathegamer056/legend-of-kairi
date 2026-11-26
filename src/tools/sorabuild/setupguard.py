"""(s288) two setup guards shared by sorabuild.py, the GitHub installer (tools/installer/installer.py) and Melty's setup
(melty-setup/build.py loads this file by path from the release). Python stdlib only.

- take_lock / release_lock: one run at a time per folder. An OS lock (msvcrt.locking on Windows, fcntl.flock elsewhere)
  on a lock file, held for the whole run. The OS drops it when the process ends in any way (closed window, Task Manager,
  power loss), so a lock left by a dead process never blocks. The file's text (pid + start time) is information only.
  Melty triage K6 (2026-11-20): a 2nd setup started while the first still ran; its cs_build met the first one's
  partybake files ("[WinError 5] Access is denied" on OpenKh.Common.dll).
- wine_version: this Windows python runs under Wine / Proton (Steam Deck, Linux). Melty triage K8 (2026-11-24): the
  Windows package under Proton, zig failed "checking cache failed: manifest_create Unexpected".
"""
import errno
import os
import sys
import time

SORABUILD_LOCK = "sorabuild.lock"   # sorabuild.py: in its work folder (Melty: state/sorabuild)
SETUP_LOCK = "setup.lock"           # melty-setup/build.py: in its state folder
LOCK_AT = 1 << 20     # Windows: the locked byte, past the text (a locked range cannot be read by others; the text can)
LOCK_TRIES = 8        # a lock just dropped by a dead process can take a moment to go: try for ~2 s before saying busy
LOCK_WAIT_S = 0.25

BUSY_TEXT = ("Another Legend of Kairi setup is still running on this PC (an earlier one that has not finished, maybe with no "
             "window). Wait for it to finish, or close it, then try again. If you cannot find it: restart the PC, or end "
             "python.exe in Task Manager%s.")

WINE_TEXT = ("This is the Windows version of Legend of Kairi, and this PC runs it through Wine / Proton (Steam Deck or Linux), "
             "where its setup cannot build the game. On a Steam Deck or Linux use the Linux release from GitHub instead: "
             "https://github.com/tvllerr/legend-of-kairi/releases")


def _lock_os(fd):
    if sys.platform == "win32":
        import msvcrt
        os.lseek(fd, LOCK_AT, 0)
        msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)   # OSError when another process holds it
    else:
        import fcntl
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)


def _unlock_os(fd):
    if sys.platform == "win32":
        import msvcrt
        os.lseek(fd, LOCK_AT, 0)
        msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
    else:
        import fcntl
        fcntl.flock(fd, fcntl.LOCK_UN)


def holder(path):
    """the pid in a lock file's text ("" when none or unreadable): only for the message"""
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            w = f.read(64).split()
        return w[0] if w and w[0].isdigit() else ""
    except OSError:
        return ""


BUSY_ERRNOS = {errno.EACCES, errno.EAGAIN, errno.EWOULDBLOCK, getattr(errno, "EDEADLK", -1), getattr(errno, "EDEADLOCK", -1)}
NO_LOCK = -1          # take_lock: this folder cannot be locked (odd drive, no rights): the run goes on without the guard


def take_lock(path, tries=LOCK_TRIES, wait=LOCK_WAIT_S):
    """-> an fd that holds the lock on path (keep it until the run ends; release_lock or the process end drops it),
    or None when another live process holds it. Fails OPEN: when the file cannot be opened or locked here for another
    reason than "held" (a drive without locks, no rights), -> NO_LOCK and the run goes on as before this guard."""
    try:
        d = os.path.dirname(path)
        if d:
            os.makedirs(d, exist_ok=True)
        fd = os.open(path, os.O_RDWR | os.O_CREAT, 0o644)   # not inherited by the tools the run starts (PEP 446)
    except OSError:
        return NO_LOCK
    for i in range(max(1, tries)):
        try:
            _lock_os(fd)
            break
        except OSError as e:
            if e.errno not in BUSY_ERRNOS:   # (Windows: EACCES when held; POSIX flock: EWOULDBLOCK)
                os.close(fd)
                return NO_LOCK
            if i + 1 >= max(1, tries):
                os.close(fd)
                return None
            time.sleep(wait)
    try:   # ours now: say who holds it (information only)
        os.ftruncate(fd, 0)
        os.lseek(fd, 0, 0)
        os.write(fd, ("%d %s\n" % (os.getpid(), time.strftime("%Y-%m-%d %H:%M:%S"))).encode("ascii"))
    except OSError:
        pass
    return fd


def release_lock(fd):
    if fd is None or fd == NO_LOCK:
        return
    try:
        _unlock_os(fd)
    except OSError:
        pass
    try:
        os.close(fd)
    except OSError:
        pass


def busy_text(path):
    """the plain-words refusal for a lock another process holds"""
    pid = holder(path)
    return BUSY_TEXT % ((" (process %s)" % pid) if pid else "")


def _wine_probe():
    """the Wine version string, "" on real Windows. ntdll's wine_get_version export is Wine's own (Proton keeps it); a Wine
    prefix that hides it is still found by its HKCU\\Software\\Wine key AND Wine's System32\\wineboot.exe (both, so a stray
    key left on a real Windows PC by some program does not block it)."""
    try:
        import ctypes
        f = getattr(ctypes.WinDLL("ntdll"), "wine_get_version")   # AttributeError on real Windows
        f.restype = ctypes.c_char_p
        f.argtypes = []
        v = f()
        return (v.decode("ascii", "replace") if v else "") or "unknown version"
    except AttributeError:
        pass
    except Exception:   # noqa: BLE001 - a probe, never the setup
        pass
    try:
        import winreg
        winreg.CloseKey(winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Software\\Wine"))
    except OSError:
        return ""
    except Exception:   # noqa: BLE001
        return ""
    sysdir = os.path.join(os.environ.get("SystemRoot") or os.environ.get("windir") or "C:\\windows", "System32")
    return "registry" if os.path.isfile(os.path.join(sysdir, "wineboot.exe")) else ""


def wine_version():
    """-> the Wine version ("registry" when only the key + wineboot.exe show it) when this Windows python runs under Wine /
    Proton, else "" (real Windows, or a native Linux / macOS python, which never is the Windows setup)"""
    if sys.platform != "win32":
        return ""
    return _wine_probe()
