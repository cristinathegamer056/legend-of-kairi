// (s269 warpcore, co-op item 4) "teleport to a friend with BotW's OWN fast-travel warp (blue dissolve, as to a shrine)": the game
// side. Included once by sora.c after sora_net_minimap.h (needs S, NET, g_on, g_loading, g_body, TITLE, CAMP / cs_text, KDG,
// linkpos_link_state / _word, ACT_AIR, LOG). Pure parts: sora_warp_core.h (host test runtime/test/warp_test.c); net: sora_net_warp.h.
// Facts: recon/s267/S267_HELPER_WARP.md, recon/s269/S269_HELPER_WARP.md (Ghidra 0x71... = runtime 0x70...).
//
// How (the user's design, s269): BotW's Travel Medallion slot. Save the old gdt WarpDLC_DestPos / WarpDLC_DestDegree with the
// game's getters (FUN_7100d389f0(&out, 0), FUN_7100d389d0(0)), write ours with the NORMAL setters (FUN_7100d38a30(&pos, 0),
// FUN_7100d389e0(deg, 0): arg 0, never the debug 1 that also sets the SAVED bool IsChangedByDebug), then ONE call of
// FUN_7100e81a30("MainField/WarpIcon" rodata, "WarpPoint01" rodata, 0) = the map's own warp call -> Demo005_0 (the blue dissolve)
// -> Demo_ChangeScene -> the start-pos lookup returns DestPos for a "WarpIcon" map -> Demo005_1 arrival. The old slot goes back
// once the arrival has begun (sora_warp_core.h wr_step: load card 0x55 gone + Link in state 0x1), never before the stage loaded.
// All game calls go through the exe's sw_api (api->call on the game thread, api->site by name = the sora_sites.h warp_* rows):
// no exe logic. An exe without those rows (before s269) = "NEEDS EXE RELINK", nothing called.
//
// Allowed = exactly the map's own gate (FUN_7100e794b0 UI manager present, then !FUN_7100d394c0 WarpProhibition*, !FUN_7100e89ae0
// event, !FUN_7100e89a20 transition, !FUN_7100d467ac paused) + Link alive (gdt CurrentHart > 0) + (a friend) his spot on MainField.
// No combat / horse / shrine block (vanilla warps from those too; Demo005_0 itself takes him off a horse).
// Spot = the friend's live feet when he stands on the ground, else his gdt PlayerSavePos (his last safe ground spot).
//
// Live lines (one-shot, nothing happens without them):
//   warpto <x> <y> <z> [deg]   the game's warp to that spot (MainField coordinates; deg = WarpDLC_DestDegree as written, default 0)
//   warpfriend <slot>          ask friend <slot> (1..4, the "net diag" numbering) where he is, then warp there; he sees
//                              "<name> warped to you" when the game took the call
//   warpslot <x> <y> <z> [deg] only the first half (4b's API): borrow the slot now, NO warp call; travel to the medallion on the
//                              map by hand within 60 s (with warpicon 1 the map marker moves there too); restored after the arrival
//   warp restore               put the saved Medallion slot back now (only when a run holds one and no load is up)
//   warpnote [name]            show the friend's note here (a look check, no warp)
// Keys: "set warplog 1" = every step / packet as a [warp] line (default 0: one line per warp, its result and the restore);
//       "set warpnet 0" = no answer to a friend's ask and no note (default 1); "set warpnx <px>" / "set warpny <px>" the note's
//       place in KH2's 512 x 416 frame (x -1 = centred, default; y default 34); "set warpicon 1" (default 0, s270 note) = after a
//       restore that followed a load, FUN_7100d88704(d, &real) + FUN_7100d88670(d, 0x2a, &real) put the in-world pillar and the full
//       map's medallion marker back on the real slot (nothing for a 0,0,0 slot). warplog also logs "[warp] map" lines from the load
//       to 120 frames after the card: the marker's pos [d+0x480]+0x28, the pillar handle d+0x488 alive?, DestPos.
// A DLL hot reload while a run holds the old slot: it rides in the process env (WR_ENV), the new copy restores it.
#pragma once
#include "sw_api.h"
#include "sora_warp_core.h"

#define WARP_NOTE_SCALE 0.5f  // the sys font as kairi-diag's notice (24 px -> 12 in KH2's 416 px frame)
#define WARP_NOTE_NQ 64
#define WARP_SLOT_TEST_MS 60000  // live warpslot: 60 s to open the map and travel to the medallion by hand before the slot goes back
static int g_warp_nx = -1, g_warp_ny = 34;

enum { WS_REQ, WS_PROHIB, WS_UI, WS_UI_EVENT, WS_UI_TRANS, WS_PAUSED, WS_POS_SET, WS_DEG_SET, WS_POS_GET, WS_DEG_GET, WS_SAVEPOS, WS_SAVEANG,
       WS_STR_MAP, WS_STR_POS, WS_N, WS_MAP_MGR = WS_N, WS_MAP_ICON, WS_MAP_FX, WS_ALL };
static const char *const WS_NAME[WS_ALL] = {"warp_request", "warp_prohibited", "warp_ui_exists", "warp_ui_event", "warp_ui_trans", "warp_paused",
                                            "warp_dest_pos_set", "warp_dest_deg_set", "warp_dest_pos_get", "warp_dest_deg_get", "warp_savepos_get",
                                            "warp_saveang_get", "warp_str_map", "warp_str_pos",
                                            // (optional: the map icon / pillar probe + fix, recon/s270 sec 6; missing = only those are off)
                                            "warp_map_mgr", "warp_map_icon_set", "warp_map_medal_fx"};
