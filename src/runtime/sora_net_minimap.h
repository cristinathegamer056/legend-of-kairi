// (s268, co-op item 3) friends on BotW's minimap. Included once by sora.c after sora_net_avatar.h. Each frame (sora.c,
// after avatar_tick) mm_tick turns each friend's offset from Link with the camera (sora_hud_mm.h mm_turn) and sends the
// dots to the exe's sora_hud_minimap (looked up by name; an older exe = nothing, logged once). Colours per slot: red,
// green, purple (user s268). The exe places them (window shape, HUD size) and draws a 10 px dot on the disc or a small
// arrow on the rim for a friend off it; it hides them in menus / the full map / events / loads / the title.
// Shown only: live mmicons (default ON since s269's two-copy run; "set mmicons 0" = off), this frame drove the world (g_av_local_ok), no
// event (g_event), we are on MainField (or the area is not known: "set area 0"), and per friend: a sample, not
// loading, not hidden (another map, s191). Unlike his body (150 m cull) a friend far away still gets the rim arrow.
// "set mmfake 1": three pretend friends around Link (one circling at 100 m, one still at 40 m south, one 400 m
// north-east = the rim arrow) for a one-game test, no room needed. "set mmlog 1": one line a second.
// (s269, review s268 F2) "set mmhide 1" (default ON since s270, "set mmhide 0" = old): follow BotW's own minimap (the exe's sora_hud_minimap_pane,
// sora_hud_mmpane.h): no dots while its N_MiniMap_00 (or a pane above it) is hidden or the AppMap screen does not draw,
// faded with its global alpha, darkened with Sora's s71 dim like the minimap under it. "set mmhidem <mask>" picks the
// signs (MMP_PANE 1, MMP_SCREEN 2, MMP_ALPHA 4, MMP_DIM 8; default 15 = all). An exe without the export = as today.
// "set mmprobe 1": one [mm] pane line a second + one on each change (also in menus / the map / with mmicons off).
#pragma once
#include "sora_hud_mm.h"
#include "sora_hud_mmpane.h"

static bool g_mm_on = true;  // live mmicons (s269: compiled default ON after the two-copy run with real friends)
static bool g_mm_fake;  // live mmfake
static bool g_mm_log;   // live mmlog
static bool g_mm_hide = true;               // live mmhide (s269; s270 compiled default ON: his live look, Pro HUD hides the dots)
static unsigned g_mm_hide_mask = MMP_ALL;   // live mmhidem (s269)
static bool g_mm_probe;                     // live mmprobe (s269)

// (s269) the exe's answer about BotW's minimap, read once a frame while mmhide or mmprobe is on
typedef int (*MmPaneFn)(SoraMmPane *);
static struct { bool tried; MmPaneFn fn; SoraMmPane p; int ok; uint64_t log_at, chg_at; int key; } MMP;

