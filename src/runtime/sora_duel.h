// (s272 duel, co-op item 6) DUELS: the game side. Included once by sora.c after sora_sumlv_late.h (needs S, G, guard_window,
// guard_block, g_rc_inv, mg_reflect_up, pl_invuln, MP, P, party_all_off, SM, SKT, fm_in_form, IN_MICKEY, link_heal, link_actor,
// rd_ok, sora_fx_*, fx_mine, g_fx_gen, CAMP / cs_text, KDG, REACT, rcs_show, M, MC, DR, SL_LV / sl_lv / sl_str_of / sl_mag_of /
// SL_KEY_*, sl_khdiff, berserk_on, ab_finisher_dmg, ab_on, kbfx_name, kbse_hit, kbreach_hit, wall_hides, g_spark_at /
// g_sweep_hit, g_wpn_cur, NET, peer_sample, g_body, g_loading, TITLE, g_hud_life_max, PAUSE_STATE, LOG).
// Pure parts + the design: sora_duel_core.h (host test runtime/test/duel_test.c); net: sora_net_duel.h; wire: NET_DUEL.
//
// How a duel goes (both games run this file; the asker A, the friend B):
//   A stands near B (6 m), both out of combat -> A's Triangle slot shows "Duel" -> Triangle = ASK to B.
//   B's game checks itself (out of combat, base form, no summon...) -> "Accept Duel" on his Triangle for 10 s (no answer = decline)
//   -> his Triangle = ANSWER accept + the ring's centre (between the two now) -> both: the ring, 3-2-1, "Go!", the fight.
//   A hit: the ATTACKER's game tests his blade window / Fire / Blizzard / Thunder against the opponent's puppet as drawn there
//   (KH2's Sora target part as a cylinder), once per window (Fire: per tick), and sends HIT (his S + Power + boost); the VICTIM's
//   game checks its own Guard / invulnerable frames / Reflect, takes KH2's formula with its own Defense + difficulty and writes the
//   life DOWN (never below 1). Sora's KH2 flinch then plays by itself (the exe sees the life drop), facing the attacker.
//   1 HP = KO -> END: the other wins. Also: leaving the ring 0.5 s = forfeit, an enemy within 30 m of the ring / either of us =
//   over without a winner, a load / another map = forfeit, the other game gone 3 s = you win, 3 min = the higher HP % wins.
//   During it: the party waits (off, back after), Drive / Limits / summons / items are refused, world damage can't take you below
//   1, no Drive gauge from duel hits (no dg_hit on that path). After: both healed + MP full.
// Live keys (DLL, hot reload): set duel 1 (default 0 = none of this: no prompt, no answer to an ASK, no hit path), set duellog 1
//   (per packet / hit lines), duelring <m> (20), duelringpct <%> (the scaled Cure ring's fx size, 250 GUESS ~10 m), duelnear <m> (6),
//   dueltime <s> (180), duelchase 0|1 (the enemy target-state read, 1), duelfake 1|2|0 (one-game test, see below),
//   duelfakehit <ms> (4000; 0 = the pretend opponent never hits), duelfakepow <power> (100).
// Live lines: duel ask <slot> (ask without the prompt), duel accept, duel decline, duel end (forfeit / cancel), duel diag.
// One-game test (no second copy): duelfake 1 = a pretend friend 3 m ahead, standing still (a sparkle marks it): "Duel" shows near
//   it, it accepts after 1.5 s; your hits on it count with your own Defense as its, it hits you every duelfakehit ms (through the
//   real victim path: Guard, KO floor, flinch); duelfake 2 = it ASKS you (your "Accept Duel" prompt). Covers the prompts, the
//   gate, the countdown, the ring + its leave rule, the enemy end, the time limit, the blade / spell hit tests, the victim path,
//   the KO, the heal after, the blocks. Not covered: the packets between two games, the puppet's lag, a third player.
#pragma once
#include "sora_duel_core.h"

#define DU_NQ 160
#define DU_FAKE_K (-2)          // DU.k of the pretend opponent
#define DU_FAKE_ACCEPT_MS 1500  // the pretend friend's answer
#define DU_SCAN_MS 200          // the enemy scan's rate
#define DU_KO_HOLD_MS 2000      // (user s273) a KO: the loser stays at 1 HP this long (the floor holds: DU_OVER), then both are healed
#define DU_ACTOR_TAG 0xD0E1D0E1D0E1ull  // (kbreach_hit's log key for the opponent: no BotW actor)
#define DU_TGT_STATE 0xcc4     // BotW enemy target record state (linkpos.c TGT_STATE, recon S55)

