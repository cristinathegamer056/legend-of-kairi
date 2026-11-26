// (s269 warpcore, co-op item 4) "teleport to a friend with BotW's OWN fast-travel warp": the pure parts, no game state, host-tested
// by runtime/test/warp_test.c. The game side is runtime/sora_warp.h (sora.c), the net side runtime/sora_net_warp.h (sora_net.h).
// Facts: recon/s267/S267_HELPER_WARP.md, recon/s269/S269_HELPER_WARP.md (Ghidra 0x71... = runtime 0x70...).
//   gate     the map's own fast-travel gate (0x7100db1700.. + 0x7100e69280..), in the order the glue reads it (each later read is
//            only made when the earlier ones pass: the UI-manager reads have no null check of their own), + Link alive. The user's
//            design (s269): exactly this, no combat / horse / shrine block (vanilla warps from all of those).
//   spot     the friend's LIVE feet when he stands on the ground, else his last safe ground spot (gdt PlayerSavePos); only on
//            MainField (the Travel Medallion slot is always "MainField/WarpIcon"); inside the gdt range of WarpDLC_DestPos.
//   run      the Medallion slot's save / restore: our spot is written, the warp called, and the OLD slot put back once the arrival
//            has begun (the 0x55 load card came and went and Link is in state 0x1), never before the new stage has loaded (the
//            stage change reads the start pos: FUN_7100c48ea0 -> FUN_7100cf5dac). No autosave can run while an event runs nor in
//            the 36 frames after one (S269 report section 4), so the restored slot is what any later save writes.
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---- gate ----
enum { WG_OK = 0, WG_NO_SITES, WG_NO_CPU, WG_NO_UI, WG_PROHIBITED, WG_EVENT, WG_TRANS, WG_PAUSED, WG_LIFE_UNKNOWN, WG_DEAD, WG_N };
static const char *const WG_WHY[WG_N] = {
    "allowed", "no warp sites in this exe / game version (NEEDS EXE RELINK, 1.9.0 only)", "not on the game thread (no guest cpu)",
    "no UI manager yet (FUN_7100e794b0 0)", "WarpProhibition / WarpProhibitionArea* set (FUN_7100d394c0)",
    "an event is running (FUN_7100e89ae0)", "a UI screen transition is up (FUN_7100e89a20)", "the game is paused (FUN_7100d467ac)",
    "CurrentHart not readable", "Link is dead (CurrentHart 0)"};
// What the glue read, in this order; a field after the first block is not read (left 0 = not blocking).
typedef struct {
    bool sites, cpu, ui, prohibited, event, trans, paused;
    bool hart_ok;   // linkpos_gdt_s32("CurrentHart") answered
    int hart;       // quarter hearts
} WgIn;
static inline int wg_gate(const WgIn *g) {
    if (!g || !g->sites) return WG_NO_SITES;
    if (!g->cpu) return WG_NO_CPU;
    if (!g->ui) return WG_NO_UI;
    if (g->prohibited) return WG_PROHIBITED;
    if (g->event) return WG_EVENT;
    if (g->trans) return WG_TRANS;
    if (g->paused) return WG_PAUSED;
    if (!g->hart_ok) return WG_LIFE_UNKNOWN;
    if (g->hart <= 0) return WG_DEAD;
    return WG_OK;
}