// (s270 note) the full map's medallion marker + the in-world pillar (recon/s270/S270_HELPER_MAPWARP.md sec 2 / 6): d = [warp_map_mgr],
// d+0x480 the marker (its world pos at +0x28..+0x30), d+0x488 the pillar effect handle {ptr, u32 gen at +8}: alive when ptr != 0 and
// [ptr+0x20] == gen (FUN_710159f9a0, read here as memory: VERIFIED disassembly, 0x710159f9a0..b8)
#define WM_OBJ 0x480
#define WM_FX 0x488
#define WM_PROBE_FRAMES 120
static int g_warp_icon;  // live "set warpicon 1": after the restore, FUN_7100d88704(d, &real) + FUN_7100d88670(d, 0x2a, &real) (default 0)

static struct {
    bool tried, bound, hooked, env_checked, map_ok;
    const SwApi *api;
    uint64_t fn[WS_ALL];
    int probe_left;      // map probe: frames still to log after the card went (-1 = logging the load itself, 0 = off)
    char probe_last[160];
    int (*gdt_s32)(const char *, int32_t *);
    void (*notice)(const SoraHudQuad *, int);
    // a warp to start in warp_tick (from a live line or a friend's SPOT)
    bool want;
    float pos[3], deg;
    int k;            // the friend's slot (-1 = warpto)
    uint32_t nonce;   // his ask's nonce (the NOTE carries it)
    const char *what;
    // the run: the old Medallion slot while ours is in it
    WarpRun run;
    float old_pos[3], old_deg;
    float bor_pos[3], bor_deg;  // (s271) our borrowed spot (the restore checks the slot still holds it)
    bool bor_known;             // (false for a run resumed from the env after a hot reload)
    bool note_pend;             // (s271 review L3) the friend's note goes once the game TOOK the warp
    int env_phase;
    int rfail;        // restore writes refused in a row (5 = given up, logged)
    uint32_t nwarps;
    // the friend's note
    uint64_t note_t0;
    char note[48];
    SoraHudQuad q[WARP_NOTE_NQ];
    int nq;
    bool note_sent;
} WP = {.k = -1};

static bool warp_bind(void) {
    if (WP.tried) return WP.bound;
    WP.tried = true;
    HMODULE exe = GetModuleHandleA(NULL);
    const SwApi *(*get)(uint32_t) = (const SwApi *(*)(uint32_t))(void *)GetProcAddress(exe, SW_API_EXPORT);
    WP.api = get ? get(1) : NULL;
    WP.gdt_s32 = (int (*)(const char *, int32_t *))(void *)GetProcAddress(exe, "linkpos_gdt_s32");
    const SwApi *a = WP.api;
    int missing = -1;
    if (SW_HAS(a, call) && SW_HAS(a, site) && SW_HAS(a, in_frame))
        for (int i = 0; i < WS_N; i++)
            if (!(WP.fn[i] = a->site(WS_NAME[i])) && missing < 0) missing = i;
    WP.bound = SW_HAS(a, call) && SW_HAS(a, site) && SW_HAS(a, in_frame) && missing < 0 && WP.gdt_s32;
    WP.map_ok = WP.bound && SW_HAS(a, read);
    for (int i = WS_N; i < WS_ALL && WP.map_ok; i++) WP.map_ok = (WP.fn[i] = a->site(WS_NAME[i])) != 0;
    LOG("[warp] BotW's fast travel to a spot: %s\n", WP.bound ? "ready (sw_api + the warp_* sites + linkpos_gdt_s32)"
                                                  : !SW_HAS(a, call) ? "this exe has no sw_api (before s175): no warp"
                                                  : missing >= 0 ? "a warp_* site is missing (NEEDS EXE RELINK with the s269 sora_sites.h; or not 1.9.0): no warp"
                                                                 : "linkpos_gdt_s32 not in this exe: no warp");
    if (!WP.bound && missing >= 0) LOG("[warp]   first missing site: %s\n", WS_NAME[missing]);
    if (WP.bound && !WP.map_ok) LOG("[warp]   the map icon / pillar probe + fix sites are missing: warplog's [warp] map lines and warpicon are off\n");
    return WP.bound;
}

