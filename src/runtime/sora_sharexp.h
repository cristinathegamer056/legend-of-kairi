// (s270, co-op item 5) SHARED EXP: a friend's kill near you pays YOU too (his design s267: full EXP for each player, "near" =
// near the FRIEND who killed it, shared kills also give Drive orbs + Wisdom / Master / Final EXP). Design = recon/s267/
// S267_HELPER_EXP_REVIVE.md sec 5.3. Each game pays only its own player, with its own numbers (own Sora LV curve, own Experience
// Boost, Anti Form's No Experience still wins): no packet, no wire change, no double count across games.
//   1. note (sora_net_edmg.h net_edmg_recv): slot k's live NET_EDMG kill that was queued here (r == 1 = the enemy is loaded
//      here) while I am alive and within sharexpr m of HIS feet (his last SNAP) on the same map -> a row {hid, peer, time}.
//      Kill sync's catch-up (net_kills_scan) calls the exe apply directly, never net_edmg_recv: catch-up kills never note.
//   2. pay (sora_levels.h sl_died_one): this game's own die hook for an enemy that is NOT on Sora's paid credit list: its HashId
//      (sx_hid = sora_airhold.h ah_actor_hid, set by sora.c each frame) matches a fresh row -> sl_credit + sl_kill (EXP, kill
//      count, Drive orbs, Wisdom / Master / Final). One payment per enemy per game: a credit Sora earned herself pays the normal
//      way (the row then just expires).
// Live: `set sharexp 0|1` (default OFF = exactly today: no rows, no lookups), `set sharexpr <m>` (50, GUESS), `set sharexpttl
// <ms>` (10000, GUESS: the remote hit's ride + the death animation), `set sharexplog 0|1` (the note / far lines).
// Pure (no game headers): host test runtime/test/sharexp_test.c.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define SX_ROWS 16
static bool g_sharexp = true;        // live sharexp (s273: default ON, the two-copy run passed: friend-only kills paid once, "both hit" not twice)
static int g_sharexp_r = 50;         // live sharexpr: metres from the friend who killed it (GUESS)
static int g_sharexp_ttl = 10000;    // live sharexpttl: ms a noted kill waits for this game's die hook (GUESS)
static bool g_sharexp_log;           // live sharexplog
static uint32_t (*sx_hid)(uint64_t actor);  // (sora.c sets it each frame to ah_actor_hid; NULL = nothing pays, e.g. host tests)

typedef struct { uint32_t hid; uint64_t at; float dist; uint8_t peer; bool used; } SxRow;
static struct { SxRow r[SX_ROWS]; uint32_t noted, nfar, paid, expired; } SX;

// distance between my feet and the killer's feet; false = too far (or a number is not finite)
static bool sx_near(const float me[3], const float killer[3], float r, float *dist) {
    float dx = me[0] - killer[0], dy = me[1] - killer[1], dz = me[2] - killer[2];
    float d2 = dx * dx + dy * dy + dz * dz;
    if (!(d2 >= 0) || !(d2 < 1e12f)) return false;  // (NaN / inf)
    float d = sqrtf(d2);
    *dist = d;
    return d <= r;
}

static void sx_note(uint32_t hid, int peer, float dist, uint64_t now) {
    if (!hid) return;
    SxRow *w = NULL, *old = NULL;
    for (int i = 0; i < SX_ROWS && !w; i++) if (SX.r[i].used && SX.r[i].hid == hid) w = &SX.r[i];  // (a repeat: refresh)
    for (int i = 0; i < SX_ROWS && !w; i++)
        if (!SX.r[i].used || now - SX.r[i].at > (uint64_t)g_sharexp_ttl) w = &SX.r[i];
    for (int i = 0; i < SX_ROWS && !w; i++) if (!old || SX.r[i].at < old->at) old = &SX.r[i];
    if (!w) w = old;
    w->used = true, w->hid = hid, w->at = now, w->dist = dist, w->peer = (uint8_t)peer;
    SX.noted++;
}

// takes (and clears) the row of hid; false = none, or it is older than sharexpttl
static bool sx_take(uint32_t hid, uint64_t now, SxRow *out) {
    if (!hid) return false;
    for (int i = 0; i < SX_ROWS; i++) {
        SxRow *r = &SX.r[i];
        if (!r->used || r->hid != hid) continue;
        r->used = false;
        if (now - r->at > (uint64_t)g_sharexp_ttl) { SX.expired++; return false; }
        *out = *r;
        return true;
    }
    return false;
}

// the live keys; true = handled. lg = the caller's LOG (printf-like) so the header stays pure
static bool sx_live_set(const char *key, int v, void (*lg)(const char *, ...)) {
    if (!strcmp(key, "sharexp")) {
        g_sharexp = v != 0;
        if (!g_sharexp) memset(SX.r, 0, sizeof SX.r);
        if (lg) lg("[sora] shared EXP %s\n", g_sharexp ? "on (a friend's kill within sharexpr m of him pays you too)" : "off");
    } else if (!strcmp(key, "sharexpr")) {
        g_sharexp_r = v < 1 ? 1 : v > 1000 ? 1000 : v;
        if (lg) lg("[sora] shared EXP range %d m from the friend who killed it\n", g_sharexp_r);
    } else if (!strcmp(key, "sharexpttl")) {
        g_sharexp_ttl = v < 500 ? 500 : v > 60000 ? 60000 : v;
        if (lg) lg("[sora] shared EXP: a noted kill waits %d ms for the die hook\n", g_sharexp_ttl);
    } else if (!strcmp(key, "sharexplog")) {
        g_sharexp_log = v != 0;
        if (lg) lg("[sora] shared EXP log %s (noted %u, far %u, paid %u, expired %u)\n", g_sharexp_log ? "on" : "off",
                   SX.noted, SX.nfar, SX.paid, SX.expired);
    } else
        return false;
    return true;
}
