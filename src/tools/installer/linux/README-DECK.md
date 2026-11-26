# Legend of Kairi on the Steam Deck (and other x86_64 Linux)

KH2 Sora in Breath of the Wild, single player, on Savage Wind 0.6.0-alpha.6. The game is a native Linux build (no Proton for it).

## What you need
- Your own BotW 1.9.0 dump (a folder with `exefs/` and `romfs/`), as for Savage Wind itself.
- The Steam KINGDOM HEARTS HD 1.5+2.5 ReMIX installed on the Deck, **unmodded** (a KH2 mod that changed the `kh2_*.pkg`
  files is refused; your KH2 folder is only read, never changed). Only the Steam KH2 is supported.
- A Steam Proton (Proton 11.0 is the one it was checked with; Library > search "Proton 11.0" > Install). Its wine runs one
  Windows tool of the data build. The game itself is native Linux: no Proton for the game.
- About 30 GB free on the Deck (less when the BotW dump is on the same drive: the game files are hard-linked then).
- The Deck PLUGGED IN: the first install builds the mod's data from your KH2 files (about 20-25 minutes).

## Install (Desktop Mode)
1. Download `Legend-of-Kairi-<version>-linux-x86_64.tar.gz` and unpack it (Dolphin: right-click > Extract > Extract here).
2. Open the unpacked folder, right-click an empty spot > Open Terminal Here (Konsole), and run:

       ./install.sh

   It finds your Steam KH2 and Proton by itself (else it asks, or `--kh2 DIR` / `--proton DIR`), asks for the BotW dump
   folder, downloads the stock Savage Wind 0.6.0-alpha.6 AppImage from the Savage Wind GitHub releases (checked by size +
   sha256), sets the game files up (10-20 minutes the first time), builds the mod's data (20-25 minutes) and checks every
   data file against the list made on a Steam Deck. It installs into `~/LegendOfKairi` (`--dir` to change; `--help`).
   Stopped half-way (sleep, battery)? Run it again: it goes on where it stopped.
3. Add it to Steam: Steam > Games > **Add a Non-Steam Game to My Library...** > Browse... > `~/LegendOfKairi/kairi.sh`
   (set the file type to "All Files") > Add Selected Programs. Do **not** set a compatibility tool (Proton) for it.
   In its Properties you can rename it to "Legend of Kairi".
4. Back to Game Mode > Library > Non-Steam > Legend of Kairi > Play.

The first start waits about a minute once (shaders). A game started outside Steam gets no controller input in Game Mode:
always start it through the Steam shortcut.

## Saves and settings
`~/LegendOfKairi/sw/save/` and `~/LegendOfKairi/sw/savagewind.ini`. To bring your Windows saves over, copy the
Windows Savage Wind folder's `save/` folder there while the game is closed (back up the Deck's first).

## Update
Desktop Mode, Konsole: `~/LegendOfKairi/update.sh`. It finds the newest Linux release on GitHub, checks the
author's signature, and installs it over yours (saves, settings and the game files are kept). A downloaded package:
`./update.sh --file Legend-of-Kairi-<version>-linux-x86_64.tar.gz` (its `.sig` next to it). A version with new data builds
it on the Deck (20-25 minutes, plugged in); your old version keeps working until the new data is built and checked.

## Logs, problems
- `~/LegendOfKairi/logs/last.log` (start), `~/LegendOfKairi/sw/savagewind.log` (the game), `logs/install.log`.
- "Savage Wind's game code is not the one ..." or "files are from another version": run `install.sh` again.
- "your KINGDOM HEARTS II files are MODDED": remove the KH2 mod (or Steam > Properties > Installed Files > Verify
  integrity), or give an unmodded copy of the KH2 folder with `--kh2 DIR`.
- "came out different again after a rebuild": the data is checked against a Steam Deck build; other Linux PCs are not
  proven yet. Send `~/LegendOfKairi/logs` to the author.
- Uninstall: delete `~/LegendOfKairi` (your saves are in it: copy `sw/save` first) and remove the Steam shortcut.

Licenses: `LICENSES/README.txt`.