// ---- the game calls (game thread only: sw_api refuses outside linkpos_frame) ----
static bool warp_call(int f, const SwArgs *a, SwRet *r) {
    SwRet tmp;
    bool ok = WP.bound && WP.api->call(NULL, WP.fn[f], a, r ? r : &tmp);
    if (!ok && g_warp_log) LOG("[warp] guest call %s refused (not on the game thread / no function there)\n", WS_NAME[f]);
    return ok;
}
static bool warp_bool(int f, bool *out) {  // () -> bool
    SwArgs a = {0};
    SwRet r = {0};
    if (!warp_call(f, &a, &r)) return false;
    *out = (r.x0 & 1) != 0;
    return true;
}
static bool warp_vec_get(int f, float out[3]) {  // (x0 = &out, w1 = 0)
    float v[4] = {0};
    SwArgs a = {.x = {0, 0}, .nx = 2, .x_scratch = 1, .scratch = 16, .out_len = 12, .out = v};
    if (!warp_call(f, &a, NULL)) return false;
    memcpy(out, v, 12);
    return true;
}
static bool warp_f32_get(int f, float *out) {  // (w0 = 0) -> s0
    SwArgs a = {.x = {0}, .nx = 1};
    SwRet r = {0};
    if (!warp_call(f, &a, &r)) return false;
    memcpy(out, &r.s0, 4);
    return true;
}
static bool warp_dest_get(float pos[3], float *deg) { return warp_vec_get(WS_POS_GET, pos) && warp_f32_get(WS_DEG_GET, deg); }
static bool warp_dest_is(const float rp[3], float rd, const float pos[3], float deg) {
    return fabsf(rp[0] - pos[0]) < 0.01f && fabsf(rp[1] - pos[1]) < 0.01f && fabsf(rp[2] - pos[2]) < 0.01f && fabsf(rd - deg) < 0.01f;
}
// (s271 live) the restore's "still ours" test: the game itself moves DestPos a little on the warp (218.46 -> 218.48 at Kakariko),
// so within 1 m / 1 deg = ours. A medallion the player places himself lands far from the friend's spot.
static bool warp_dest_near(const float rp[3], float rd, const float pos[3], float deg) {
    float dd = fabsf(rd - deg);
    if (dd > 180) dd = 360 - dd;
    return fabsf(rp[0] - pos[0]) < 1 && fabsf(rp[1] - pos[1]) < 1 && fabsf(rp[2] - pos[2]) < 1 && dd < 1;
}
// (s271, VERIFIED recon/ghidra/out/s271_gdt1..3.c) the NORMAL write goes through the TriggerParamRef at gdt+0xbd0 into the buffer at
// [gdt+0xbd8] (FUN_7100b40970; the copy into [gdt+0xbe0] only when its "propagate" byte +0xbe9 is set), while the getter
// FUN_7100b403ec reads [gdt+0xbe0]. So a read-back on the SAME frame shows the OLD value (s271 live: "read back 0 0 0"). The setters'
// own bool (x0: FUN_7100b40930 / FUN_7100b36c80, tail-called by d38a30 / d389e0) says accepted / refused (write-locked bit 18,
// wrong handle). The new value shows in the getter after the game copies the buffers (GUESS: at the frame end): warp_check_* below.
static bool warp_dest_set(const float pos[3], float deg) {  // the NORMAL write (flag 0) -> the setters' accept bools
    float v[4] = {pos[0], pos[1], pos[2], 0};
    SwArgs a = {.x = {0, 0}, .nx = 2, .x_scratch = 1, .scratch = 16, .init_len = 12, .init = v};
    SwRet r1 = {0}, r2 = {0};
    if (!warp_call(WS_POS_SET, &a, &r1)) return false;
    SwArgs b = {.x = {0}, .nx = 1, .nv = 1};
    memcpy(&b.s[0], &deg, 4);
    if (!warp_call(WS_DEG_SET, &b, &r2)) return false;
    bool acc = (r1.x0 & 1) && (r2.x0 & 1);
    if (g_warp_log || !acc)
        LOG("[warp] Medallion slot write %.2f %.2f %.2f (%.1f deg): %s\n", pos[0], pos[1], pos[2], deg,
            acc ? "accepted (the getter shows it after the game's buffer copy)"
                : (r1.x0 & 1) ? "spot accepted, angle REFUSED by the gdt" : "REFUSED by the gdt (setter returned 0: write-locked or a bad handle)");
    return acc;
}

// ---- (s271) the later-frame check: a borrow waits until the getter shows our spot, THEN calls BotW's warp ----
#define WARP_CHECK_FRAMES 90
static struct {
    int left, n;
    bool call;  // true = call FUN_7100e81a30 once it reads (warpto / warpfriend); false = a log-only check (warpslot, the restore)
    float pos[3], deg;
    const char *what;
} WCK;
static void warp_check_start(const float pos[3], float deg, bool call, const char *what) {
    WCK.left = WARP_CHECK_FRAMES, WCK.n = 0, WCK.call = call, WCK.what = what;
    memcpy(WCK.pos, pos, 12), WCK.deg = deg;
}

