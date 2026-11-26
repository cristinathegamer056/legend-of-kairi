// (shielddef, user: "implement the defense stats from shields to Sora") Link's equipped BotW shield raises Sora's KH2
// Defense by its Guard / 10 (user's pick; KH2's Def runs 2 at LV1 to 53 at LV99, so the Hylian Shield's 90 = +9, a
// KH2-armor-sized step). No durability wear (user's pick). The Def feeds sl_def_tick's KH2 damage formula and the Status
// page. Guard = the actor pack's bgparamlist WeaponCommon.GuardPower (romfs Actor/Pack/Weapon_Shield_*.sbactorpack,
// BotW 1.9.0, all 33 shields). A shield's "Shield Guard Up" bonus: the pouch modifier (+0x80 value) counts when flag
// 0x100 is set (GUESS: BotW's AddGuard bit; logged). Env SAVAGEWIND_SORA_SHIELD_DEF (default 0: s288 port, OFF until the user
// plays it), live "set shielddef 0/1". BotW hits only (sl_def_tick -> the exe's hurt scale); duel hits (sora_duel.h du_take_hit:
// SL_LV[].def, life written directly) never see the bonus.
// Logs "[shield] ...".
#pragma once

#include "sora_shield_tab.h"  // (eqshield) the shield table (actor, English name, Guard), shared with the Shield row

static int g_shield_def = -1;
static struct { int (*info)(char *, int, int *, int *); bool bound; uint64_t next_ms; int bonus, guard; char name[64]; } SHD;

static bool shield_def_on(void) {
    if (g_shield_def < 0) g_shield_def = env_f("SAVAGEWIND_SORA_SHIELD_DEF", 1) != 0;  /* (s288: the user played it, shield changes Status Def: default ON) */
    return g_shield_def > 0;
}
// Sora's Def bonus from Link's equipped shield (re-read every 500 ms; 0 = off / no shield / old exe)
static int shield_def_bonus(void) {
    if (!shield_def_on()) return 0;
    if (!SHD.bound) {
        SHD.bound = true;
        SHD.info = (int (*)(char *, int, int *, int *))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_shield_info");
        LOG("[shield] exe export linkpos_shield_info: %s\n", SHD.info ? "found" : "NOT in this exe (shield Def off)");
    }
    if (!SHD.info) return 0;
    uint64_t now = GetTickCount64();
    if (now < SHD.next_ms) return SHD.bonus;
    SHD.next_ms = now + 500;
    char nm[64] = "";
    int mv = 0, mf = 0, guard = 0;
    if (SHD.info(nm, (int)sizeof nm, &mv, &mf)) {
        for (int i = 0; i < SHIELD_GUARD_N; i++)
            if (!strcmp(SHIELD_GUARD[i].name, nm)) guard = SHIELD_GUARD[i].guard;
        if (mf & SHIELD_GUARD_UP_FLAG) guard += mv;
    } else nm[0] = 0;
    int bonus = guard / 10;
    if (strcmp(nm, SHD.name) || bonus != SHD.bonus) {
        LOG("[shield] equipped %s: Guard %d (modifier value %d flags %#x) -> Def +%d\n", nm[0] ? nm : "(none)", guard, mv, mf, bonus);
        snprintf(SHD.name, sizeof SHD.name, "%s", nm);
    }
    SHD.bonus = bonus, SHD.guard = guard;
    return bonus;
}
static bool shield_live(const char *key, int v) {
    if (strcmp(key, "shielddef")) return false;
    g_shield_def = v != 0, SHD.next_ms = 0;
    LOG("[shield] live: shielddef %d\n", g_shield_def);
    return true;
}