static int mm_pane_key(const SoraMmPane *p, int ok) {
    int a = p->glb_alpha <= 0 ? 0 : p->glb_alpha >= 255 ? 2 : 1;
    return ok | p->screen << 1 | p->draws << 2 | (p->mode + 1) << 3 | p->found << 5 | p->vis << 6 | p->chain_vis << 7 | a << 8;
}
static float mm_pane_tick(uint64_t now) {
    if (!MMP.tried) {
        MMP.tried = true;
        MMP.fn = (MmPaneFn)(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_minimap_pane");
        LOG("[mm] BotW's minimap probe: %s\n", MMP.fn ? "sora_hud_minimap_pane found" : "sora_hud_minimap_pane not in this exe (NEEDS EXE RELINK): dots as before");
    }
    MMP.ok = 0;
    if (MMP.fn) {
        MMP.p.size = sizeof MMP.p;
        MMP.ok = MMP.fn(&MMP.p) >= 0;
    }
    float k = mmp_factor(MMP.ok, &MMP.p, g_mm_hide_mask);
    if (g_mm_probe) {
        int key = mm_pane_key(&MMP.p, MMP.ok);
        bool chg = key != MMP.key && now >= MMP.chg_at;
        if (chg || now >= MMP.log_at) {
            const SoraMmPane *p = &MMP.p;
            MMP.key = key, MMP.log_at = now + 1000, MMP.chg_at = now + 100;
            LOG("[mm] pane%s: exe %s, AppMap %s, draws %d, map mode %d, " MMP_NAME " %s (vis %d, chain %s%s, alpha %d, global %d, "
                "depth %d, walked %d, searches %d) -> dots x%.2f (mmhide %d mask %u)\n",
                chg ? " CHANGED" : "", MMP.fn ? (MMP.ok ? "ok" : "bad size") : "none", p->screen ? "up" : "none", p->draws, p->mode,
                p->found ? "found" : "not found", p->vis, p->chain_vis ? "visible" : "hidden at ", p->chain_vis ? "" : p->hidden_at,
                p->alpha, p->glb_alpha, p->depth, p->walked, p->searches, k, g_mm_hide, g_mm_hide_mask);
        }
    }
    return k;
}

typedef void (*MmSend)(const SoraHudMapDot *, int);
static struct { bool tried; MmSend fn; bool sent; uint64_t log_at; const char *why; } MM;

static void mm_send(const SoraHudMapDot *d, int n) {
    if (!MM.tried) {
        MM.tried = true;
        MM.fn = (MmSend)(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_minimap");
        LOG("[mm] minimap icons: %s\n", MM.fn ? "the exe draws them (sora_hud_minimap)" : "sora_hud_minimap not in this exe (NEEDS EXE RELINK): no icons");
    }
    if (!MM.fn || (!n && !MM.sent)) return;
    MM.fn(d, n);
    MM.sent = n > 0;
}

static bool mm_area_ok(void) { return !NET.area[0] || !strcmp(NET.area, "MainField"); }

static void mm_tick(uint64_t now) {
    SoraHudMapDot d[NET_MAX_PEERS + 3];
    int n = 0;
    float v[12], fx = 0, fz = 0;
    bool can_dot = (g_mm_on && NET.on) || g_mm_fake;  // (s270: mmhide default ON; single player never calls the exe probe)
    float hk = (g_mm_hide && can_dot) || g_mm_probe ? mm_pane_tick(now) : 1;  // (s269: BotW's minimap: 0 hidden .. 1 as it is)
    if (!g_mm_hide) hk = 1;
    const char *why = !(g_mm_on || g_mm_fake) ? "off" : !g_av_local_ok ? "not in the world" : g_event ? "event"
                      : !mm_area_ok() ? "not on MainField" : !(hk > 0) ? "hidden: BotW hides its minimap (mmhide)"
                      : !linkpos_camera_view(v) ? "no camera" : NULL;
    if (!why) {
        fx = -v[8], fz = -v[10];  // (view rows right / up / back: forward = -back, flattened)
        if (fx * fx + fz * fz < 0.04f) {  // (s268 review F4: looking nearly straight up / down: the up row's flat part points ahead / behind)
            float s = v[9] < 0 ? -1.0f : 1.0f;
            fx = s * v[4], fz = s * v[6];
        }
        float lk[3] = {g_body[0], g_body[1], g_body[2]};
        if (g_mm_on && NET.on)
            for (int k = 0; k < NET_MAX_PEERS && n < (int)(sizeof d / sizeof d[0]); k++) {
                NetPeerSlot *sl = &NET.slot[k];
                if (!sl->used || sl->hidden) continue;
                PeerPose q = peer_sample(&sl->p, now);
                if (!q.valid || q.alpha <= 0 || (q.flags & NF_LOADING)) continue;
                float mx, my;
                if (!mm_turn(q.feet[0] - lk[0], q.feet[2] - lk[2], fx, fz, &mx, &my)) continue;
                d[n++] = (SoraHudMapDot){mx, my, mm_colour(k), q.alpha < 1 ? q.alpha : 1};
            }
        if (g_mm_fake) {
            float t = (float)(now % 600000) * 0.001f;
            const float off[3][2] = {{100.0f * cosf(t * 0.3f), 100.0f * sinf(t * 0.3f)}, {0, 40.0f}, {283.0f, -283.0f}};
            for (int i = 0; i < 3 && n < (int)(sizeof d / sizeof d[0]); i++) {
                float mx, my;
                if (mm_turn(off[i][0], off[i][1], fx, fz, &mx, &my)) d[n++] = (SoraHudMapDot){mx, my, mm_colour(i), 1};
            }
        }
    }
    if (n > 4) n = 4;  // (the exe keeps 4)
    if (g_mm_hide)  // (s269: BotW's own fade; Sora's s71 dim darkens the minimap under the dots, so them too)
        for (int i = 0; i < n; i++) {
            d[i].a *= hk;
            if (g_mm_hide_mask & MMP_DIM) d[i].rgb = mmp_dim_rgb(d[i].rgb, FD.a);
        }
    mm_send(d, n);
    if (why != MM.why) {  // (s269: default ON, so single player (no room, no fake) stays quiet)
        if (can_dot) LOG("[mm] icons %s\n", why ? why : "shown");
        MM.why = why;
    }
    if (g_mm_log && !why && now >= MM.log_at) {
        MM.log_at = now + 1000;
        LOG("[mm] %d dot(s), camera fwd %.2f %.2f, area %s%s", n, fx, fz, NET.area[0] ? NET.area : "-", n ? ":" : "\n");
        for (int i = 0; i < n; i++) LOG(" (%.0f %.0f m #%06x)%s", d[i].x, d[i].y, (unsigned)d[i].rgb, i == n - 1 ? "\n" : "");
    }
}

// live keys (sora.c's set handler): true = taken
static bool mm_live_key(const char *key, int v) {
    if (!strcmp(key, "mmicons")) g_mm_on = v != 0, LOG("[mm] friends on the minimap %s\n", g_mm_on ? "on" : "off (set mmicons 0)");
    else if (!strcmp(key, "mmfake")) g_mm_fake = v != 0, LOG("[mm] pretend friends %s\n", g_mm_fake ? "on (100 m circling, 40 m south, 400 m north-east)" : "off");
    else if (!strcmp(key, "mmlog")) g_mm_log = v != 0, LOG("[mm] log %s\n", g_mm_log ? "on" : "off");
    else if (!strcmp(key, "mmhide")) g_mm_hide = v != 0, LOG("[mm] follow BotW's minimap (hidden / faded / dimmed) %s\n", g_mm_hide ? "on" : "off (dots as before)");
    else if (!strcmp(key, "mmhidem")) g_mm_hide_mask = (unsigned)v & MMP_ALL, LOG("[mm] mmhide signs %u (1 pane hidden, 2 screen not drawn, 4 its alpha, 8 Sora's dim)\n", g_mm_hide_mask);
    else if (!strcmp(key, "mmprobe")) g_mm_probe = v != 0, MMP.key = -1, LOG("[mm] minimap pane probe log %s\n", g_mm_probe ? "on" : "off");
    else return false;
    return true;
}