// ---- (s270 note) the full map's medallion marker + the in-world pillar ----
static uint64_t warp_map_d(void) {  // the map manager (0 = none / not readable)
    uint64_t d = 0;
    if (!WP.map_ok || !WP.api->read(WP.fn[WS_MAP_MGR], &d, 8) || !WP.api->in_guest(d, WM_FX + 16)) return 0;
    return d;
}
// the marker's world pos (false = no marker) and whether the pillar's effect handle is alive (-1 = not readable)
static bool warp_map_read(uint64_t d, float icon[3], int *fx_alive) {
    uint64_t obj = 0, h[2] = {0, 0};
    *fx_alive = -1;
    if (d && WP.api->read(d + WM_FX, h, 12)) {
        uint32_t cur = 0;
        *fx_alive = h[0] && WP.api->read(h[0] + 0x20, &cur, 4) ? cur == (uint32_t)h[1] : 0;
    }
    return d && WP.api->read(d + WM_OBJ, &obj, 8) && obj && WP.api->read(obj + 0x28, icon, 12);
}
// warplog: from the load (each change while it is up) to WM_PROBE_FRAMES frames after the card went, one line a frame
static void warp_map_probe(void) {
    if (!g_warp_log || !WP.probe_left || !WP.map_ok) return;
    float icon[3] = {0}, dp[3] = {0}, dd = 0;
    int fx = -1;
    uint64_t d = warp_map_d();
    // (s271 review M2: no gdt getter call while a load is up / on the title; the marker + pillar are plain memory reads)
    bool have = warp_map_read(d, icon, &fx), dok = !g_loading && !TITLE.on && warp_dest_get(dp, &dd);
    char b[160];
    snprintf(b, sizeof b, "icon %s%.2f %.2f %.2f, pillar fx %s, DestPos %s%.2f %.2f %.2f (%.1f deg)", have ? "" : "(none) ", icon[0], icon[1], icon[2],
             fx < 0 ? "?" : fx ? "alive" : "dead", dok ? "" : "(not read) ", dp[0], dp[1], dp[2], dd);
    if (WP.probe_left < 0) {  // (the load: only changes)
        if (strcmp(b, WP.probe_last)) LOG("[warp] map (load): %s\n", b), snprintf(WP.probe_last, sizeof WP.probe_last, "%s", b);
        return;
    }
    LOG("[warp] map +%d: %s%s\n", WM_PROBE_FRAMES - WP.probe_left, b, WP.run.st == WR_IDLE ? " [slot restored]" : "");
    WP.probe_left--;
}
// warpicon: after the restore, put the pillar + the map marker on the REAL slot (the map may have re-read our spot at its first
// update on the new stage). A zero slot (no medallion placed): nothing called (hiding the marker / killing the pillar = GUESS calls)
static void warp_map_fix(const float real[3]) {
    if (!g_warp_icon) return;
    uint64_t d = warp_map_d(), obj = 0;
    if (!d || !WP.api->read(d + WM_OBJ, &obj, 8) || !obj) { LOG("[warp] warpicon: no map manager / marker now: nothing done\n"); return; }
    if (real[0] == 0 && real[1] == 0 && real[2] == 0) {
        float icon[3];
        int fx;
        warp_map_read(d, icon, &fx);
        LOG("[warp] warpicon: the real slot is 0,0,0 (no medallion placed): left as is (marker %.2f %.2f %.2f, pillar %s)\n", icon[0], icon[1], icon[2],
            fx < 0 ? "?" : fx ? "ALIVE" : "dead");
        return;
    }
    float v[4] = {real[0], real[1], real[2], 0};
    SwArgs a = {.x = {d, 0}, .nx = 2, .x_scratch = 2, .scratch = 16, .init_len = 12, .init = v};
    bool f1 = warp_call(WS_MAP_FX, &a, NULL);
    SwArgs b = {.x = {d, 0x2a, 0}, .nx = 3, .x_scratch = 4, .scratch = 16, .init_len = 12, .init = v};
    bool f2 = warp_call(WS_MAP_ICON, &b, NULL);
    LOG("[warp] warpicon: pillar (FUN_7100d88704) %s, map marker (FUN_7100d88670) %s, at %.2f %.2f %.2f\n", f1 ? "re-placed" : "REFUSED",
        f2 ? "re-placed" : "REFUSED", real[0], real[1], real[2]);
}

// ---- the hot-reload copy of the old slot ----
static void warp_env_put(int phase) {
    if (phase == WP.env_phase) return;
    WP.env_phase = phase;
    if (!phase) { SetEnvironmentVariableA(WR_ENV, NULL); return; }
    char b[96];
    wr_env_fmt(b, sizeof b, phase, WP.old_pos, WP.old_deg);
    SetEnvironmentVariableA(WR_ENV, b);
}
static void warp_env_resume(uint64_t now) {
    char b[96];
    DWORD n = GetEnvironmentVariableA(WR_ENV, b, sizeof b);
    int ph;
    if (!n || n >= sizeof b || !wr_env_parse(b, &ph, WP.old_pos, &WP.old_deg)) return;
    wr_resume(&WP.run, ph, now);
    WP.env_phase = 0, warp_env_put(WP.run.st);
    LOG("[warp] a run from the last DLL copy holds the Medallion slot (phase %s): old %.2f %.2f %.2f (%.1f deg) goes back after the arrival\n",
        WR_NAME[ph], WP.old_pos[0], WP.old_pos[1], WP.old_pos[2], WP.old_deg);
}

static void warp_restore(const char *why) {
    if (WP.run.st == WR_IDLE) return;
    if (WCK.left && WCK.call) WCK.left = 0, LOG("[warp] %s: the pending warp call is cancelled (restore: %s)\n", WCK.what ? WCK.what : "warp", why);
    // (s271 review M3) write the old slot back only while the slot holds OUR spot (or still the old one: our write not copied yet).
    // Anything else = the player placed a medallion meanwhile: keep his.
    float cp[3], cd;
    // (a run resumed after a DLL hot reload does not know our spot: always write the old one back then)
    if (WP.bor_known && warp_dest_get(cp, &cd) && !warp_dest_near(cp, cd, WP.bor_pos, WP.bor_deg) && !warp_dest_near(cp, cd, WP.old_pos, WP.old_deg)) {
        LOG("[warp] Medallion slot NOT put back (%s): it holds %.2f %.2f %.2f (%.1f deg) now, neither ours nor the old one (you placed one?): left as is\n",
            why, cp[0], cp[1], cp[2], cd);
        WP.rfail = 0, wr_end(&WP.run), warp_env_put(0);
        return;
    }
    bool ok = warp_dest_set(WP.old_pos, WP.old_deg);
    if (ok) warp_check_start(WP.old_pos, WP.old_deg, false, "restore");
    if (!ok) WP.rfail++;
    if (ok || WP.rfail == 1 || WP.rfail >= 5)
        LOG("[warp] Medallion slot put back to %.2f %.2f %.2f (%.1f deg) after %.1f s: %s (%s)\n", WP.old_pos[0], WP.old_pos[1], WP.old_pos[2], WP.old_deg,
            (GetTickCount64() - WP.run.t0) / 1000.0,
            ok ? "done" : WP.rfail >= 5 ? "FAILED 5 times, given up (the slot keeps the warp's spot)" : "FAILED (tried again next frame)", why);
    if (!ok && WP.rfail < 5) return;  // (a refused write: the run stays, wr_step asks again next frame)
    WP.rfail = 0;
    if (ok && WP.run.load_seen) warp_map_fix(WP.old_pos);  // (only after a stage change: before it the map never re-read our spot)
    wr_end(&WP.run);
    warp_env_put(0);
}

