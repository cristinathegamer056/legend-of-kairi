#!/bin/bash
# Legend of Kairi (Linux / Steam Deck) - update this install to the newest release on GitHub.
#   ./update.sh                 look for a newer release, download it, check its signature, install it over this one
#   ./update.sh --file TAR.GZ   install a release package you downloaded yourself (its .sig must sit next to it)
#   ./update.sh --force         also re-install when the newest release is this version
#   --kh2 DIR / --proton DIR    passed on to install.sh (a KINGDOM HEARTS / Proton folder it cannot find by itself)
# Saves, settings and Savage Wind's game files are kept (install.sh's update path). Only a package signed by the Legend of
# Kairi author is taken: the signature covers "legend-of-kairi-update-v1\n<file name>\n<size>\n<sha256>\n" (Ed25519, the
# same key and the same text as the Windows updater, tools/installer/sigcheck.py), checked with the system's openssl.
set -u -o pipefail
R=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
API=https://api.github.com/repos/tvllerr/legend-of-kairi/releases
DL_PREFIX=https://github.com/tvllerr/legend-of-kairi/releases/download/
PUBKEYS="20cbed9b257173dce6201213cf1246b6dda4d497b86dc6e90e1f57938ce8151d"
die() { echo "STOPPED: $*" >&2; exit 1; }
FILE=""; FORCE=0; PASS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --file) FILE=$2; shift 2 ;;
        --force) FORCE=1; shift ;;
        --kh2|--proton) PASS+=("$1" "$2"); shift 2 ;;
        -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
        *) die "unknown option $1" ;;
    esac
done
for t in python3 openssl sha256sum; do command -v $t >/dev/null 2>&1 || die "'$t' is missing on this system"; done
HAVE=$(cat "$R/VERSION" 2>/dev/null) || die "$R is not a Legend of Kairi install (no VERSION)"
mkdir -p "$R/dl"
W=$(mktemp -d "$R/dl/update.XXXXXX") || die "cannot write in $R/dl"
trap 'rm -rf "$W"' EXIT

if [ -z "$FILE" ]; then
    command -v curl >/dev/null 2>&1 || die "'curl' is missing"
    echo "Legend of Kairi $HAVE: looking for a newer release..."
    curl -fsSL --proto '=https' --max-filesize 5242880 -H 'Accept: application/vnd.github+json' -o "$W/releases.json" "$API" \
        || die "could not reach GitHub"
    # the newest release (GitHub lists newest first) that has a Linux package AND its .sig
    read -r NAME URL SIGURL < <(python3 -I -B - "$W/releases.json" "$DL_PREFIX" <<'EOF'
import json, re, sys
pre = sys.argv[2]
for rel in json.load(open(sys.argv[1], encoding="utf-8")):
    if rel.get("draft"):
        continue
    a = {x.get("name", ""): x.get("browser_download_url", "") for x in rel.get("assets") or []}
    for n, u in a.items():
        if re.fullmatch(r"Legend-of-Kairi-[0-9A-Za-z.+-]+-linux-x86_64\.tar\.gz", n) and (n + ".sig") in a \
                and u.startswith(pre) and a[n + ".sig"].startswith(pre):
            print(n, u, a[n + ".sig"])
            sys.exit(0)
print("- - -")
EOF
)
    [ "${NAME:--}" != "-" ] || die "no Linux release found on GitHub"
    NEW=${NAME#Legend-of-Kairi-}; NEW=${NEW%-linux-x86_64.tar.gz}
    if [ "$NEW" = "$HAVE" ] && [ $FORCE = 0 ]; then echo "Up to date: Legend of Kairi $HAVE is the newest."; exit 0; fi
    echo "Downloading Legend of Kairi $NEW..."
    curl -fsSL --proto '=https' --max-filesize 4096 -o "$W/$NAME.sig" "$SIGURL" || die "the .sig download failed"
    curl -fL --proto '=https' --max-filesize 2000000000 -o "$W/$NAME" "$URL" || die "the download failed"
    FILE="$W/$NAME"
else
    FILE=$(readlink -f "$FILE") || die "no such file"
    [ -f "$FILE" ] && [ -f "$FILE.sig" ] || die "need $FILE and $FILE.sig"
    NAME=$(basename "$FILE")
    case "$NAME" in Legend-of-Kairi-*-linux-x86_64.tar.gz) ;; *) die "$NAME is not a Legend of Kairi Linux package" ;; esac
    NEW=${NAME#Legend-of-Kairi-}; NEW=${NEW%-linux-x86_64.tar.gz}
    cp -f "$FILE.sig" "$W/$NAME.sig"
fi

# ---- the signature (Ed25519 over name + size + sha256), with the system openssl
SIZE=$(stat -c %s "$FILE"); SHA=$(sha256sum "$FILE" | cut -d' ' -f1)
printf 'legend-of-kairi-update-v1\n%s\n%s\n%s\n' "$NAME" "$SIZE" "$SHA" > "$W/msg"
python3 -I -B -c 'import sys; open(sys.argv[2], "wb").write(bytes.fromhex(open(sys.argv[1]).read().strip()))' "$W/$NAME.sig" "$W/sig.bin" \
    || die "the .sig file is not readable"
[ "$(stat -c %s "$W/sig.bin")" = 64 ] || die "the .sig file is not an Ed25519 signature"
OK=0
for K in $PUBKEYS; do
    # Ed25519 public key (32 bytes) as a PEM: the fixed SubjectPublicKeyInfo prefix + the key
    python3 -I -B -c 'import base64, sys; d = bytes.fromhex("302a300506032b6570032100" + sys.argv[1]); print("-----BEGIN PUBLIC KEY-----\n" + base64.b64encode(d).decode() + "\n-----END PUBLIC KEY-----")' "$K" > "$W/pub.pem"
    openssl pkeyutl -verify -pubin -inkey "$W/pub.pem" -rawin -in "$W/msg" -sigfile "$W/sig.bin" >/dev/null 2>&1 && OK=1 && break
done
[ $OK = 1 ] || die "this package is not signed by the Legend of Kairi author: not installed"
echo "Signature OK ($NAME, $SIZE bytes)."

# ---- unpack (no absolute paths, no .., no links out) and run its install.sh over this install
python3 -I -B - "$FILE" "$W/pkg" <<'EOF' || die "the package did not unpack"
import os, sys, tarfile
src, out = sys.argv[1], sys.argv[2]
os.makedirs(out)
with tarfile.open(src, "r:gz") as t:
    try:
        t.extractall(out, filter="data")
    except TypeError:          # (python without extraction filters: check the names by hand)
        for m in t.getmembers():
            n = os.path.normpath(m.name)
            if n.startswith(("/", "..")) or m.issym() or m.islnk() or m.isdev():
                raise SystemExit("unsafe entry %s" % m.name)
        t.extractall(out)
EOF
P=$(find "$W/pkg" -mindepth 2 -maxdepth 2 -name install.sh | head -n 1)
[ -n "$P" ] || die "no install.sh in the package"
[ "$(cat "$(dirname "$P")/VERSION" 2>/dev/null)" = "$NEW" ] || die "the package's VERSION is not $NEW"
echo "Installing Legend of Kairi $NEW over $HAVE (saves and settings are kept; a new version may build the mod's data, 20-25 minutes)..."
bash "$P" --dir "$R" --yes "${PASS[@]}" || die "the install of $NEW did not finish (your saves are untouched; log: $R/logs/install.log)"
echo "Updated: Legend of Kairi $HAVE -> $NEW."
