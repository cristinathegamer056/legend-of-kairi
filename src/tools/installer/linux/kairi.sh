#!/bin/sh
# Legend of Kairi (KH2 Sora in Breath of the Wild) - Linux / Steam Deck launcher, single player.
# Copied by install.sh into the install folder; running install.sh (or update.sh) again replaces it.
# Steam: Desktop Mode > Steam > Games > Add a Non-Steam Game to My Library > Browse > this file (no Proton / compatibility tool).
# The Linux twin of the Windows release's "Play Legend of Kairi.bat" (tools/installer/gameside.py launcher_text, solo).
#
# Install folder (R = the folder of this file):
#   bin/   the game program (savagewind, sora_mod.so, deps/lib, fonts, colour-luts, vk_pipelines.seed)
#   data/  the mod's files (built by install.sh from your Steam KINGDOM HEARTS II, every file checked)
#   sw/    the Savage Wind 0.6.0-alpha.6 game folder: extracted/ (BotW files + swgame.so), save/, savagewind.ini, cache/
#   logs/  last.log (this start), prev.log (the one before)
R=$(dirname "$(readlink -f "$0")")
LOGS="$R/logs"
mkdir -p "$LOGS"
say() {  # an error the player must see: a box when there is a screen (Game Mode has none of the terminal), and the log
    echo "[kairi] $*" >> "$LOGS/last.log"
    echo "Legend of Kairi: $*" >&2
    command -v zenity >/dev/null 2>&1 && zenity --error --title="Legend of Kairi" --width=420 --text="$*" >/dev/null 2>&1
    exit 1
}
[ -f "$LOGS/last.log" ] && mv -f "$LOGS/last.log" "$LOGS/prev.log"
echo "[kairi] $(date) start, install folder $R" > "$LOGS/last.log"

EXE="$R/bin/savagewind"
SW="$R/sw"
[ -x "$EXE" ] || say "The game program is missing ($EXE). Run install.sh again."
[ -d "$SW/extracted" ] || say "The Savage Wind game files are missing ($SW/extracted). Run install.sh again."
# one game at a time (this install's program)
if pgrep -f "^$EXE" >/dev/null 2>&1; then say "Legend of Kairi is already running. Close that game first, then start this again."; fi
# the game code Savage Wind set up must be the one this release's program was made for (data/GAME_ID, written by install.sh)
if [ -f "$R/data/GAME_ID" ]; then
    want=$(cat "$R/data/GAME_ID"); have=$(cat "$SW/extracted/swgame.so.id" 2>/dev/null)
    [ "$want" = "$have" ] || say "Savage Wind's game code is not the one this Legend of Kairi was made for. Run install.sh again: it fixes it."
fi
# the body files, and data/BUILD_ID (written by install.sh once every data file checked out) = manifest.json's build id
for k in sora_model.bin sora_pose.bin sora_motion.bin forms/roxas_model.bin ragmap/sora.bin hud/camp_seq.bin; do
    [ -f "$R/data/$k" ] || say "Legend of Kairi's files are incomplete ($k is missing). Run install.sh again."
done
want=$(sed -n 's/.*"build_id": *"\([0-9a-f]*\)".*/\1/p' "$R/manifest.json" 2>/dev/null | head -n 1)
if [ -n "$want" ]; then
    [ "$(cat "$R/data/BUILD_ID" 2>/dev/null)" = "$want" ] || say "Legend of Kairi's files are from another version. Run install.sh again."
fi

export SORA_ROOT="$R"
export SAVAGEWIND_DATA_ROOT="$SW"
export SAVAGEWIND_SORA=1
export SAVAGEWIND_SORA_DIR="$R/bin"
export SAVAGEWIND_SORA_RELOAD=0
export SAVAGEWIND_SORA_LIVE=
# solo: multiplayer off (this release plays on your own; SORA_SOLO also keeps the mod's co-op code off)
export SAVAGEWIND_SORA_SOLO=1
export SAVAGEWIND_SORA_NET=0
export SAVAGEWIND_SORA_NET_PEERS=
export SAVAGEWIND_LINK_EDMG=0
export SAVAGEWIND_LINK_ESYNC=0
export SAVAGEWIND_LINK_ESYNC_ATK=0
# Savage Wind's own update check off (it would offer the stock program over the mod's), vertex-copy feature off (as Windows)
export SAVAGEWIND_NO_UPDATE_CHECK=1
export SAVAGEWIND_VK_VDYN=0
# title logos, when this release has them
[ -f "$R/data/logo/Logo3DGameTitle_00.bin" ] && export SAVAGEWIND_LOGO_TEX="$R/data/logo/Logo3DGameTitle_00.bin"
[ -f "$R/data/logo/Logo2D_00.bin" ] && export SAVAGEWIND_LOGO2D_TEX="$R/data/logo/Logo2D_00.bin"
[ -f "$R/data/overlay/Pack/Title.pack" ] && export SAVAGEWIND_ROMFS_OVERLAY="$R/data/overlay"
export LD_LIBRARY_PATH="$R/bin/deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
echo "[kairi] env: $(env | grep -E '^(SAVAGEWIND_|SORA_)' | tr '\n' ' ')" >> "$LOGS/last.log"
cd "$SW" || say "Cannot open $SW."
exec "$EXE" extracted --no-launch-flags >> "$LOGS/last.log" 2>&1