// ---- the two halves (game thread). Also for 4b (BotW's own pick: its Travel dialog calls e81a30 itself after the menu closes):
//   warp_borrow(pos, deg, what, start_ms, now)  the Medallion slot <- our spot NOW (normal 0 path, read back), the old slot saved
//                                               (process env too); the run then restores it after the arrival began. start_ms = how
//                                               long to wait for a warp-out before "not taken" (0 = 5 s; 4b: the menu close + unpause)
//   warp_call_game(what)                        our own ONE call of FUN_7100e81a30 (the live warpto / warpfriend path only)
// The gate is the caller's (warp_fire reads the map's own; BotW's dialog has already passed it for 4b). ----
static bool warp_borrow(const float pos[3], float deg, const char *what, uint32_t start_ms, uint64_t now) {
    if (WP.run.st != WR_IDLE) { LOG("[warp] %s refused: a warp is still running (%s)\n", what, WR_NAME[WP.run.st]); return false; }
    if (!warp_bind() || !WP.api->in_frame()) { LOG("[warp] %s refused: %s\n", what, WG_WHY[WP.bound ? WG_NO_CPU : WG_NO_SITES]); return false; }
    if (!wsp_pos_ok(pos)) {
        LOG("[warp] %s refused: %.2f %.2f %.2f is outside WarpDLC_DestPos's range (x +-5000, y -100..1800, z +-4000)\n", what, pos[0], pos[1], pos[2]);
        return false;
    }
    if (!warp_dest_get(WP.old_pos, &WP.old_deg)) { LOG("[warp] %s refused: the Medallion slot could not be read\n", what); return false; }
    if (g_warp_log) LOG("[warp] old Medallion slot %.2f %.2f %.2f (%.1f deg)\n", WP.old_pos[0], WP.old_pos[1], WP.old_pos[2], WP.old_deg);
    if (!warp_dest_set(pos, deg)) {
        bool back = warp_dest_set(WP.old_pos, WP.old_deg);  // (a half write: the spot may have gone in without the angle)
        LOG("[warp] %s refused: the Medallion slot took no new spot (old slot written back: %s)\n", what, back ? "accepted" : "REFUSED too");
        return false;
    }
    memcpy(WP.bor_pos, pos, 12), WP.bor_deg = deg, WP.bor_known = true;
    wr_begin_ms(&WP.run, now, start_ms);
    WP.rfail = 0;
    warp_env_put(WR_OUT);
    LOG("[warp] %s: Medallion slot borrowed for %.2f %.2f %.2f (%.1f deg); old %.2f %.2f %.2f (%.1f deg) goes back after the arrival\n", what,
        pos[0], pos[1], pos[2], deg, WP.old_pos[0], WP.old_pos[1], WP.old_pos[2], WP.old_deg);
    return true;
}
static bool warp_call_game(const char *what) {
    SwArgs a = {.x = {WP.fn[WS_STR_MAP], WP.fn[WS_STR_POS], 0}, .nx = 3};
    if (WP.run.st == WR_IDLE || !warp_call(WS_REQ, &a, NULL)) {
        warp_restore("the warp call itself was refused");
        LOG("[warp] %s refused: FUN_7100e81a30 could not be called\n", what);
        return false;
    }
    WP.nwarps++;
    WP.run.t0 = GetTickCount64();  // (s271: the start window counts from the call, not from the borrow a few frames earlier)
    LOG("[warp] %s: BotW's warp asked (FUN_7100e81a30 \"MainField/WarpIcon\" \"WarpPoint01\"); its accept bool is not reachable (see sora_warp.h)\n", what);
    return true;
}

