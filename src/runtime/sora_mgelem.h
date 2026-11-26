// sora_mgelem.h: (mgelem, PR #61; s288 port) Sora's Blizzard hits freeze and Thunder hits shock enemies through BotW's own element
// reaction, as an Ice / Shock Arrow does (enemies BotW never freezes / shocks ignore it). Fire already burns them (s59 fire_world).
// Key g_mgelem: compiled default 0 (s288 port: OFF until the user plays it), env SAVAGEWIND_SORA_MGELEM, live "set mgelem 0/1".
// 0 = no call at all (spell_hit is exactly the old one). Per-hit log lines: g_mgelem_log (live "set mgelemlog 0/1", default 1).
// The exe side is linkpos.c linkpos_enemy_element (the react(this, el) call 1.8.2 0x7000a73ec4 / 1.9.0 0x7000b873d4, sora_sites.h
// elem_react; another game version = 0 = refused there). Donald's spells are not changed.
// Co-op (ESYNC): an enemy that is a PUPPET here (another game owns its brain) gets no BotW reaction here, the same rule as the
// s194 netreact KH2 reaction (its owner would not see it, and the puppet's body is moved from the owner's samples). Not sent to
// the owner either (the port adds no net message): friends see the damage, not the ice / shock. A duel opponent is never
// in spell_hit's list (spells reach him through du_spell, sora_duel.h), so a friend is never frozen by this.
// Needs (sora.c, before this include): KH_SPELLS / SPELL_*, ah_actor_hid (sora_airhold.h), net_react_es / nr_puppet (sora_net_react.h).
#pragma once
static int g_mgelem = 1;  /* (s288: Claude tested freeze + shock in game; user: "Turn it on") */
static int g_mgelem_log = 1;
static bool mgelem_puppet_(uint64_t actor) {  // this enemy's brain runs on another game (ESYNC on in a room)
    if (!net_react_esync_on()) return false;
    bool local = false;
    int owner = -1, brain = -1;
    uint32_t hid = ah_actor_hid(actor);
    bool found = hid && net_react_es(hid, &local, &owner, &brain);
    return nr_puppet(true, found, local, owner, NET.me, brain);
}
// spell_hit, right after its damage went out: Blizzard -> BotW ice (3), Thunder -> BotW electric (4)
static void mgelem_hit(int kind, uint64_t actor) {
    if (!g_mgelem || (kind != SPELL_BLIZZARD && kind != SPELL_THUNDER)) return;
    int el = kind == SPELL_BLIZZARD ? 3 : 4;
    if (mgelem_puppet_(actor)) {
        if (g_mgelem_log) LOG("[sora] mgelem: %s hit %llx: puppet here (another game owns it): no BotW %s reaction\n", KH_SPELLS[kind].name,
                              (unsigned long long)actor, el == 3 ? "freeze" : "shock");
        return;
    }
    static int (*elem)(uint64_t, int);
    static bool looked;
    if (!looked) {
        looked = true, elem = (int (*)(uint64_t, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_enemy_element");
        if (!elem) LOG("[sora] mgelem: this exe has no linkpos_enemy_element (NEEDS EXE RELINK): no freeze / shock\n");
    }
    int r = elem ? elem(actor, el) : -1;
    if (g_mgelem_log)
        LOG("[sora] mgelem: %s hit %llx -> BotW %s reaction %s\n", KH_SPELLS[kind].name, (unsigned long long)actor,
            el == 3 ? "freeze" : "shock", r > 0 ? "sent" : r == 0 ? "not possible (no reaction object)" : "refused");
}