static float g_du_ring_m = DU_RING_M, g_du_near_m = DU_NEAR_M;
static bool g_du_r1_once;  // (s275) live "duel lock": one R1 press for the one-game check (sora.c lock_tick)
static int g_du_lock = 1;  // (s275; default ON s279, his two-copy check "test 1 works") live "set duellock 0" = off: the opponent is a KH2 target in the FIGHT (sora.c targets_build)
static int g_du_ring_pct = 250, g_du_chase = 1, g_du_limit_s = 180, g_du_fake_hit_ms = 4000, g_du_fake_pow = 100;
// (s286, the user) live "set dueldmg <pct>": the damage WE take in a duel x pct (du_dmg_scale; 100 = KH2 as is);
// "set duelrag 1": a combo finisher that lands on us knocks us down (BotW's ragdoll, the exe's linkpos_link_knock);
// "set duelfakefin <n>": every n-th hit of the pretend friend is a finisher (0 = none). Both new = default off.
static int g_du_dmg_pct = 100, g_du_rag = 0, g_du_fake_fin = 3;
typedef int (*LinkposKnockFn)(int f54);
// knock Link (and so Sora) down: an explosion-type LARGE reaction with no life lost. -> false = an exe without it
static bool du_knock(void) {
    static LinkposKnockFn fn;
    static int tried;
    if (!tried) {
        tried = 1, fn = (LinkposKnockFn)(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_link_knock");
        if (!fn) LOG("[duel] knock-down: linkpos_link_knock not in this exe (an older exe): finishers do not ragdoll\n");
    }
    if (!fn) return false;
    fn(0x15);
    return true;
}

static struct {
    DuSm sm;
    int k;                  // the opponent's slot (DU_FAKE_K = the pretend one, -1 none)
    uint8_t player;         // his player id at the ask (a slot that changed hands = not him)
    char name[16];
    float c[3], r;          // the ring
    DuFake fk;
    int fake_mode;          // live duelfake: 0 off, 1 it waits to be asked, 2 it asks you
    int fake_mark_fx;
    uint64_t fake_mark_at;
    // prompts
    int prompt;             // 0 none, 1 "Duel" (we ask prompt_k), 2 "Accept Duel"
    int prompt_k;
    uint64_t cool_until, idle_since;
    // hits taken outside a duel (the gate's 10 s rule)
    int hits_seen;
    uint64_t last_hit_ms, own_write_ms;
    // the fight
    uint16_t hseq;
    DuSeen seen;
    DuOut out;
    uint64_t last_flag_ms, ring_tx;
    float front_from[3];
    uint64_t front_at;
    int hit_swing, hit_win, hit_blade;
    bool party_was;
    int life_ok;            // the last unpaused fight frame's life (a BotW-menu meal is put back)
    bool paused_seen;
    int nhits_dealt, nhits_taken, nblocked;
    // the enemy scan
    uint64_t scan_at;
    float scan_enemy_m, scan_chase_m;
    float scan_at_pos[3];
    // fx
    int ring_fx;
    uint32_t ring_gen;
    struct { bool on; uint8_t a, b; float c[3], r; uint64_t at; int fx; uint32_t gen; } other[NET_MAX_PEERS];
    // the screen text
    void (*notice)(const SoraHudQuad *, int);
    SoraHudQuad q[DU_NQ];
    int nq;
    bool notice_sent;
    char banner[64];
    uint64_t banner_t0, banner_ms;
    int last_digit;
    uint32_t nduels;
    uint64_t heal_at;       // (s273) a KO's heal after DU_KO_HOLD_MS (0 = none pending)
    bool heal_ch;
} DU = {.k = -1, .ring_fx = -1, .fake_mark_fx = -1, .hits_seen = -1, .hit_swing = -1, .last_digit = -9};

static bool du_fake(void) { return DU.k == DU_FAKE_K; }
static const char *du_who(void) { return du_fake() ? "the pretend friend" : DU.name[0] ? DU.name : "your friend"; }
static void du_banner(const char *txt, uint64_t ms, uint64_t now) {
    snprintf(DU.banner, sizeof DU.banner, "%s", txt), DU.banner_t0 = now ? now : 1, DU.banner_ms = ms;
}

// ---- the opponent as seen here ----
static bool du_slot_him(int k) {
    return k >= 0 && k < NET_MAX_PEERS && NET.on && NET.slot[k].used && NET.slot[k].p.st != PEER_NONE && NET.slot[k].p.player == DU.player;
}
// a friend's drawn feet here (false = no valid pose / hidden on another map)
static bool du_peer_feet(int k, float out[3], uint64_t now) {
    // (s284, s273 review L8: a faded-out friend is no target: his invisible slot no longer takes Triangle / hits)
    if (k < 0 || k >= NET_MAX_PEERS || !NET.slot[k].used || NET.slot[k].hidden || NET.slot[k].p.st != PEER_LIVE) return false;
    PeerPose q = peer_sample(&NET.slot[k].p, now);
    if (!q.valid) return false;
    memcpy(out, q.feet, sizeof q.feet);
    return isfinite(out[0]) && isfinite(out[1]) && isfinite(out[2]);
}
static uint16_t du_peer_flags(int k) {
    const NetPeer *p = &NET.slot[k].p;
    return p->n > 0 ? p->ring[p->n - 1].flags : 0;
}
static int du_peer_hp(int k) {
    const NetPeer *p = &NET.slot[k].p;
    return p->n > 0 ? p->ring[p->n - 1].hp : 0;
}
static bool du_opp_feet(float out[3], uint64_t now) {
    if (du_fake()) { memcpy(out, DU.fk.feet, 12); return DU.fk.on; }
    return du_slot_him(DU.k) && du_peer_feet(DU.k, out, now);
}

// ---- the gate ----
static void du_scan(uint64_t now, const float at[3], bool force) {
    if (!force && now < DU.scan_at) return;
    DU.scan_at = now + DU_SCAN_MS;
    memcpy(DU.scan_at_pos, at, 12);
    DU.scan_enemy_m = DU.scan_chase_m = -1;
    uint64_t a[64];
    float p[64][3];
    int32_t l[64];
    int n = linkpos_enemies(a, p, l, 64);
    for (int i = 0; i < n; i++) {
        if (l[i] <= 0) continue;
        float d = hypotf(p[i][0] - at[0], p[i][2] - at[2]);
        if (d > DU_CHASE_M && d > DU_ENEMY_M) continue;
        char nm[64];
        if (!linkpos_enemy_name(a[i], nm, (int)sizeof nm) || !du_enemy_name(nm)) continue;  // (animals do not count)
        if (d <= DU_ENEMY_M && (DU.scan_enemy_m < 0 || d < DU.scan_enemy_m)) DU.scan_enemy_m = d;
        if (g_du_chase && d <= DU_CHASE_M && rd_ok(a[i] + DU_TGT_STATE, 4) && du_chasing_state(*(const int32_t *)(uintptr_t)(a[i] + DU_TGT_STATE)) &&
            (DU.scan_chase_m < 0 || d < DU.scan_chase_m))
            DU.scan_chase_m = d;
    }
}
static bool du_menu_up(void) { return M.sub >= 0 || M.shortcut || MC.on || CAMP.drew || DR.st != DR_OFF; }
static bool du_ground(void) { return linkpos_link_state() == LINK_GROUND && !ACT_AIR(S.act) && !S.air_frames; }
static int du_gate_now(uint64_t now, bool strict, bool busy) {
    DuGateIn g = {.on = g_duel && g_on, .driven = (NET.driven || g_av_local_ok) && !g_loading && !NET.dead && !TITLE.on, .busy = busy, .menu = du_menu_up(),
                  .form = fm_in_form() || IN_MICKEY, .summon = sm_out(), .board = SKT.riding, .ground = du_ground(), .now = now,
                  .cool_until = DU.cool_until, .last_hit_ms = DU.last_hit_ms};
    float me[3] = {g_body[0], g_body[1], g_body[2]};
    du_scan(now, me, false);
    g.enemy_m = DU.scan_enemy_m, g.chase_m = DU.scan_chase_m;
    return du_gate(&g, strict);
}

// ---- sends ----
static void du_send(const NetDuelMsg *m, uint64_t now) {
    if (du_fake() || DU.k < 0) return;
    if (du_slot_him(DU.k)) net_duel_send(DU.k, m, now);
}
static void du_ring_note(int phase, uint64_t now) {
    if (du_fake()) return;
    NetDuelMsg m = {.op = NDO_RING, .id = DU.sm.id, .a = NET.me, .b = DU.player, .phase = (uint8_t)phase,
                    .radius_cm = (uint16_t)(DU.r * 100 > 65000 ? 65000 : DU.r * 100)};
    memcpy(m.c, DU.c, 12);
    net_duel_ring_all(&m, DU.k, now);
    DU.ring_tx = now + DU_RING_TX_MS;
}

// ---- the life write (as link_heal, downward; never below 1) ----
static bool du_life_set(int v) {
    int cur = linkpos_link_life();
    uint64_t a = link_actor();
    if (cur <= 0 || v < 1 || !a || !rd_ok(a + 0x848, 4) || *(const int32_t *)(uintptr_t)(a + 0x848) != cur) return false;
    *(int32_t *)(uintptr_t)(a + 0x848) = v;
    return true;
}

// ---- start / end ----
static void du_ring_fx_off(void) {
    if (DU.ring_fx >= 0 && fx_mine(DU.ring_fx, DU.ring_gen)) sora_fx_kill(DU.ring_fx);
    DU.ring_fx = -1;
}
static void du_go_live(uint64_t now) {
    DU.hseq = 0, memset(&DU.seen, 0, sizeof DU.seen), DU.out.since = 0, DU.last_flag_ms = 0, DU.ring_tx = 0, DU.front_at = 0;
    DU.hit_swing = -1, DU.life_ok = -1, DU.paused_seen = false, DU.nhits_dealt = DU.nhits_taken = DU.nblocked = 0, DU.last_digit = -9;
    DU.party_was = P.on;
    if (P.on) P.on = false, party_all_off();  // (user: no party in a duel; Mickey's own pattern, sora_mickey.h; back at the end)
    DU.heal_at = 0;
    {  // (user s273: both fully healed + MP full at the countdown, so every duel starts fair)
        int l0 = linkpos_link_life(), l1 = l0 > 0 ? link_heal(999) : l0;
        bool ch = mp_charging();
        MP.mp = MP.mp_max, MP.charge = MP.charge_total = 0;
        LOG("[duel] start heal: life %d -> %d, MP full%s\n", l0, l1, ch ? " (MP Charge ended)" : "");
    }
    DU.nduels++;
    sora_sfx_event("submenu", 1.0f);  // GUESS: the menu's submenu SE as the duel's start
    LOG("[duel] duel %08x with %s: ring %.1f m at %.2f %.2f %.2f, 3-2-1 then %d s (you are the %s)\n", (unsigned)DU.sm.id, du_who(), DU.r, DU.c[0],
        DU.c[1], DU.c[2], g_du_limit_s, DU.sm.role == DU_ROLE_ASKER ? "asker" : "friend");
    du_ring_note(1, now);
}
static const char *du_result_txt(int res, int reason) {
    if (res == DUR_WIN) return reason == DUE_GONE ? "You win! (your friend left)" : reason == DUE_FORFEIT || reason == DUE_LEFT ? "You win! (your friend left the ring)" : "You win!";
    if (res == DUR_LOSE) return reason == DUE_FORFEIT ? "You lose... (you left the ring)" : reason == DUE_LEFT ? "You lose... (you left)" : "You lose...";
    if (res == DUR_DRAW) return "Draw";
    return reason == DUE_ENEMY ? "Duel over: an enemy came near" : "Duel cancelled";
}
// the heal after a duel (full life + MP); DU.heal_ch = MP Charge was cut
static int du_heal_now(void) {
    int life = linkpos_link_life(), l2 = life > 0 ? link_heal(999) : life;
    DU.heal_ch = mp_charging();
    MP.mp = MP.mp_max, MP.charge = MP.charge_total = 0;
    return l2;
}
// the end on this game: heal, MP, the party back, the banner. send = tell the other game (END)
static void du_end(int result, int reason, bool send, uint64_t now) {
    bool was_live = du_sm_live(&DU.sm);
    int life = linkpos_link_life();
    if (send) {
        NetDuelMsg m = {.op = NDO_END, .id = DU.sm.id, .reason = (uint8_t)reason, .result = (uint8_t)result, .pm = du_pm(life, g_hud_life_max)};
        du_send(&m, now);
    }
    du_sm_end(&DU.sm, result, reason, now);
    DU.prompt = 0;
    if (was_live) {
        // (user: both fully healed + MP full after; s273: a KO holds ~2 s at 1 HP first, the flinch + the win / lose moment)
        bool hold = reason == DUE_KO;
        int l2 = life;
        bool ch = false;
        if (hold) DU.heal_at = now + DU_KO_HOLD_MS;
        else DU.heal_at = 0, l2 = du_heal_now(), ch = DU.heal_ch;
        if (!hold) {  // (s273 live: the party back at once let Donald Cure the KO'd Sora mid-hold: back with the heal instead)
            if (DU.party_was) P.on = true;
            DU.party_was = false;
        }
        du_ring_note(0, now);
        if (hold) LOG("[duel] over: %s (%s, %s) after %.1f s; hits dealt %d, taken %d, blocked %d; life %d, the heal in %d ms\n", DUR_NAME[result],
                      DUE_NAME[reason], du_who(), (now - DU.sm.count0) / 1000.0, DU.nhits_dealt, DU.nhits_taken, DU.nblocked, life, DU_KO_HOLD_MS);
        else LOG("[duel] over: %s (%s, %s) after %.1f s; hits dealt %d, taken %d, blocked %d; healed %d -> %d, MP full%s\n", DUR_NAME[result], DUE_NAME[reason],
            du_who(), (now - DU.sm.count0) / 1000.0, DU.nhits_dealt, DU.nhits_taken, DU.nblocked, life, l2, ch ? " (MP Charge ended)" : "");
        sora_sfx_event(result == DUR_WIN ? "decide" : result == DUR_LOSE ? "cancel" : "page", 1.0f);  // GUESS / INVENTED SEs
    } else LOG("[duel] %s ended before the countdown: %s\n", du_who(), DUE_NAME[reason]);
    du_ring_fx_off();
    du_banner(was_live ? du_result_txt(result, reason) : reason == DUE_CANCEL ? "Duel cancelled" : "", DU_OVER_MS, now);
    DU.cool_until = now + DU_COOL_MS;
    if (du_fake()) DU.fk.on = DU.fake_mode != 0, DU.fk.life = DU.fk.max;  // (the pretend friend stays for the next try)
}
// a refusal / no answer before the countdown: back to idle with a short banner
static void du_drop(const char *txt, uint64_t now) {
    if (txt && txt[0]) du_banner(txt, 2500, now);
    du_sm_reset(&DU.sm), DU.prompt = 0, DU.cool_until = now + DU_COOL_MS;
    DU.k = -1;
}

// ---- asking / answering ----
static void du_ask(int k, uint64_t now) {
    if (du_sm_busy(&DU.sm)) { LOG("[duel] ask refused: %s\n", DUG_WHY[DUG_BUSY]); return; }
    bool fake = k == DU_FAKE_K;
    if (!fake && (k < 0 || k >= NET_MAX_PEERS || !net_duel_slot_ok(k))) { LOG("[duel] ask refused: no friend in slot %d\n", k + 1); return; }
    int gw = du_gate_now(now, true, false);  // (s273 review M3: the asker passes the same gate as the friend)
    if (gw != DUG_OK) { sora_sfx_event("error", 1.0f); LOG("[duel] ask refused here: %s\n", DUG_WHY[gw]); return; }
    du_sm_ask(&DU.sm, net_duel_nonce(), now);
    DU.sm.limit_ms = (uint32_t)g_du_limit_s * 1000u;
    DU.k = k, DU.player = fake ? 0 : NET.slot[k].p.player, DU.r = g_du_ring_m;
    snprintf(DU.name, sizeof DU.name, "%s", fake ? "Pretend" : NET.slot[k].p.name);
    DU.prompt = 0;
    if (!fake) {
        NetDuelMsg m = {.op = NDO_ASK, .id = DU.sm.id};
        du_send(&m, now);
    }
    sora_sfx_event("decide", 1.0f);
    LOG("[duel] asked %s for a duel (duel %08x; the answer waits %d s)\n", du_who(), (unsigned)DU.sm.id, (DU_ACCEPT_MS + DU_ASK_SLACK_MS) / 1000);
}
// the ring's centre: between the two now, on the floor under it (a ray down; none = the middle height)
static void du_centre_now(const float him[3], float c[3]) {
    float me[3] = {g_body[0], g_body[1], g_body[2]};
    du_mid(me, him, c);
    float a[3] = {c[0], c[1] + 3.0f, c[2]}, b[3] = {c[0], c[1] - 10.0f, c[2]}, h[3] = {0}, n[3] = {0};
    if (linkpos_raycast(a, b, h, n) && n[1] > 0.5f) c[1] = h[1] + 0.05f;
}
static void du_accept(uint64_t now) {
    if (DU.sm.phase != DU_ASKED) return;
    int why = du_gate_now(now, true, false);
    if (why != DUG_OK) { sora_sfx_event("error", 1.0f); LOG("[duel] Accept refused here: %s\n", DUG_WHY[why]); return; }
    float him[3];
    if (!du_opp_feet(him, now)) him[0] = g_body[0], him[1] = g_body[1], him[2] = g_body[2];  // (no pose of him here: the ring on us)
    du_centre_now(him, DU.c);
    DU.r = g_du_ring_m;
    if (!du_fake()) {
        NetDuelMsg m = {.op = NDO_ANSWER, .id = DU.sm.id, .ans = DUA_ACCEPT, .radius_cm = (uint16_t)(DU.r * 100)};
        memcpy(m.c, DU.c, 12);
        du_send(&m, now);
    }
    DU.prompt = 0;
    du_banner("", 0, now);  // (review L15: the "wants to duel" banner goes)
    du_sm_count(&DU.sm, now);
    du_go_live(now);
}
static void du_decline(int ans, int why, const char *log_why, uint64_t now) {
    if (DU.sm.phase != DU_ASKED) return;
    NetDuelMsg m = {.op = NDO_ANSWER, .id = DU.sm.id, .ans = (uint8_t)ans, .why = (uint8_t)why};
    du_send(&m, now);
    LOG("[duel] %s's duel declined: %s\n", du_who(), log_why);
    du_banner("", 0, now);  // (review L15)
    du_drop("", now);
}

// ---- hits: the victim's side ----
static void du_take_hit(const NetDuelMsg *m, uint64_t now) {
    if (DU.sm.phase != DU_FIGHT) return;
    int life = linkpos_link_life();
    if (life <= 0) return;
    float p[3] = {g_body[0], g_body[1], g_body[2]}, to_u = 0.01f * g_k;
    bool front = du_from_front(p, S.face, m->from);
    const SlLevel *lv = &SL_LV[sl_lv() - 1];
    int v = 0, dmg = du_dmg_scale(du_dmg(m->stat, lv->def, m->power, m->boost, sl_khdiff, &v), g_du_dmg_pct);
    // blocked the way KH2 blocks a hit on Sora here (sora_guard.h guard_frame's "want"): the Guard window facing the attacker, the
    // post-block / Reaction-Dodge invulnerable frames, Reflect's barrier, a party Limit (refused in a duel anyway)
    // (s284, s273 review L9: Guard's window / the invulnerable frames / Reflect only count down on driven frames: a paused
    //  player takes the hit, or one block + the pause menu would block everything until the time judge)
    // (s288 port) KH2's own Guard window (guard_window_kh: gparry's longer one is for BotW's hits only); the Aerial Dodge
    // block (adblock) and the Dodge Roll's frames (rollinv, in g_rc_inv) count too: the user's ruling 2026-11-25
    bool paused = *(volatile uint32_t *)(uintptr_t)PAUSE_STATE != 0;
    int why = du_block_why(paused, g_guard && guard_window_kh() && front, mg_reflect_up(), G.inv > 0 || g_rc_inv > 0, ad_window(),
                           pl_invuln());
    if (why != DUB_NONE) {
        if (why == DUB_ADODGE) {  // (the dodge goes on: spark + clang, no reflect motion; no BotW parry hold to ride out here)
            uint64_t hold = g_ad_hold_ms;
            ad_block(p, to_u, dmg);
            g_ad_hold_ms = hold;
        } else guard_block(p, to_u, dmg, "duel hit", why == DUB_GUARD);
        DU.nblocked++;
        if (g_duel_log) LOG("[duel] %s's hit (%d) blocked: %s\n", du_who(), m->hseq, du_block_name(why));
        return;
    }
    if (g_duel_log && g_guard && S.special == SP_GUARD)  // (s285: why a hit during our Guard was not blocked)
        LOG("[duel] hit #%u during Guard not blocked: window %d, front %d (from %+.2f %+.2f of us, face %.1f), paused %d\n", m->hseq,
            (int)guard_window_kh(), (int)front, m->from[0] - p[0], m->from[2] - p[2], S.face * 180 / PI, (int)paused);
    bool ko = false;
    int after = du_life_after(life, dmg, &ko);
    if (!du_life_set(after)) { LOG("[duel] hit by %s: the life write was refused (life %d)\n", du_who(), life); return; }
    DU.own_write_ms = now, DU.front_at = now, memcpy(DU.front_from, m->from, 12);
    if (DU.life_ok >= 0) DU.life_ok = after;
    DU.nhits_taken++;
    if (m->atkp) sora_sfx_hit(m->atkp, 1.0f, p);
    LOG("[duel] hit by %s (%s%s, #%u): S %u - Def %d = V %d x Power %u%s%s = %d -> life %d -> %d%s\n", du_who(),
        m->src == 0 ? "blade" : m->src == 1 ? "Fire" : m->src == 2 ? "Blizzard" : "Thunder", m->flags & NDH_FINISH ? " finisher" : "", m->hseq, m->stat,
        lv->def, v, m->power, m->boost ? " +boost" : "", sl_khdiff != 1 ? " x difficulty" : "", dmg, life, after, ko ? ": KO" : "");
    if (g_du_dmg_pct != 100) LOG("[duel]   (dueldmg %d %%: the damage above is already scaled)\n", g_du_dmg_pct);
    if (g_du_rag && (m->flags & NDH_FINISH)) {  // (s286) a finisher knocks us down, KO or not
        bool ok = du_knock();
        LOG("[duel] finisher #%u knocks you down%s\n", m->hseq, ok ? "" : ": not in this exe");
    }
    if (ko) du_end(DUR_LOSE, DUE_KO, true, now);
}

// ---- hits: the attacker's side ----
// the pretend friend takes it with our own Defense / difficulty (GUESS: as if he were us)
static void du_fake_take(const NetDuelMsg *m, uint64_t now) {
    int v = 0, dmg = du_dmg_scale(du_dmg(m->stat, SL_LV[sl_lv() - 1].def, m->power, m->boost, sl_khdiff, &v), g_du_dmg_pct);
    bool ko = false;
    int before = DU.fk.life;
    DU.fk.life = du_life_after(DU.fk.life, dmg, &ko);
    LOG("[duel] fake: your hit (%s) S %u - Def %d = V %d x Power %u = %d: its life %d -> %d / %d%s\n", m->src ? "spell" : "blade", m->stat,
        SL_LV[sl_lv() - 1].def, v, m->power, dmg, before, DU.fk.life, DU.fk.max, ko ? ": KO" : "");
    if (ko) du_end(DUR_WIN, DUE_KO, false, now);
}
// (s285) where our current swing started (sora.c, each attack start): a blade hit's "from" (du_hit_out)
static float g_du_swing_from[3];
static bool g_du_swing_set;
static void du_swing_start(const float p[3]) { memcpy(g_du_swing_from, p, sizeof g_du_swing_from), g_du_swing_set = true; }
static void du_hit_out(NetDuelMsg *m, const float at[3], uint64_t now) {
    m->op = NDO_HIT, m->id = DU.sm.id, m->hseq = ++DU.hseq;
    m->from[0] = g_body[0], m->from[1] = g_body[1], m->from[2] = g_body[2];
    if (m->src == 0 && g_du_swing_set) {
        // (s285, two-copy check: a Guard never blocked a Keyblade hit) the swing's lunge ends ~2 cm from (or past) the victim's
        // centre: two Soras have no collision, KH2's enemies do. du_from_front on our body there is a coin flip, and our facing
        // then is not reliable either (s285 logs: still the old way, or his). So a blade hit comes "from" where this swing
        // STARTED (sora.c's attack start -> du_swing_start), 1.5 m off in the logs. Spells keep the caster's body (no lunge).
        m->from[0] = g_du_swing_from[0], m->from[2] = g_du_swing_from[2];
    }
    memcpy(m->at, at, 12);
    DU.nhits_dealt++;
    if (g_duel_log)
        LOG("[duel] HIT %s (#%u, %s, Power %u, S %u%s) at %.2f %.2f %.2f\n", du_who(), m->hseq, m->src == 0 ? "blade" : "spell", m->power, m->stat,
            m->boost ? ", boost" : "", at[0], at[1], at[2]);
    if (du_fake()) du_fake_take(m, now);
    else du_send(m, now);
}
// sora.c blade_hits_one, once per live window after the enemies: our blade window against the opponent's body. -> 1 = a hit
static int du_blade(int blade, const HitWin *win, int w, const float prev[6], const float now6[6], const float feet[3], float to_u) {
    if (!g_duel || DU.sm.phase != DU_FIGHT || !win || !win->power) return 0;
    if (DU.hit_swing == S.swing && DU.hit_win == w && DU.hit_blade == blade) return 0;  // (one hit per window, as an enemy)
    uint64_t now = GetTickCount64();
    float e[3];
    if (!du_opp_feet(e, now)) return 0;
    float hx = e[0] - feet[0], hz = e[2] - feet[2], wrad = win->pad[0] * 2.0f * to_u;
    if (hx * hx + hz * hz > (6 + wrad) * (6 + wrad)) return 0;
    if (wall_hides(e)) return 0;
    float ep[3] = {e[0], e[1] + DU_BODY_Y0_CM * to_u, e[2]};
    if (!kbreach_hit(blade, S.swing, win, DU_ACTOR_TAG, prev, now6, ep, DU_BODY_R_CM * to_u, DU_BODY_H_CM * to_u, wrad, to_u, blade == 0 && g_wpn_cur.def))
        return 0;
    DU.hit_swing = S.swing, DU.hit_win = w, DU.hit_blade = blade;
    const SlLevel *lv = &SL_LV[sl_lv() - 1];
    NetDuelMsg m = {.power = win->power, .src = 0, .react = win->reaction, .atkp = win->atkp};
    m.stat = (uint16_t)(sl_str_of(lv) + SL_KEY_STR + (berserk_on() ? 2 : 0));  // (KH2 S: Str + Keyblade Str, +2 Berserk Charge; GUESS: a BotW weapon in hand counts as the Keyblade)
    int b = ab_finisher_dmg(win->kind, 1000);  // (Combo Boost / Air Combo Boost as a percent over 100)
    m.boost = (uint16_t)(b > 1000 ? (b - 1000 + 9) / 10 : 0);
    if (win->kind != ATKP_KIND_NORMAL) m.flags |= NDH_FINISH;
    const float *at = g_spark_at ? g_sweep_hit : now6 + 3;
    sora_fx_event(kbfx_name(win->kind != ATKP_KIND_NORMAL ? "hit_finish" : "hit"), at, S.face, to_u);
    kbse_hit(win->atkp, 1.0f, e);
    S.hit = true;  // (the combo connected, as on an enemy)
    du_hit_out(&m, at, now);
    return 1;
}
// sora.c spell_tick: Fire's tick / Blizzard's shard / Thunder's burst against the opponent: a cylinder r around c, his feet
// between lo and hi above c (BotW units). kind = SPELL_FIRE .. SPELL_THUNDER. -> true = a hit (the shard ends)
static bool du_spell(int kind, int power, const float c[3], float r, float lo, float hi) {
    if (!g_duel || DU.sm.phase != DU_FIGHT || kind < 0 || kind > 2) return false;
    uint64_t now = GetTickCount64();
    float e[3];
    if (!du_opp_feet(e, now) || !du_body_near(c, e, r, lo, hi)) return false;
    const SlLevel *lv = &SL_LV[sl_lv() - 1];
    static const int ROW[3] = {502, 248, 250};  // (the spells' atkp rows, as sora.c spell_hit's Drive keys: their hit sounds)
    NetDuelMsg m = {.power = (uint16_t)power, .src = (uint8_t)(kind + 1), .flags = NDH_SPELL, .react = 3, .atkp = (uint16_t)ROW[kind]};
    m.stat = (uint16_t)(sl_mag_of(lv) + SL_KEY_MAG);  // (KH2: Mag + Keyblade Mag)
    m.boost = ab_on(AB.on, AB_FIRE_BOOST + kind) ? AB_ELEMENT_UP : 0;  // (Fire / Blizzard / Thunder Boost)
    float to_u = 0.01f * g_k, at[3] = {e[0], e[1] + 0.9f * g_k, e[2]};
    static const char *const FX[3] = {"fire_hit", "blizzard_hit", "thunder_hit"};
    sora_fx_event(FX[kind], at, 0, to_u);
    du_hit_out(&m, at, now);
    return true;
}
// (s275) sora.c targets_build: the opponent's stand-in target while the FIGHT runs (his drawn feet; HP % from his SNAP, the
// pretend friend's own life). Only DU_ACTOR_TAG leaves here: every g_lock.actor reader outside sora.c's Target list compares
// it against BotW's enemy list (no match), none dereferences it.
static bool du_target(uint64_t *tag, float feet[3], int32_t *life_pct) {
    if (!g_du_lock || !g_duel || DU.sm.phase != DU_FIGHT || !du_opp_feet(feet, GetTickCount64())) return false;
    int pct = du_fake() ? du_pm(DU.fk.life, DU.fk.max) / 10 : du_peer_hp(DU.k);
    *tag = DU_ACTOR_TAG, *life_pct = pct < 1 ? 1 : pct > 100 ? 100 : pct;
    return true;
}
// Thunder with no enemy target: aimed at the opponent when he is within KH2's LockOnRange and ahead (GUESS: KH2's Thunder takes
// the battle target; here the friend is the only one)
static void du_thunder_aim(uint64_t target, const float pos[3], const float dir[2], float at[3]) {
    if (!g_duel || DU.sm.phase != DU_FIGHT || (target && target != DU_ACTOR_TAG)) return;  // (s275: a lock on him = no enemy target)
    float e[3];
    if (!du_opp_feet(e, GetTickCount64())) return;
    float dx = e[0] - pos[0], dz = e[2] - pos[2], R = KH_LOCK_RANGE * 0.01f * g_k;
    if (dx * dx + dz * dz <= R * R && dx * dir[0] + dz * dir[1] >= 0) memcpy(at, e, 12);
}
// sora.c's flinch: face the attacker of a fresh duel hit (true = *front set)
static bool du_front(const float p[3], float face, bool *front) {
    if (!g_duel || !DU.front_at || GetTickCount64() > DU.front_at + DU_FRONT_MS) return false;
    *front = du_from_front(p, face, DU.front_from);
    return true;
}
// sora.c after menu_tick: the commands a duel refuses (Drive Forms, Limits, party Limits, summons) -> -1
static int du_cast_gate(int cast) {
    if (!g_duel || !DUS.live || cast < 0) return cast;
    bool no = (cast >= CMD_FORM && cast < CMD_FORM + FM_NFORMS) || (cast >= CMD_LIMIT && cast < CMD_LIMIT + LM_N) ||
              (cast >= CMD_PLIMIT && cast < CMD_PLIMIT + PL_NLIM) || (cast >= CMD_SUMMON_CAST && cast <= CMD_SUMMON_FPS);
    if (!no) return cast;
    sora_sfx_event("error", 1.0f);
    LOG("[duel] command %d refused in a duel (no Drive Forms / Limits / summons)\n", cast);
    return -1;
}

// ---- the packets ----
static void du_on_msg(int k, const NetDuelMsg *m, uint64_t now) {
    bool mine = DU.sm.phase != DU_IDLE && m->id == DU.sm.id && k == DU.k;
    if (g_duel_log && m->op != NDO_RING) LOG("[duel] net: %s from slot %d (duel %08x%s)\n", NDO_NAME[m->op], k + 1, (unsigned)m->id, mine ? "" : ", not ours");
    switch (m->op) {
    case NDO_ASK: {
        if (DU.sm.phase == DU_ASKED && m->id == DU.sm.id) return;  // (a copy)
        int why = du_gate_now(now, false, du_sm_busy(&DU.sm));
        if (why == DUG_OK) {  // (s284, s273 review L6: an ASK from a friend not drawn near us = too far / another map)
            float him[3], me[3] = {g_body[0], g_body[1], g_body[2]};
            if (!du_peer_feet(k, him, now) || du_hdist(him, me) > 3 * g_du_near_m || fabsf(him[1] - me[1]) > 20.0f) why = DUG_FAR;
        }
        if (why != DUG_OK) {
            NetDuelMsg r = {.op = NDO_ANSWER, .id = m->id, .ans = DUA_BUSY, .why = (uint8_t)why};
            net_duel_send(k, &r, now);
            if (net_lograte_ok(&NETD.lr, now, NULL)) LOG("[duel] \"%s\" asked for a duel: answered busy (%s)\n", NET.slot[k].p.name, DUG_WHY[why]);  // (L6: a flood logs 8 / 2 s)
            return;
        }
        du_sm_asked(&DU.sm, m->id, now);
        DU.sm.limit_ms = (uint32_t)g_du_limit_s * 1000u;
        DU.k = k, DU.player = NET.slot[k].p.player;
        snprintf(DU.name, sizeof DU.name, "%s", NET.slot[k].p.name);
        DU.prompt = 2;
        sora_sfx_event("submenu", 1.0f);
        LOG("[duel] \"%s\" asks you for a duel: Triangle = Accept Duel (%d s)\n", DU.name, DU_ACCEPT_MS / 1000);
        char b[48];
        snprintf(b, sizeof b, "%.15s wants to duel", DU.name);
        du_banner(b, DU_ACCEPT_MS, now);
        return;
    }
    case NDO_ANSWER:
        if (!mine || DU.sm.phase != DU_ASKING) return;
        if (m->ans == DUA_ACCEPT) {
            int gw = du_gate_now(now, false, false);  // (s273 review M3: the asker is checked again at the accept: no form / summon taken into it;
                                                      //  not strict: a jump or the menu while waiting does not cancel)
            if (gw != DUG_OK) {
                NetDuelMsg e = {.op = NDO_END, .id = DU.sm.id, .reason = DUE_CANCEL, .result = DUR_CANCEL};
                du_send(&e, now);
                LOG("[duel] \"%s\" accepted, but you can't duel now: %s (cancelled)\n", DU.name, DUG_WHY[gw]);
                du_drop("Duel cancelled", now);
                return;
            }
            memcpy(DU.c, m->c, 12), DU.r = m->radius_cm >= 100 && m->radius_cm <= 3000 ? m->radius_cm / 100.0f : g_du_ring_m;  // (review L5: 1 - 30 m)
            float me[3] = {g_body[0], g_body[1], g_body[2]};
            if (du_hdist(me, DU.c) > 60.0f || fabsf(DU.c[1] - me[1]) > 20.0f) memcpy(DU.c, me, 12);  // (a centre far from us = not this place: ours)
            du_sm_count(&DU.sm, now);
            LOG("[duel] \"%s\" accepted\n", DU.name);
            du_go_live(now);
        } else {
            char b[64];
            if (m->ans == DUA_BUSY) snprintf(b, sizeof b, "%.15s can't duel now", DU.name);
            else snprintf(b, sizeof b, "%.15s declined", DU.name);
            LOG("[duel] \"%s\" %s%s%s\n", DU.name, m->ans == DUA_BUSY ? "can't duel now: " : "declined (or let it time out)",
                m->ans == DUA_BUSY && m->why < DUG_N ? DUG_WHY[m->why] : "", "");
            du_drop(b, now);
        }
        return;
    case NDO_HIT:
        if (!mine || DU.sm.phase != DU_FIGHT || !du_seen_add(&DU.seen, m->hseq)) return;
        du_take_hit(m, now);
        return;
    case NDO_END:
        if (!mine) return;
        if (m->reason >= DUE_N || m->result >= DUR_N) {  // (s273 review H1: a newer build's reason would index past DUE_NAME)
            NetDuelMsg c = *m;
            c.reason = m->reason >= DUE_N ? DUE_CANCEL : m->reason, c.result = m->result >= DUR_N ? DUR_CANCEL : m->result;
            LOG("[duel] END from slot %d with an unknown reason %u / result %u: read as cancel\n", k + 1, m->reason, m->result);
            du_on_msg(k, &c, now);
            return;
        }
        if (DU.sm.phase == DU_ASKED || DU.sm.phase == DU_ASKING) { LOG("[duel] \"%s\" called it off\n", DU.name); du_drop("Duel cancelled", now); return; }
        if (du_sm_live(&DU.sm)) { du_end(du_result_from_peer(m->reason, m->result), m->reason, false, now); return; }
        // (both of us dropped to 1 HP at once: each said "I lost" -> a draw on both)
        if (DU.sm.phase == DU_OVER && DU.sm.result == DUR_LOSE && DU.sm.reason == DUE_KO && m->reason == DUE_KO && m->result == DUR_LOSE) {
            DU.sm.result = DUR_DRAW;
            du_banner(du_result_txt(DUR_DRAW, DUE_KO), DU_OVER_MS, now);
            LOG("[duel] both KO at once: a draw\n");
        }
        return;
    case NDO_TIME:
        if (!mine || !du_sm_live(&DU.sm)) return;  // (s273 review L7: only in a live duel, permille clamped)
        du_sm_time_his(&DU.sm, m->pm > 1000 ? 1000 : m->pm);
        return;
    case NDO_RING: {
        if (m->a == NET.me || m->b == NET.me) return;  // (our own duel: we draw it from our state)
        uint8_t lo = m->a < m->b ? m->a : m->b, hi = m->a < m->b ? m->b : m->a;
        int s = -1;
        for (int i = 0; i < NET_MAX_PEERS; i++) {
            uint8_t ol = DU.other[i].a < DU.other[i].b ? DU.other[i].a : DU.other[i].b, oh = DU.other[i].a < DU.other[i].b ? DU.other[i].b : DU.other[i].a;
            if (DU.other[i].on && ol == lo && oh == hi) s = i;
        }
        if (!m->phase) {
            if (s >= 0) {
                if (DU.other[s].fx >= 0 && fx_mine(DU.other[s].fx, DU.other[s].gen)) sora_fx_kill(DU.other[s].fx);
                DU.other[s].on = false, DU.other[s].fx = -1;
                if (g_duel_log) LOG("[duel] the duel of players %u and %u is over (their ring is gone)\n", lo, hi);
            }
            return;
        }
        for (int i = 0; i < NET_MAX_PEERS && s < 0; i++) if (!DU.other[i].on) s = i, DU.other[i].fx = -1;
        if (s < 0) return;
        if (!DU.other[s].on && g_duel_log) LOG("[duel] players %u and %u duel: their ring at %.2f %.2f %.2f (you can't hit or be hit there)\n", lo, hi, m->c[0], m->c[1], m->c[2]);
        DU.other[s].on = true, DU.other[s].a = m->a, DU.other[s].b = m->b, DU.other[s].at = now;
        memcpy(DU.other[s].c, m->c, 12), DU.other[s].r = m->radius_cm >= 100 && m->radius_cm <= 3000 ? m->radius_cm / 100.0f : DU_RING_M;  // (review L5)
        return;
    }
    }
}

// ---- the ring fx (KH2's Cure ring scaled, as the save points: user FYI "ring look = scaled Cure ring") ----
static void du_ring_fx_keep(int *fx, uint32_t *gen, const float c[3], float r) {
    bool alive = *fx >= 0 && fx_mine(*fx, *gen) && sora_fx_alive(*fx);
    if (alive) return;
    int id = sora_fx_id("cure_ring");
    // the ring's size: duelringpct at 10 m, scaled with the radius (GUESS: the Cure ring is ~4 m at 100 %)
    float sc = 0.01f * g_k * (float)g_du_ring_pct / 100.0f * (r / DU_RING_FX_REF_M);
    int h = id >= 0 ? sora_fx_spawn(id, c, 0, sc) : -1;
    *fx = h, *gen = h >= 0 && h < 32 ? g_fx_gen[h] : 0;
}

// ---- the screen text (sora_hud_notice: one slot; kairi-diag's notice wins, the warp note gives way while ours shows) ----
static void du_text(CsOut *o, const char *txt, float cx, float y, float scale, float alpha, bool box) {
    uint8_t m[64];
    int n = 0;
    for (int i = 0; txt[i] && n < 63; i++) {
        uint8_t b = txt[i] == ' ' ? 0x01 : cs_ascii(&CAMP.seq, txt[i]);
        if (b) m[n++] = b;
    }
    if (!n || o->n + n + 1 > o->cap) return;
    float w = 0, h = 0;
    cs_text(&CAMP.seq, NULL, m, n, 0, 0, scale, NULL, &w, &h);
    float x0 = cx - w * 0.5f;
    if (box) {
        SoraHudQuad *b = &o->q[o->n++];
        memset(b, 0, sizeof *b);
        const float bx0 = x0 - 6, bx1 = x0 + w + 6, by0 = y - 3, by1 = y + h + 2;
        b->x[0] = bx0, b->y[0] = by0, b->x[1] = bx1, b->y[1] = by0, b->x[2] = bx0, b->y[2] = by1, b->x[3] = bx1, b->y[3] = by1;
        uint32_t ba = (uint32_t)(0x60 * alpha + 0.5f);
        for (int k = 0; k < 4; k++) b->col[k] = ba << 24;
        b->tex = 2;
    }
    const float tc[4] = {1, 1, 1, alpha};
    cs_text(&CAMP.seq, o, m, n, x0, y, scale, tc, NULL, NULL);
}
static void du_draw(uint64_t now) {
    DU.nq = 0;
    if (!DU.notice) DU.notice = (void (*)(const SoraHudQuad *, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_notice");
    bool want = false;
    if (DU.notice && !KDG.shown_kind) {
        if (!CAMP.tried) camp_load();
        if (CAMP.ok) {
            CsOut o = {DU.q, 0, DU_NQ};
            int dg = DU.sm.phase == DU_COUNT || DU.sm.phase == DU_FIGHT ? du_count_digit(DU.sm.count0, now) : -1;
            if (dg != DU.last_digit && dg >= 0) sora_sfx_event(dg ? "cursor" : "decide", 1.0f);  // GUESS: the menu's cursor / decide SEs
            DU.last_digit = dg;
            if (dg > 0) {
                char b[4] = {(char)('0' + dg), 0};
                du_text(&o, b, 256, 150, 3.0f, 1, false);  // (Borderlands-style big digits; GUESS look)
            } else if (dg == 0) du_text(&o, "Go!", 256, 150, 2.5f, 1, false);
            if (DU.sm.phase == DU_FIGHT || DU.sm.phase == DU_TIMEUP) {
                char b[16];
                du_clock_txt(du_sm_left(&DU.sm, now), b, (int)sizeof b);
                du_text(&o, b, 256, 18, 0.7f, 1, true);
            }
            if (DU.sm.phase == DU_ASKING) {
                char b[48];
                snprintf(b, sizeof b, "Duel: waiting for %.15s", du_who());
                du_text(&o, b, 256, 34, 0.5f, 1, true);
            }
            if (DU.banner_t0 && now < DU.banner_t0 + DU.banner_ms && DU.banner[0]) {
                uint64_t left = DU.banner_t0 + DU.banner_ms - now;
                float al = left >= 500 ? 1.0f : (float)left / 500.0f;
                bool big = DU.sm.phase == DU_OVER;
                du_text(&o, DU.banner, 256, big ? 150 : 34, big ? 1.2f : 0.5f, al, true);
            }
            DU.nq = o.n;
            want = o.n > 0;
        }
    }
    if (want) DU.notice(DU.q, DU.nq), DU.notice_sent = true;
    else if (DU.notice_sent) {
        if (DU.notice && !KDG.shown_kind) DU.notice(NULL, 0);
        DU.notice_sent = false;
    }
}

// ---- the pretend opponent ----
static void du_fake_place(void) {
    DU.fk.on = true, DU.fk.max = DU.fk.life = DU_FAKE_LIFE, DU.fk.next_hit = 0;
    DU.fk.feet[0] = g_body[0] + sinf(S.face) * DU_FAKE_DIST, DU.fk.feet[1] = g_body[1], DU.fk.feet[2] = g_body[2] + cosf(S.face) * DU_FAKE_DIST;
    LOG("[duel] fake: a pretend friend at %.2f %.2f %.2f (3 m ahead), %d quarter hearts\n", DU.fk.feet[0], DU.fk.feet[1], DU.fk.feet[2], DU.fk.life);
}
static void du_fake_tick(uint64_t now) {
    if (!DU.fk.on) return;
    if (now >= DU.fake_mark_at) {  // (a sparkle on its feet every 2 s: where it stands)
        DU.fake_mark_at = now + 2000;
        float m[3] = {DU.fk.feet[0], DU.fk.feet[1] + 0.9f * g_k, DU.fk.feet[2]};
        int id = sora_fx_id("cure_sparkle");
        if (id >= 0) sora_fx_spawn(id, m, 0, 0.01f * g_k);
    }
    if (!du_fake()) {
        if (DU.fake_mode == 2 && !du_sm_busy(&DU.sm) && now >= DU.cool_until) {  // (it asks you)
            if (du_gate_now(now, false, false) != DUG_OK) return;
            du_sm_asked(&DU.sm, net_duel_nonce(), now);
            DU.sm.limit_ms = (uint32_t)g_du_limit_s * 1000u;
            DU.k = DU_FAKE_K, snprintf(DU.name, sizeof DU.name, "Pretend"), DU.prompt = 2;
            sora_sfx_event("submenu", 1.0f);
            du_banner("Pretend wants to duel", DU_ACCEPT_MS, now);
            LOG("[duel] fake: the pretend friend asks you for a duel: Triangle = Accept Duel\n");
        }
        return;
    }
    if (DU.sm.phase == DU_ASKING && now >= DU.sm.t0 + DU_FAKE_ACCEPT_MS) {
        du_centre_now(DU.fk.feet, DU.c), DU.r = g_du_ring_m;
        du_sm_count(&DU.sm, now);
        LOG("[duel] fake: the pretend friend accepted\n");
        du_go_live(now);
    }
    if (DU.sm.phase == DU_FIGHT && g_du_fake_hit_ms > 0) {
        if (!DU.fk.next_hit) DU.fk.next_hit = now + (uint64_t)g_du_fake_hit_ms;
        if (now >= DU.fk.next_hit) {
            DU.fk.next_hit = now + (uint64_t)g_du_fake_hit_ms;
            const SlLevel *lv = &SL_LV[sl_lv() - 1];
            NetDuelMsg m = {.op = NDO_HIT, .id = DU.sm.id, .hseq = ++DU.fk.hseq, .power = (uint16_t)g_du_fake_pow, .stat = (uint16_t)(sl_str_of(lv) + SL_KEY_STR),
                            .atkp = 0, .react = 3};
            if (g_du_fake_fin > 0 && m.hseq % (unsigned)g_du_fake_fin == 0) m.flags |= NDH_FINISH;  // (s286: the ragdoll check)
            memcpy(m.from, DU.fk.feet, 12), m.at[0] = g_body[0], m.at[1] = g_body[1], m.at[2] = g_body[2];
            if (du_seen_add(&DU.seen, m.hseq)) du_take_hit(&m, now);
        }
    }
}

// ---- once a mod frame (sora_mod_frame, after warp_tick; game thread) ----
static void du_off_all(const char *why, uint64_t now) {
    if (du_sm_live(&DU.sm)) du_end(DUR_CANCEL, DUE_CANCEL, true, now), LOG("[duel] stopped: %s\n", why);
    else if (DU.sm.phase == DU_ASKED) du_decline(DUA_BUSY, DUG_OFF, why, now);
    else if (DU.sm.phase == DU_ASKING) { NetDuelMsg m = {.op = NDO_END, .id = DU.sm.id, .reason = DUE_CANCEL}; du_send(&m, now); du_drop("", now); }
    if (DU.heal_at) {  // (a KO hold still waiting: heal + the party now)
        DU.heal_at = 0, du_heal_now();
        if (DU.party_was) P.on = true;
        DU.party_was = false;
    }
    du_sm_reset(&DU.sm), DU.prompt = 0, DU.k = -1;
    du_ring_fx_off();
    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (DU.other[i].on && DU.other[i].fx >= 0 && fx_mine(DU.other[i].fx, DU.other[i].gen)) sora_fx_kill(DU.other[i].fx);
        DU.other[i].on = false, DU.other[i].fx = -1;
    }
    DUS.live = DUS.floor = false, DUS.eat_x_until = 0;
    net_duel_drop_all();
}
static void du_frame(uint64_t now) {
    if (!g_duel || !g_on) {
        bool other = false;  // (s284, s273 review L12: two others' ring seen here is cleaned up too)
        for (int i = 0; i < NET_MAX_PEERS; i++) other = other || DU.other[i].on;
        if (DUS.live || DUS.floor || DU.sm.phase != DU_IDLE || DU.ring_fx >= 0 || other) du_off_all(g_on ? "duel 0" : "Sora mode off", now);
        if (DU.notice_sent) du_draw(now);
        return;
    }
    net_duel_tick(now);
    int k;
    NetDuelMsg m;
    while (net_duel_pop(&k, &m)) du_on_msg(k, &m, now);
    // hits taken outside a duel (the gate's "a hit in 10 s"): a life drop we did not write
    int lh = linkpos_link_hits();
    if (DU.hits_seen >= 0 && lh != DU.hits_seen && now > DU.own_write_ms + 500) DU.last_hit_ms = now;
    DU.hits_seen = lh;
    du_fake_tick(now);
    if (DU.heal_at && now >= DU.heal_at) {  // (s273: the KO hold is over)
        int l0 = linkpos_link_life(), l1 = du_heal_now();
        DU.heal_at = 0;
        if (DU.party_was) P.on = true;
        DU.party_was = false;
        LOG("[duel] KO hold over: healed %d -> %d, MP full%s\n", l0, l1, DU.heal_ch ? " (MP Charge ended)" : "");
    }
    // the opponent gone from the room / the slot
    if (DU.sm.phase != DU_IDLE && DU.sm.phase != DU_OVER && !du_fake() && !du_slot_him(DU.k)) {
        if (du_sm_live(&DU.sm)) du_end(DUR_WIN, DUE_GONE, false, now);
        else du_drop("Your friend left", now);
    }
    switch (du_sm_step(&DU.sm, now)) {
    case DUS_ASK_TIMEOUT:
        LOG("[duel] no answer from %s (duels off there, or an older build)\n", du_who());
        {
            char b[48];
            snprintf(b, sizeof b, "No answer from %.15s", du_who());
            du_drop(b, now);
        }
        break;
    case DUS_PROMPT_TIMEOUT: du_decline(DUA_DECLINE, 0, "no answer in 10 s", now); break;
    case DUS_FIGHT: LOG("[duel] Go! (%d s)\n", g_du_limit_s); break;
    case DUS_TIMEUP: {
        uint16_t pm = du_pm(linkpos_link_life(), g_hud_life_max);
        du_sm_time_mine(&DU.sm, pm);
        if (du_fake()) du_sm_time_his(&DU.sm, du_pm(DU.fk.life, DU.fk.max));
        NetDuelMsg t = {.op = NDO_TIME, .id = DU.sm.id, .pm = pm};
        du_send(&t, now);
        LOG("[duel] time up: your HP %.1f %%\n", pm / 10.0);
        break;
    }
    case DUS_TIME_DECIDE:
        if (!DU.sm.his_time) {
            int hp = du_fake() ? du_pm(DU.fk.life, DU.fk.max) / 10 : du_peer_hp(DU.k);
            du_sm_time_his(&DU.sm, (uint16_t)(hp * 10));
            LOG("[duel] no TIME from %s: his last SNAP's HP %d %% decides\n", du_who(), hp);
        }
        break;
    case DUS_IDLE:
        DU.k = -1;
        break;
    }
    if (du_sm_time_ready(&DU.sm)) {
        int r = du_time_result(DU.sm.my_pm, DU.sm.his_pm);
        LOG("[duel] time limit: you %.1f %% vs %s %.1f %%\n", DU.sm.my_pm / 10.0, du_who(), DU.sm.his_pm / 10.0);
        // (s284, s273 review L10 + L11: END carries our judged result, so a game with a longer dueltime, or one whose TIME was
        //  lost and would read our healed SNAP, takes the same result; an END that arrives after its own end is ignored)
        du_end(r, DUE_TIME, true, now);
    }
    if (du_sm_live(&DU.sm)) {
        float me[3] = {g_body[0], g_body[1], g_body[2]};
        if (P.on) P.on = false, party_all_off(), DU.party_was = true, LOG("[duel] the party was turned on mid-duel: off again (back after)\n");  // (review L13)
        if (g_loading || TITLE.on || NET.dead) du_end(DUR_LOSE, DUE_LEFT, true, now);
        else {
            // enemies: near the ring or near me (one scan around each, alternating, 5 a second)
            static bool alt;
            static uint64_t next;
            bool scanned = false;
            if (now >= next) next = now + DU_SCAN_MS, alt = !alt, du_scan(now, alt ? DU.c : me, true), DU.scan_at = 0, scanned = true;
            if (scanned && (DU.scan_enemy_m >= 0 || DU.scan_chase_m >= 0)) {
                LOG("[duel] an enemy %s %.0f m from %s: no winner\n", DU.scan_enemy_m >= 0 ? "is" : "hunts you", DU.scan_enemy_m >= 0 ? DU.scan_enemy_m : DU.scan_chase_m,
                    alt ? "the ring" : "you");
                du_end(DUR_CANCEL, DUE_ENEMY, true, now);
            }
        }
        // (s286) a finisher's knock-down may throw us across the line: the ring-out clock waits while BotW tumbles us
        bool tumbling = g_du_rag && linkpos_link_ragdoll() == 1;
        if (du_sm_live(&DU.sm) && DU.sm.phase != DU_COUNT && du_out_step(&DU.out, !tumbling && !du_inside(DU.c, DU.r, me), now)) {
            LOG("[duel] you left the ring (%.1f m from its centre, radius %.1f): forfeit\n", du_hdist(me, DU.c), DU.r);
            du_end(DUR_LOSE, DUE_FORFEIT, true, now);
        }
        if (du_sm_live(&DU.sm) && !du_fake()) {
            // (s273 review M2: the flag's own SNAP arrival time, not now: a SNAP stream that simply stops ends the duel in 3 s)
            if (du_slot_him(DU.k) && (du_peer_flags(DU.k) & NF_DUEL)) {
                const NetPeer *pp = &NET.slot[DU.k].p;
                uint64_t at = pp->n > 0 ? pp->at[pp->n - 1] : 0;
                if (at > now) at = now;
                if (at > DU.last_flag_ms) DU.last_flag_ms = at;
            }
            if (du_peer_gone(DU.sm.count0, DU.last_flag_ms, now)) {
                LOG("[duel] %s's game shows no duel for %d s: it left (you win)\n", du_who(), DU_PEER_MS / 1000);
                du_end(DUR_WIN, DUE_GONE, true, now);
            }
        }
        // (no items: a meal from BotW's own menu while paused is put back; Cure / the floor are ours and run unpaused)
        if (du_sm_live(&DU.sm) && DU.sm.phase != DU_COUNT) {
            bool paused = *(volatile uint32_t *)(uintptr_t)PAUSE_STATE != 0;
            int life = linkpos_link_life();
            if (paused) DU.paused_seen = true;
            else {
                if (DU.paused_seen && DU.life_ok > 0 && life > DU.life_ok && du_life_set(DU.life_ok))
                    LOG("[duel] a heal from BotW's menu (%d -> %d) taken back: no items in a duel\n", DU.life_ok, life), DU.own_write_ms = now;
                DU.paused_seen = false;
                DU.life_ok = linkpos_link_life();
            }
        }
        if (du_sm_live(&DU.sm) && now >= DU.ring_tx) du_ring_note(DU.sm.phase == DU_COUNT ? 1 : 2, now);
    }
    DUS.live = du_sm_live(&DU.sm);
    DUS.floor = DUS.live || DU.sm.phase == DU_OVER;
    // fx: our ring, the others' rings
    if (DUS.live) du_ring_fx_keep(&DU.ring_fx, &DU.ring_gen, DU.c, DU.r);
    else du_ring_fx_off();
    for (int i = 0; i < NET_MAX_PEERS; i++) {
        if (!DU.other[i].on) continue;
        if (now > DU.other[i].at + DU_RING_STALE_MS) {
            if (DU.other[i].fx >= 0 && fx_mine(DU.other[i].fx, DU.other[i].gen)) sora_fx_kill(DU.other[i].fx);
            DU.other[i].on = false, DU.other[i].fx = -1;
            continue;
        }
        if (hypotf(DU.other[i].c[0] - g_body[0], DU.other[i].c[2] - g_body[2]) < 60.0f) du_ring_fx_keep(&DU.other[i].fx, &DU.other[i].gen, DU.other[i].c, DU.other[i].r);
    }
    du_draw(now);
}

// ---- the driving path (sora.c, before rc_tick): the two Triangle prompts. can = Sora stands free on the ground ----
static void du_input(uint64_t *press, uint64_t *held, bool can) {
    if (!g_duel || !g_on) return;
    uint64_t now = GetTickCount64();
    bool react = g_react && REACT.kind != REACT_NONE;  // (a Save / Dodge / Reflect prompt is up: it keeps Triangle)
    const char *txt = NULL;
    if (DU.sm.phase == DU_ASKED && DU.prompt == 2) {
        if (!react && can && !du_menu_up()) txt = "Accept Duel";
        float him[3];  // (he walked off: declined)
        float me[3] = {g_body[0], g_body[1], g_body[2]};
        if (!du_fake() && du_opp_feet(him, now) && du_hdist(him, me) > 3 * g_du_near_m) { du_decline(DUA_DECLINE, 0, "he walked off", now); return; }
        int why = du_gate_now(now, false, false);
        if (why != DUG_OK) { du_decline(DUA_BUSY, why, DUG_WHY[why], now); return; }
    } else if (DU.sm.phase == DU_IDLE && !react && can) {
        DU.prompt = 0;
        if (!DU.idle_since) DU.idle_since = now;
        // (Triangle is BotW's A too: "Duel" only after standing still DU_IDLE_MS facing the friend, so a pick-up / talk next to him
        // is not eaten while walking about; GUESS rule, the user may prefer a command-menu row = a cmd.bin rebake)
        if (now >= DU.idle_since + DU_IDLE_MS && du_gate_now(now, true, false) == DUG_OK) {
            float best = g_du_near_m, f[3], me[3] = {g_body[0], g_body[1], g_body[2]}, fw[2] = {sinf(S.face), cosf(S.face)};
            int pick = -1;
            if (DU.fake_mode == 1 && DU.fk.on && du_hdist(DU.fk.feet, me) <= best && du_facing(me, fw, DU.fk.feet)) pick = DU_FAKE_K, best = du_hdist(DU.fk.feet, me);
            for (int k = 0; k < NET_MAX_PEERS; k++) {
                if (!net_duel_slot_ok(k) || !du_peer_feet(k, f, now)) continue;
                uint16_t fl = du_peer_flags(k);
                if (fl & (NF_DEAD | NF_LOADING | NF_DUEL)) continue;
                float d = du_hdist(f, me);
                if (d <= best && fabsf(f[1] - me[1]) < 3.0f && du_facing(me, fw, f)) best = d, pick = k;
            }
            if (pick != -1) DU.prompt = 1, DU.prompt_k = pick, txt = "Duel";
        }
    }
    if (!can) DU.idle_since = 0;
    if (!txt) return;
    DUS.eat_x_until = now + 150;  // (pad_tap: BotW does not get this Triangle as its A)
    if (*press & BTN_CLIMB) {
        *press &= ~(uint64_t)BTN_CLIMB, *held &= ~(uint64_t)BTN_CLIMB;
        if (DU.prompt == 1) du_ask(DU.prompt_k, now);
        else du_accept(now);
        return;
    }
    *held &= ~(uint64_t)BTN_CLIMB;
    if (!rcs_show(txt, false)) {
        static bool said;
        if (!said) said = true, LOG("[duel] no Reaction Command slot (rcslot 0 / an old exe): the prompt is not drawn, Triangle still works\n");
    }
}

// ---- live ----
static bool du_live_key(const char *key, int v) {
    uint64_t now = GetTickCount64();
    if (!strcmp(key, "duel")) {
        g_duel = v != 0;
        LOG("[duel] duel %d (%s)\n", g_duel, g_duel ? "a Triangle \"Duel\" near a friend, his \"Accept Duel\", the ring, KH2 damage" : "off: no prompt, no answer, no hits");
    } else if (!strcmp(key, "duellog")) g_duel_log = v != 0, LOG("[duel] duellog %d\n", g_duel_log);
    else if (!strcmp(key, "duelring")) g_du_ring_m = v < 3 ? 3.0f : v > 40 ? 40.0f : (float)v, LOG("[duel] ring radius %.0f m (next duel)\n", g_du_ring_m);
    else if (!strcmp(key, "duelringpct")) g_du_ring_pct = v < 5 ? 5 : v > 1000 ? 1000 : v, du_ring_fx_off(), LOG("[duel] ring fx size %d %%\n", g_du_ring_pct);
    else if (!strcmp(key, "duelnear")) g_du_near_m = v < 1 ? 1.0f : v > 30 ? 30.0f : (float)v, LOG("[duel] the Duel prompt within %.0f m\n", g_du_near_m);
    else if (!strcmp(key, "dueltime")) g_du_limit_s = v < 10 ? 10 : v > 3600 ? 3600 : v, LOG("[duel] time limit %d s (next duel)\n", g_du_limit_s);
    else if (!strcmp(key, "duelchase")) g_du_chase = v != 0, LOG("[duel] duelchase %d (an Enemy_* hunting you blocks / ends a duel)\n", g_du_chase);
    else if (!strcmp(key, "duelfakehit")) g_du_fake_hit_ms = v < 0 ? 0 : v, LOG("[duel] the pretend friend hits every %d ms (0 = never)\n", g_du_fake_hit_ms);
    else if (!strcmp(key, "duellock")) g_du_lock = v != 0, LOG("[duel] lock-on to the opponent %s\n", g_du_lock ? "ON" : "off");
    else if (!strcmp(key, "duelfakepow")) g_du_fake_pow = v < 1 ? 1 : v > 1000 ? 1000 : v, LOG("[duel] the pretend friend's Power %d\n", g_du_fake_pow);
    else if (!strcmp(key, "dueldmg")) g_du_dmg_pct = v < 1 ? 1 : v > 1000 ? 1000 : v, LOG("[duel] damage you take in a duel x %d %%\n", g_du_dmg_pct);
    else if (!strcmp(key, "duelrag")) g_du_rag = v != 0, LOG("[duel] duelrag %d (a finisher on you %s)\n", g_du_rag, g_du_rag ? "knocks you down" : "is a normal hit");
    else if (!strcmp(key, "duelfakefin")) g_du_fake_fin = v < 0 ? 0 : v, LOG("[duel] the pretend friend's every %d-th hit is a finisher (0 = none)\n", g_du_fake_fin);
    else if (!strcmp(key, "duelfake")) {
        if (du_fake() && du_sm_busy(&DU.sm)) du_off_all("duelfake changed", now);
        DU.fake_mode = v < 0 ? 0 : v > 2 ? 2 : v;
        if (DU.fake_mode) du_fake_place();
        else DU.fk.on = false;
        LOG("[duel] duelfake %d (%s)\n", DU.fake_mode, DU.fake_mode == 1 ? "walk up to it: Triangle = Duel" : DU.fake_mode == 2 ? "it asks you: Triangle = Accept Duel" : "off");
    } else return false;
    return true;
}
static bool du_live_line(const char *ln) {
    if (strncmp(ln, "duel ", 5)) return false;
    uint64_t now = GetTickCount64();
    const char *a = ln + 5;
    int s;
    if (!g_duel) LOG("[duel] live: %s ignored: duel is 0 (set duel 1)\n", ln);
    else if (sscanf(a, "ask %d", &s) == 1) {
        if (s == 0 && DU.fake_mode) du_ask(DU_FAKE_K, now);
        else du_ask(s - 1, now);
    } else if (!strcmp(a, "lock")) g_du_r1_once = true, LOG("[duel] live: one R1 press (lock-on / release)\n");
    else if (!strcmp(a, "knock")) LOG("[duel] live: knock-down %s\n", du_knock() ? "asked (watch [linkpos] knock)" : "not in this exe");
    else if (!strcmp(a, "accept")) du_accept(now);
    else if (!strcmp(a, "decline")) du_decline(DUA_DECLINE, 0, "live duel decline", now);
    else if (!strcmp(a, "end")) {
        if (du_sm_live(&DU.sm)) du_end(DUR_LOSE, DUE_FORFEIT, true, now);
        else if (DU.sm.phase == DU_ASKING) { NetDuelMsg m = {.op = NDO_END, .id = DU.sm.id, .reason = DUE_CANCEL}; du_send(&m, now); du_drop("Duel cancelled", now); }
        else if (DU.sm.phase == DU_ASKED) du_decline(DUA_DECLINE, 0, "live duel end", now);
    } else if (!strcmp(a, "diag")) {
        float me[3] = {g_body[0], g_body[1], g_body[2]};
        du_scan(now, me, true);
        int why = du_gate_now(now, true, du_sm_busy(&DU.sm));
        LOG("[duel] diag: phase %s (%s), %s, ring %.1f m at %.2f %.2f %.2f, gate here: %s (enemy %.0f m, hunting %.0f m), hits dealt %d taken %d blocked %d, "
            "duels %u, floor %d\n", DU_PH_NAME[DU.sm.phase], DU.sm.role == DU_ROLE_ASKER ? "asker" : DU.sm.role == DU_ROLE_FRIEND ? "friend" : "-", du_who(), DU.r, DU.c[0],
            DU.c[1], DU.c[2], DUG_WHY[why], DU.scan_enemy_m, DU.scan_chase_m, DU.nhits_dealt, DU.nhits_taken, DU.nblocked, DU.nduels, DUS.floor);
        net_duel_diag();
    } else LOG("[duel] live: \"%s\"? (duel ask <slot> | accept | decline | end | diag)\n", ln);
    return true;
}