// ---- start (game thread) ----
static void warp_fire(uint64_t now) {
    WP.want = false;
    const char *what = WP.what ? WP.what : "warp";
    if (WP.run.st != WR_IDLE) { LOG("[warp] %s refused: a warp is still running (%s)\n", what, WR_NAME[WP.run.st]); return; }
    if (g_loading || TITLE.on) { LOG("[warp] %s refused: %s is up\n", what, TITLE.on ? "BotW's title" : "a load"); return; }  // (s271 review M1)
    WgIn g = {.sites = warp_bind()};
    if (g.sites) g.cpu = WP.api->in_frame();
    if (g.cpu && warp_bool(WS_UI, &g.ui) && g.ui && warp_bool(WS_PROHIB, &g.prohibited) && !g.prohibited && warp_bool(WS_UI_EVENT, &g.event) && !g.event &&
        warp_bool(WS_UI_TRANS, &g.trans) && !g.trans && warp_bool(WS_PAUSED, &g.paused) && !g.paused) {
        int32_t h = 0;
        g.hart_ok = WP.gdt_s32("CurrentHart", &h) != 0, g.hart = h;
    }
    int why = wg_gate(&g);
    if (why != WG_OK) { LOG("[warp] %s refused: %s\n", what, WG_WHY[why]); return; }
    if (g_warp_log) LOG("[warp] gate open (life %d)\n", g.hart);
    if (!warp_borrow(WP.pos, WP.deg, what, 0, now)) return;
    warp_check_start(WP.pos, WP.deg, true, what);  // (s271: the call goes once the getter shows our spot, warp_check_tick)
    WP.note_pend = WP.k >= 0;
}
// (s271) once a frame: the later-frame read-back of a borrow / restore; a borrow's warp call goes from here
static void warp_check_tick(void) {
    if (!WCK.left) return;
    if (g_loading || TITLE.on) {  // (no gdt calls then; a pending call is dropped: the run's own rules restore / drop)
        if (WCK.call) WCK.left = 0, LOG("[warp] %s: a load / the title came before the slot read back: no warp call\n", WCK.what);
        return;
    }
    WCK.n++;
    float rp[3], rd;
    if (warp_dest_get(rp, &rd) && warp_dest_is(rp, rd, WCK.pos, WCK.deg)) {
        WCK.left = 0;
        if (g_warp_log || WCK.call)
            LOG("[warp] %s: the Medallion slot reads %.2f %.2f %.2f (%.1f deg) after %d frame%s\n", WCK.what, rp[0], rp[1], rp[2], rd, WCK.n, WCK.n == 1 ? "" : "s");
        if (WCK.call && WP.run.st == WR_OUT) warp_call_game(WCK.what);
        return;
    }
    if (--WCK.left) return;
    LOG("[warp] %s: the Medallion slot still reads %.2f %.2f %.2f (%.1f deg) after %d frames, not %.2f %.2f %.2f%s\n", WCK.what, rp[0], rp[1], rp[2], rd,
        WCK.n, WCK.pos[0], WCK.pos[1], WCK.pos[2], WCK.call ? ": no warp call, slot put back" : "");
    if (WCK.call) WCK.call = false, warp_restore("the borrowed spot never read back");
}

// ---- the friend's side: what we tell an ASK ----
static bool warp_spot_fill(NetWarpMsg *m) {
    m->flags = 0;
    if (!strcmp(NET.area, "MainField")) m->flags |= NWF_MAINFIELD;
    if (g_loading) m->flags |= NWF_LOADING;
    if (NET.dead) m->flags |= NWF_DEAD;
    // on the ground: the frame drove Link (in the world, not a load / menu), Link's own state is "ground", Sora not jumping /
    // gliding, not swimming / wading (NET.wet), alive
    if (NET.driven && !g_loading && !NET.dead && !NET.wet && linkpos_link_state() == LINK_GROUND && !ACT_AIR(S.act)) {
        m->flags |= NWF_GROUND;
        m->live[0] = g_body[0], m->live[1] = g_body[1], m->live[2] = g_body[2];
        m->live_deg = wsp_deg_norm(S.face * 57.2957795f);  // (GUESS: the same sense as PlayerSavePosAngleYDegree)
    }
    if (!g_loading && !TITLE.on && warp_bind() && WP.api->in_frame() && warp_vec_get(WS_SAVEPOS, m->save) && warp_f32_get(WS_SAVEANG, &m->save_deg))
        m->flags |= NWF_SAVE;  // (s271 review M2: no gdt reads in a load / on the title)
    return true;
}
// ---- the warper's side: his answer ----
static void warp_on_spot(int k, const NetWarpMsg *m) {
    const char *name = k >= 0 && k < NET_MAX_PEERS ? NET.slot[k].p.name : "?";
    if (!m) { LOG("[warp] warpfriend %d refused: \"%s\" did not answer in %d ms (an older build, or gone)\n", k + 1, name, WA_TIMEOUT_MS); return; }
    float pos[3], deg = 0;
    int how = wsp_choose(m->flags, m->live, m->live_deg, m->save, m->save_deg, pos, &deg);
    if (how == WSP_NONE) {
        LOG("[warp] warpfriend %d refused: \"%s\" %s (flags %#x)\n", k + 1, name,
            !(m->flags & NWF_MAINFIELD) ? "is not on MainField" : "has no usable spot (not on the ground, no safe spot)", m->flags);
        return;
    }
    if (g_warp_log) LOG("[warp] warpfriend %d \"%s\": %s %.2f %.2f %.2f (%.1f deg)\n", k + 1, name, WSP_NAME[how], pos[0], pos[1], pos[2], deg);
    memcpy(WP.pos, pos, 12), WP.deg = deg, WP.k = k, WP.nonce = m->nonce, WP.what = how == WSP_LIVE ? "warpfriend (his feet)" : "warpfriend (his safe spot)";
    WP.want = true;  // (warp_tick, this frame: the receive runs inside net_frame, before it)
}