// ---- spot ----
// gamedata range of WarpDLC_DestPos (base gamedata, VERIFIED s269): min (-5000, -100, -4000), max (5000, 1800, 4000)
#define WSP_X 5000.0f
#define WSP_Y_LO (-100.0f)
#define WSP_Y_HI 1800.0f
#define WSP_Z 4000.0f
static inline bool wsp_pos_ok(const float p[3]) {
    return p && isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]) && fabsf(p[0]) <= WSP_X && p[1] >= WSP_Y_LO && p[1] <= WSP_Y_HI &&
           fabsf(p[2]) <= WSP_Z;
}
// degrees into [-180, 180) (a non-finite one = 0)
static inline float wsp_deg_norm(float d) {
    if (!isfinite(d)) return 0;
    double r = fmod((double)d + 180.0, 360.0);
    if (r < 0) r += 360.0;
    return (float)(r - 180.0);
}
// The game's own Medallion placement (0x7100e40cfc..0x7100e40d34, VERIFIED) writes DestDegree = PlayerSavePosAngleYDegree - 180
// (fmov s1 = 0xc3340000). We do the same for both spots (meaning of the 180: GUESS, the arrival faces the way the game would).
#define WSP_DEG_ADD (-180.0f)
enum { WSP_NONE = 0, WSP_LIVE = 1, WSP_SAVE = 2 };
static const char *const WSP_NAME[3] = {"none", "live feet (on the ground)", "last safe ground spot (PlayerSavePos)"};
// flags = NWF_* (sora_net_proto.h; repeated here as numbers so this header stands alone: 1 ground, 2 save ok, 4 MainField)
static inline int wsp_choose(uint8_t flags, const float live[3], float live_deg, const float save[3], float save_deg, float pos[3], float *deg) {
    if (!(flags & 4)) return WSP_NONE;
    if ((flags & 1) && wsp_pos_ok(live)) {
        memcpy(pos, live, 12), *deg = wsp_deg_norm(live_deg + WSP_DEG_ADD);
        return WSP_LIVE;
    }
    if ((flags & 2) && wsp_pos_ok(save)) {
        memcpy(pos, save, 12), *deg = wsp_deg_norm(save_deg + WSP_DEG_ADD);
        return WSP_SAVE;
    }
    return WSP_NONE;
}

// ---- ask (warper side, waiting for the friend's SPOT) ----
#define WA_TIMEOUT_MS 2500  // no SPOT by then = give up (an older build never answers; GUESS: a friend 300 ms away answers in 1-2 tries)
#define WA_RESEND_MS 400    // UDP: the ASK again (a Melty room event is reliable: sent once)
enum { WAA_WAIT = 0, WAA_SEND, WAA_GIVE_UP };
// -> what to do now; *next = when the next ASK goes (updated on WAA_SEND)
static inline int wa_step(uint64_t now, uint64_t t0, uint64_t *next, bool reliable) {
    if (now >= t0 + WA_TIMEOUT_MS) return WAA_GIVE_UP;
    if (now >= *next) {
        *next = reliable ? t0 + WA_TIMEOUT_MS : now + WA_RESEND_MS;
        return WAA_SEND;
    }
    return WAA_WAIT;
}

