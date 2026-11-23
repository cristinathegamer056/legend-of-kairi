// s122 Keyblade vs the BotW world (recon/S122_HELPER_TREES.md, S122_HELPER_STASIS.md): cuttable trees and Stasis'd
// objects. linkpos.c keeps both lists from the opacity hook; sora.c finds the functions with GetProcAddress (an exe
// linked before s122 has none: then trees / Stasis are off). Separate from linkpos_break.h on purpose (that struct
// is shared with older exes).
#pragma once
#include <stdint.h>

typedef struct {
    uint64_t actor;
    float pos[3];           // feet (mMtx translation), BotW units
    int32_t life, life_max; // hits left as BotW life (+0x848; LumberjackTree GParam Life 40)
    char name[48];
} LinkposTree;

typedef struct {
    uint64_t actor;
    float pos[3];           // mMtx translation, BotW units
    int32_t hits;           // StasisInfo hit counter (+0x27c)
    float stored;           // |stored blow vector| (+0x280), BotW's own units
    char name[48];
} LinkposStasisObj;

// (logchop) a felled tree's trunk (Obj_Tree*_Trunk, profile WeaponLargeSword, its LumberjackTree node in mode 1), lying
typedef struct {
    uint64_t actor;
    float pos[3];   // mMtx translation (the trunk's root end), BotW units
    float axis[3];  // the trunk's length direction (the tree's local Y, unit)
    char name[48];
} LinkposTrunk;
typedef int (*LinkposTrunksFn)(LinkposTrunk *out, int max);  // "linkpos_trunks": copies, any thread
typedef int (*LinkposTrunkHitFn)(uint64_t actor);            // "linkpos_trunk_hit": 1 = chop flag set, -1 refused
typedef int (*LinkposTreesFn)(LinkposTree *out, int max);                    // "linkpos_trees": copies, any thread
typedef int (*LinkposTreeHitFn)(uint64_t actor, int n, float dx, float dz);  // "linkpos_tree_hit": life after (0 = felled), -1 refused
typedef int (*LinkposStasisObjsFn)(LinkposStasisObj *out, int max);         // "linkpos_stasis_objs": copies, any thread
// "linkpos_stasis_hit": BotW's StasisInfo::add(info, actor, &dir, &pos, flags bit0 count, bit1 angle, bit2 noPush, kind
// 0 one-handed / 1 two-handed / 2 spear). 1 = added, -1 refused.
typedef int (*LinkposStasisHitFn)(uint64_t actor, float dx, float dy, float dz, int kind, int flags);