// ---- the note "<name> warped to you" (sora_hud_notice: one slot, shared with kairi-diag's notice, which wins) ----
static void warp_note_compose(float alpha) {
    WP.nq = 0;
    if (!CAMP.ok) return;
    uint8_t m[64];
    int n = 0;
    for (int i = 0; WP.note[i] && n < 60; i++) {
        uint8_t b = WP.note[i] == ' ' ? 0x01 : cs_ascii(&CAMP.seq, WP.note[i]);
        if (b) m[n++] = b;
    }
    if (!n) return;
    float w = 0, h = 0;
    cs_text(&CAMP.seq, NULL, m, n, 0, 0, WARP_NOTE_SCALE, NULL, &w, &h);
    float x0 = g_warp_nx < 0 ? 256.0f - w * 0.5f : (float)g_warp_nx, y0 = (float)g_warp_ny;
    SoraHudQuad *b = &WP.q[0];
    memset(b, 0, sizeof *b);
    const float bx0 = x0 - 6, bx1 = x0 + w + 6, by0 = y0 - 3, by1 = y0 + h + 2;
    b->x[0] = bx0, b->y[0] = by0, b->x[1] = bx1, b->y[1] = by0, b->x[2] = bx0, b->y[2] = by1, b->x[3] = bx1, b->y[3] = by1;
    uint32_t ba = (uint32_t)(0x60 * alpha + 0.5f);
    for (int k = 0; k < 4; k++) b->col[k] = ba << 24;
    b->tex = 2;
    CsOut o = {WP.q, 1, WARP_NOTE_NQ};
    const float tc[4] = {1, 1, 1, alpha};  // (white, as KH2's information line; GUESS on the exact look)
    cs_text(&CAMP.seq, &o, m, n, x0, y0, WARP_NOTE_SCALE, tc, NULL, NULL);
    WP.nq = o.n;
}
static void warp_note_show(const char *who, uint64_t now) {
    snprintf(WP.note, sizeof WP.note, "%.15s warped to you", who && who[0] ? who : "A friend");
    WP.note_t0 = now ? now : 1;
    if (!CAMP.tried) camp_load();
    LOG("[warp] note: \"%s\"%s\n", WP.note, CAMP.ok ? "" : " (no font: log only)");
}
static void warp_on_note(int k) {
    warp_note_show(k >= 0 && k < NET_MAX_PEERS ? NET.slot[k].p.name : "", GetTickCount64());
}
static void warp_note_tick(uint64_t now) {
    float al = wn_alpha(now, WP.note_t0);
    if (al <= 0) {
        if (WP.note_sent && WP.notice && !KDG.shown_kind) WP.notice(NULL, 0);
        WP.note_sent = false, WP.note_t0 = 0;
        return;
    }
    if (!WP.notice) WP.notice = (void (*)(const SoraHudQuad *, int))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_notice");
    if (!WP.notice || KDG.shown_kind) return;  // (kairi-diag's own notice is up: it keeps the slot)
    warp_note_compose(al);
    if (WP.nq) WP.notice(WP.q, WP.nq), WP.note_sent = true;
}

// ---- live ----
static bool warp_live_key(const char *key, int v) {
    if (!strcmp(key, "warplog")) g_warp_log = v != 0, LOG("[warp] warplog %d\n", g_warp_log);
    else if (!strcmp(key, "warpnet")) g_warp_net = v != 0, LOG("[warp] warpnet %d (%s)\n", g_warp_net, g_warp_net ? "a friend's ask is answered, his note shown" : "no answers, no notes");
    else if (!strcmp(key, "warpicon")) g_warp_icon = v != 0, LOG("[warp] warpicon %d (%s)\n", g_warp_icon, g_warp_icon ? "after the restore the pillar + map marker go back onto the real slot" : "off");
    else if (!strcmp(key, "warpnx")) g_warp_nx = v < -1 ? -1 : v > 560 ? 560 : v, LOG("[warp] note x %d\n", g_warp_nx);
    else if (!strcmp(key, "warpny")) g_warp_ny = v < 0 ? 0 : v > 400 ? 400 : v, LOG("[warp] note y %d\n", g_warp_ny);
    else return false;
    return true;
}
static bool warp_live_line(const char *ln) {
    float x, y, z, d;
    int s;
    if (!strncmp(ln, "warpto ", 7)) {
        int n = sscanf(ln + 7, "%f %f %f %f", &x, &y, &z, &d);
        if (n < 3) { LOG("[warp] live: warpto needs <x> <y> <z> [deg]\n"); return true; }
        WP.pos[0] = x, WP.pos[1] = y, WP.pos[2] = z, WP.deg = n >= 4 ? wsp_deg_norm(d) : 0, WP.k = -1, WP.nonce = 0, WP.what = "warpto";
        WP.want = true;
        LOG("[warp] live: warpto %.2f %.2f %.2f (%.1f deg)\n", x, y, z, WP.deg);
        return true;
    }
    if (!strncmp(ln, "warpslot ", 9)) {  // (4b's half alone: borrow the slot now, NO warp call; then travel to the medallion by hand)
        int n = sscanf(ln + 9, "%f %f %f %f", &x, &y, &z, &d);
        if (n < 3) { LOG("[warp] live: warpslot needs <x> <y> <z> [deg]\n"); return true; }
        float p[3] = {x, y, z};
        if (g_loading || TITLE.on) { LOG("[warp] live: warpslot refused: a load / BotW's title is up\n"); return true; }  // (s271 review M1)
        if (warp_borrow(p, n >= 4 ? wsp_deg_norm(d) : 0, "warpslot", WARP_SLOT_TEST_MS, GetTickCount64()) && g_warp_icon) {
            // (warpicon 1: the map marker moves there too = recon/s270 route A's one call, no gdt; a player with no medallion then has one
            // to pick. After the arrival warp_map_fix puts it back on a real slot; on a 0,0,0 slot it stays shown until a load: GUESS)
            uint64_t dm = warp_map_d(), obj = 0;
            float v[4] = {x, y, z, 0};
            SwArgs b = {.x = {dm, 0x2a, 0}, .nx = 3, .x_scratch = 4, .scratch = 16, .init_len = 12, .init = v};
            bool ok = dm && WP.api->read(dm + WM_OBJ, &obj, 8) && obj && warp_call(WS_MAP_ICON, &b, NULL);
            LOG("[warp] warpslot: map marker %s\n", ok ? "moved there (FUN_7100d88670)" : "NOT moved (no map manager / marker)");
        }
        return true;
    }
    if (sscanf(ln, "warpfriend %d", &s) == 1) {
        if (WP.run.st != WR_IDLE) LOG("[warp] live: warpfriend %d refused: a warp is still running (%s)\n", s, WR_NAME[WP.run.st]);
        else if (s < 1 || s > NET_MAX_PEERS || !net_warp_ask(s - 1, GetTickCount64()))
            LOG("[warp] live: warpfriend %d refused: no friend in that slot (net %s; \"net diag\" lists the slots)\n", s, NET.on ? "on" : "off");
        else LOG("[warp] live: warpfriend %d: asking \"%s\" where he is\n", s, NET.slot[s - 1].p.name);
        return true;
    }
    if (!strncmp(ln, "warpset ", 8)) {  // (s271, dev) write the Medallion slot itself (0 0 0 0 = no medallion), no warp; one-shot
        int n = sscanf(ln + 8, "%f %f %f %f", &x, &y, &z, &d);
        if (n < 3) { LOG("[warp] live: warpset needs <x> <y> <z> [deg]\n"); return true; }
        float p[3] = {x, y, z};
        if (WP.run.st != WR_IDLE) LOG("[warp] live: warpset refused: a warp is running (%s)\n", WR_NAME[WP.run.st]);
        else if (g_loading || TITLE.on) LOG("[warp] live: warpset refused: a load / BotW's title is up\n");
        else if (!warp_bind() || !WP.api->in_frame()) LOG("[warp] live: warpset refused: no sites / not the game thread\n");
        else if (warp_dest_set(p, n >= 4 ? wsp_deg_norm(d) : 0)) {
            LOG("[warp] live: warpset %.2f %.2f %.2f: written\n", x, y, z);
            warp_check_start(p, n >= 4 ? wsp_deg_norm(d) : 0, false, "warpset");
        } else LOG("[warp] live: warpset %.2f %.2f %.2f: REFUSED\n", x, y, z);
        return true;
    }
    if (!strcmp(ln, "warp restore")) {
        if (WP.run.st == WR_IDLE) LOG("[warp] live: warp restore: no run holds the Medallion slot\n");
        else if (g_loading) LOG("[warp] live: warp restore refused: a load is up (the stage may still read the slot)\n");
        else warp_restore("live warp restore");
        return true;
    }
    if (!strncmp(ln, "warpnote", 8) && (!ln[8] || ln[8] == ' ')) {
        warp_note_show(ln[8] ? ln + 9 : NET.name, GetTickCount64());
        return true;
    }
    return false;
}

