// (eqshield) BotW 1.9.0's 33 shields: actor, English name (Msg_USen ActorType/WeaponShield), Guard = the actor pack's
// bgparamlist WeaponCommon.GuardPower (romfs Actor/Pack/Weapon_Shield_*.sbactorpack). The KH2 Equipment page's Shield row
// shows it (sora_camp_equip.h); sora_shield.h adds Guard / 10 to Sora's Defense.
#pragma once
typedef struct { const char *name, *disp; int guard; } ShieldGuard;  // actor, English name (Msg_USen ActorType/WeaponShield), Guard
static const ShieldGuard SHIELD_GUARD[] = {
    {"Weapon_Shield_001", "Wooden Shield", 2},
    {"Weapon_Shield_002", "Soldier's Shield", 16},
    {"Weapon_Shield_003", "Knight's Shield", 40},
    {"Weapon_Shield_004", "Boko Shield", 3},
    {"Weapon_Shield_005", "Spiked Boko Shield", 10},
    {"Weapon_Shield_006", "Dragonbone Boko Shield", 25},
    {"Weapon_Shield_007", "Lizal Shield", 15},
    {"Weapon_Shield_008", "Reinforced Lizal Shield", 22},
    {"Weapon_Shield_009", "Steel Lizal Shield", 35},
    {"Weapon_Shield_013", "Guardian Shield", 18},
    {"Weapon_Shield_014", "Guardian Shield+", 30},
    {"Weapon_Shield_015", "Guardian Shield++", 42},
    {"Weapon_Shield_016", "Lynel Shield", 30},
    {"Weapon_Shield_017", "Mighty Lynel Shield", 44},
    {"Weapon_Shield_018", "Savage Lynel Shield", 62},
    {"Weapon_Shield_021", "Rusty Shield", 3},
    {"Weapon_Shield_022", "Royal Shield", 55},
    {"Weapon_Shield_023", "Forest Dweller's Shield", 30},
    {"Weapon_Shield_025", "Silver Shield", 18},
    {"Weapon_Shield_026", "Gerudo Shield", 20},
    {"Weapon_Shield_030", "Hylian Shield", 90},
    {"Weapon_Shield_031", "Hunter's Shield", 3},
    {"Weapon_Shield_032", "Fisherman's Shield", 3},
    {"Weapon_Shield_033", "Royal Guard's Shield", 70},
    {"Weapon_Shield_034", "Emblazoned Shield", 3},
    {"Weapon_Shield_035", "Traveler's Shield", 4},
    {"Weapon_Shield_036", "Radiant Shield", 35},
    {"Weapon_Shield_037", "Daybreaker", 48},
    {"Weapon_Shield_038", "Ancient Shield", 70},
    {"Weapon_Shield_040", "Pot Lid", 1},
    {"Weapon_Shield_041", "Shield of the Mind's Eye", 16},
    {"Weapon_Shield_042", "Kite Shield", 14},
    {"Weapon_Shield_057", "Hero's Shield", 65},
};
#define SHIELD_GUARD_N ((int)(sizeof SHIELD_GUARD / sizeof *SHIELD_GUARD))
#define SHIELD_GUARD_UP_FLAG 0x100u  // GUESS: the pouch modifier flag of "Shield Guard Up" (+0x88; value +0x80)
