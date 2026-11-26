"""BotW-derived level data (built from the player's own romfs)."""
import os
from sb import Step


def _gdt(ctx):
    ctx.need(os.path.join(ctx.romfs, "Pack", "Bootup.pack"))
    tmp = ctx.outp("levels/gamedata_flag_names.txt") + ".tmp"
    ctx.py(os.path.join(ctx.tools, "levels", "gdt_names.py"), ctx.romfs, tmp)
    ctx.commit(tmp, ctx.outp("levels/gamedata_flag_names.txt"))


def _updrafts(ctx):
    # (s288 #21 updraft) BotW's updraft columns of the main map for Sora's glide (runtime/sora_updraft.h); MainField only
    ctx.need(os.path.join(ctx.romfs, "Map", "MainField"), os.path.join(ctx.romfs, "Actor", "Pack"))
    tmp = ctx.outp("levels/updrafts.txt") + ".tmp"
    ctx.py(os.path.join(ctx.tools, "levels", "gen_updrafts.py"), ctx.romfs, tmp)
    ctx.commit(tmp, ctx.outp("levels/updrafts.txt"))


STEPS = [Step("gdt_names", _gdt, outputs=["levels/gamedata_flag_names.txt"]),
         Step("updrafts", _updrafts, outputs=["levels/updrafts.txt"])]