// ---- once a mod frame (sora_mod_frame, after the body; game thread, inside linkpos_frame) ----
static void warp_tick(uint64_t now) {
    if (!WP.hooked) {
        WP.hooked = true;
        NETW.hk.spot_fill = warp_spot_fill, NETW.hk.on_spot = warp_on_spot, NETW.hk.on_note = warp_on_note;
    }
    if (!g_on) return;  // (Sora mode off: the body does not run the load latch; no live lines either)
    if (!WP.env_checked) WP.env_checked = true, warp_env_resume(now);
    net_warp_ask_tick(now);
    if (WP.want) warp_fire(now);
    else warp_check_tick();  // (s271: not on the borrow's own frame: the game copies the buffers after it)
    if (WP.run.st != WR_IDLE) {
        WrIn in = {.now = now, .loading = g_loading, .title = TITLE.on};
        in.sw1 = linkpos_link_state_word() == 0x1;
        bool ui = false;
        if (!g_loading && WP.run.st == WR_OUT && warp_bind() && warp_bool(WS_UI, &ui) && ui) warp_bool(WS_UI_EVENT, &in.event);
        int was = WP.run.st;
        bool seen = WP.run.ev_seen;
        const char *why = "";
        int a = wr_step(&WP.run, &in, &why);
        if (WP.run.st == WR_LOAD && was != WR_LOAD) WP.probe_left = -1, WP.probe_last[0] = 0;  // (s270 note: the map probe from the load)
        else if (was == WR_LOAD && WP.run.st != WR_LOAD) WP.probe_left = WM_PROBE_FRAMES;  // (... to 120 frames after the card)
        if (g_warp_log && (WP.run.st != was || WP.run.ev_seen != seen))
            LOG("[warp] run: %s -> %s after %llu ms (event %d, state 0x1 %d, loading %d)\n", WR_NAME[was], WR_NAME[WP.run.st],
                (unsigned long long)(now - WP.run.t0), in.event, in.sw1, in.loading);
        if (!seen && WP.run.ev_seen) {
            LOG("[warp] the game took it: warp-out after %llu ms (%s)\n", (unsigned long long)(now - WP.run.t0), in.event ? "event bit" : "Link state 0x1");
            if (WP.note_pend && WP.k >= 0) net_warp_note(WP.k, WP.nonce);  // (s271 review L3: the friend's note only now)
            WP.note_pend = false;
        }
        if (WP.run.st == WR_IDLE) WP.note_pend = false;
        if (a == WRA_RESTORE) warp_restore(why);
        else if (a == WRA_DROP) LOG("[warp] run dropped: %s\n", why), wr_end(&WP.run), warp_env_put(0), WP.probe_left = 0;
        else warp_env_put(WP.run.st);
    } else if (WP.probe_left < 0) WP.probe_left = 0;  // (the run ended during a load: nothing more to watch)
    warp_map_probe();
    warp_note_tick(now);
}