// ---- run (the Medallion slot after the warp call) ----
#define WR_START_MS 5000     // no warp-out (event bit / state 0x1) and no load this long after the call = the game did not take it
#define WR_NOLOAD_MS 15000   // the warp-out was seen, then nothing for this long and still no load = it ended without a stage change
#define WR_ARRIVE_MS 8000    // the load card is gone but no 0x1 this long after = restore anyway (the stage is loaded by then)
#define WR_MAX_MS 180000     // a run this old is put back at the first frame that is not a load
enum { WR_IDLE = 0, WR_OUT, WR_LOAD, WR_ARRIVE };
static const char *const WR_NAME[4] = {"idle", "warp-out", "loading", "arrival"};
typedef struct {
    int st;
    uint64_t t0;        // the warp call
    uint64_t t_ev;      // last frame the warp-out (event / 0x1) was seen
    uint64_t t_load;    // the load card went away
    bool ev_seen, load_seen;
    uint32_t start_ms;  // the "did not take it" window (0 = WR_START_MS); a borrow for BotW's own Travel dialog (4b) passes a longer one
                        // (the menu closes and the game unpauses before BotW calls e81a30 itself)
} WarpRun;
typedef struct {
    uint64_t now;
    bool loading;  // sora.c g_loading (screen 0x55 / the menu-load latch)
    bool event;    // FUN_7100e89ae0 (an event this frame or the last one)
    bool sw1;      // Link's state word == 0x1 (an event Link: the warp-out, the arrival)
    bool title;    // BotW's title screen (a quit: another save may load next)
} WrIn;
enum { WRA_NONE = 0, WRA_RESTORE, WRA_DROP };
static inline void wr_begin(WarpRun *r, uint64_t now) { memset(r, 0, sizeof *r), r->st = WR_OUT, r->t0 = now ? now : 1; }
static inline void wr_begin_ms(WarpRun *r, uint64_t now, uint32_t start_ms) { wr_begin(r, now), r->start_ms = start_ms; }
static inline void wr_end(WarpRun *r) { memset(r, 0, sizeof *r); }
// one frame; -> WRA_RESTORE (put the old slot back now, then wr_end), WRA_DROP (forget it, write nothing), or WRA_NONE.
// Never RESTORE while loading.
static inline int wr_step(WarpRun *r, const WrIn *in, const char **why) {
    const char *w = "";
    int a = WRA_NONE;
    if (!r || r->st == WR_IDLE || !in) return WRA_NONE;
    uint64_t now = in->now;
    if (in->title) { w = "BotW's title came up (another save may load: nothing written)"; a = WRA_DROP; goto out; }
    if (in->loading) {
        if (r->st != WR_LOAD) r->st = WR_LOAD, r->load_seen = true;
        goto out;
    }
    if (now >= r->t0 + WR_MAX_MS) { w = "timeout (180 s)"; a = WRA_RESTORE; goto out; }
    switch (r->st) {
    case WR_OUT:
        if (in->event || in->sw1) r->ev_seen = true, r->t_ev = now;
        else if (!r->ev_seen && now >= r->t0 + (r->start_ms ? r->start_ms : WR_START_MS))
            w = "no warp-out and no load in the start window (the game did not take the call)", a = WRA_RESTORE;
        else if (r->ev_seen && now >= r->t_ev + WR_NOLOAD_MS) w = "the warp-out ended without a load", a = WRA_RESTORE;
        break;
    case WR_LOAD:  // (the card is gone this frame)
        r->st = WR_ARRIVE, r->t_load = now;
        if (in->sw1) w = "arrival (load card gone, Link in state 0x1)", a = WRA_RESTORE;
        break;
    case WR_ARRIVE:
        if (in->sw1) w = "arrival (load card gone, Link in state 0x1)", a = WRA_RESTORE;
        else if (now >= r->t_load + WR_ARRIVE_MS) w = "load over, no state 0x1 within 8 s (restored anyway: the stage is loaded)", a = WRA_RESTORE;
        break;
    }
out:
    if (why) *why = w;
    return a;
}

// ---- the saved slot across a DLL hot reload (process env, like sora_party.h PPICK_ENV) ----
// "<phase> <x> <y> <z> <deg>": phase = the WarpRun state (1 out, 2 loading, 3 arrival). A new DLL copy resumes in OUT (with the
// warp-out taken as seen: it waits for the load or WR_NOLOAD_MS) or in LOAD (waits for the card to go, then the arrival rule).
#define WR_ENV "SORA_WARP_SLOT"
static inline int wr_env_fmt(char *out, int cap, int st, const float old_pos[3], float old_deg) {
    return snprintf(out, (size_t)cap, "%d %.9g %.9g %.9g %.9g", st, old_pos[0], old_pos[1], old_pos[2], old_deg);  // (s271 review L2: %.9g = exact floats)
}
// -> true = a usable saved slot (phase 1..3, finite numbers)
static inline bool wr_env_parse(const char *s, int *st, float old_pos[3], float *old_deg) {
    if (!s) return false;
    int p = 0;
    float x, y, z, d;
    if (sscanf(s, "%d %f %f %f %f", &p, &x, &y, &z, &d) != 5 || p < WR_OUT || p > WR_ARRIVE) return false;
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(d)) return false;
    *st = p, old_pos[0] = x, old_pos[1] = y, old_pos[2] = z, *old_deg = d;
    return true;
}
// resume a run from a saved phase (now = this copy's first frame)
static inline void wr_resume(WarpRun *r, int phase, uint64_t now) {
    wr_begin(r, now);
    if (phase == WR_OUT) r->ev_seen = true, r->t_ev = r->t0;
    else r->st = WR_LOAD, r->load_seen = true;
}

// ---- the friend's note ("<name> warped to you") ----
#define WN_SHOW_MS 4000
#define WN_FADE_MS 600
// alpha 0..1 of a note shown since t0 (0 = gone)
static inline float wn_alpha(uint64_t now, uint64_t t0) {
    if (!t0 || now < t0 || now >= t0 + WN_SHOW_MS) return 0;
    uint64_t left = t0 + WN_SHOW_MS - now;
    return left >= WN_FADE_MS ? 1.0f : (float)left / (float)WN_FADE_MS;
}
