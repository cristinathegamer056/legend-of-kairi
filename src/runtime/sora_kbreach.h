// sora_kbreach.h (s249 kbreach): each KH2 Keyblade's OWN HIT REACH, from its model's attack spheres (sora_kbreach_tab.h, made by
// tools/kbreach_gen.py from the KH2 extract). Notes + numbers: recon/s249/S249_HELPER_KBREACH.md.
//
// How KH2 hits (exe, VERIFIED by decompile; RVA, base 0x140000000):
//  - an attack window = mset range trigger 10 (atkp, group) makes a YS::ATTACK_OBJ (FUN_1403d25d0 -> ctor FUN_14040f1c0; the
//    group sits at +0xC8). Its frame step FUN_14040f610 walks the WEAPON object's collision rows of type 6 (mask 0x2040,
//    FUN_1403b6250 / FUN_1403c9bc0) and takes every row whose Group = the window's group. FUN_1403c9c00 makes the shape: sphere,
//    centre = bone matrix x (X, Y, Z), radius = the row's own Radius (no atkp / power scale).
//  - each sphere is tested against the targets' shapes (FUN_1403d08e0 -> FUN_1403c94c0); sphere vs an upright cylinder is
//    FUN_1403c9780: dy = |cyl.y - c.y|; no hit if dy >= R + cyl.h; the sphere's radius at that height (R, or R*sqrt(1-t^2) past
//    the cap) + cyl.r must exceed the horizontal distance.
//  - the motion since the last frame: if a sphere's centre moved more than 1.5 R (DAT_1405b22e0 = 1.5), extra spheres every R
//    from the new centre back toward the old one (up to 32 rows keep their old centre). Not on the attack's first frame; ours
//    fills the path on every frame (prev = the last frame's blade, as the old sweep), so a window that opens between two of our
//    (30 fps) frames still gets the KH2 frame in between.
// Ours: the enemy = its BotW cylinder (enemy_body: r, h from the feet). Live "set kbreach":
//  0 = today: the swept blade segment (grip -> the bake's tip) + g_hit_pad around it, for every Keyblade (sweep_hits_rh).
//  1 = today's sweep, but each Keyblade gets KH2's DIFFERENCE to the Keyblade the bake was made with (the Kingdom Key; Valor's /
//      Master's / Final's left-hand bakes = Star Seeker): the segment reaches farther by the farthest sphere edge's difference, the
//      radius grows by the biggest sphere's difference. The Kingdom Key in the main hand = today EXACTLY (same call, same numbers).
//  2 = KH2's own test: the window's group's spheres of the held Keyblade along the baked grip -> tip direction, KH2's sub-steps,
//      vs the enemy cylinder (no g_hit_pad: the sphere radius is the reach). The Kingdom Key differs from today (report).
// Untouched (today's sweep): key 0, kbfx 0, Mickey, Anti, a BotW weapon in the main hand, a window of a group the Keyblade has no
// sphere for (a body-mset window: Limit's Zantetsuken group 20, Sora's body windows 18 / 19 / 23), a body-shape window (sorabake
// FBODY: pad[0] is the body shape's radius, not the Keyblade's), trees / boxes / barrels / Stasis (their own calls), the remote
// player (hits are decided by the attacker; nothing new goes over the net).
// GUESS: the sphere centres lie on the baked grip -> tip line (the bake's tip vertex is 2.2 deg off bone 2's axis for the Kingdom
// Key, 3.5 deg for the left-hand Star Seeker bake); the grip = bone 0 of the weapon (bones 1 / 2 sit on it in the bind pose).
// Needs (sora.c, after sweep_hits_rh and sora_kbfx.h): HitWin, sweep_hits_rh, g_sweep_hit, g_sweep_min, kbfx_item, kbfx_row,
// KBFX_TAB, g_kbfx, IN_MICKEY, IN_ANTI, LOG.
#pragma once
#include "sora_kbreach_tab.h"

#define KBREACH_KK 0x029  // the Kingdom Key: the bake of the main hand (sorabake W_EX010), our Keyblades' spheres
#define KBREACH_SS 0x1e0  // Star Seeker: the bake of the left hand of Valor / Master / Final (tools/sorabuild/steps/bakes.py, W_EX010_30)
static const uint16_t KBREACH_BAKE_ID[2] = {KBREACH_KK, KBREACH_SS};
static int g_kbreach = 1; // live "set kbreach": 0 = the reach before s250, 1 = KH2's difference on that sweep (default since s250, user verified), 2 = KH2's spheres
static int g_kbreach_log;  // live "set kbreachlog 1": one line per window + per enemy the new / old test met
static uint8_t g_kbreach_used[KBREACH_N][2];  // the first use per (Keyblade, blade) was logged

static int kbreach_row(int id) {
    for (int i = 0; i < KBREACH_N; i++)
        if (KBREACH_TAB[i].id == id) return i;
    return -1;
}
static const KbreachRow *kbreach_of(int id) {  // an id without a row (a test value): the Kingdom Key's, as the bake
    int r = kbreach_row(id);
    return &KBREACH_TAB[r >= 0 ? r : kbreach_row(KBREACH_KK)];
}
typedef struct { int n; float edge, rmax; } KbreachGrp;  // a group's spheres: count, farthest edge (cm + r), biggest radius (cm)
static KbreachGrp kbreach_grp(const KbreachRow *k, int grp) {
    KbreachGrp g = {0, -1e9f, 0};
    for (int i = 0; i < k->n; i++) {
        const KbreachSph *s = &KBREACH_SPH[k->first + i];
        if (s->grp != grp) continue;
        g.n++;
        if (s->cm + s->r > g.edge) g.edge = (float)(s->cm + s->r);
        if (s->r > g.rmax) g.rmax = s->r;
    }
    return g;
}
static const char *kbreach_name(int id) { int r = kbfx_row(id); return r >= 0 ? KBFX_TAB[r].name : "?"; }
// "40/0/34/68 r40" (cm, in the file's order, as KH2 walks them; the group's last radius)
static void kbreach_desc(const KbreachRow *k, int grp, char *b, int n) {
    int len = 0, lr = -1;
    b[0] = 0;
    for (int i = 0; i < k->n && len < n - 16; i++) {
        const KbreachSph *s = &KBREACH_SPH[k->first + i];
        if (s->grp != grp) continue;
        len += snprintf(b + len, n - len, "%s%d", len ? "/" : "", s->cm);
        lr = s->r;
    }
    if (lr >= 0) snprintf(b + len, n - len, " r%d", lr);
}

// Which test a window of blade `blade` gets (0 = today's), and the Keyblades: own = the held one, ref = the bake's
static int kbreach_mode(int blade, const HitWin *w, bool botw, const KbreachRow **own, const KbreachRow **ref) {
    if (!g_kbreach || !g_kbfx || IN_MICKEY || IN_ANTI || botw || blade < 0 || blade > 1 || !w) return 0;
    *ref = kbreach_of(KBREACH_BAKE_ID[blade]);
    KbreachGrp rg = kbreach_grp(*ref, w->group);
    if (!rg.n) return 0;  // (a group the Keyblade has no shape for: a body-mset window)
    if (w->pad[0] && abs((int)w->pad[0] * 2 - (int)rg.rmax) > 1) return 0;  // (a body-shape window: FBODY's pad[0] = the body shape)
    *own = kbreach_of(kbfx_item(blade));
    if (!kbreach_grp(*own, w->group).n) return 0;
    int oi = (int)(*own - KBREACH_TAB);
    if (!g_kbreach_used[oi][blade]) {  // one line per Keyblade (and blade) the first time it hits this way
        g_kbreach_used[oi][blade] = 1;
        char d1[80], d2[80];
        KbreachGrp o1 = kbreach_grp(*own, 1), o2 = kbreach_grp(*own, 2), r1 = kbreach_grp(*ref, 1), r2 = kbreach_grp(*ref, 2);
        kbreach_desc(*own, 1, d1, sizeof d1), kbreach_desc(*own, 2, d2, sizeof d2);
        LOG("[sora] kbreach %d: %s (0x%x, blade %d) hits with its own KH2 spheres: group 1 at %s cm = reach %.0f cm (%s %.0f), "
            "group 2 at %s cm = %.0f cm (%.0f)%s\n", g_kbreach >= 2 ? 2 : 1, kbreach_name((*own)->id), (*own)->id, blade, d1, o1.edge,
            kbreach_name((*ref)->id), r1.edge, d2, o2.edge, r2.edge, (*own)->own ? "" : " [ours: the Kingdom Key's spheres]");
    }
    return g_kbreach >= 2 ? 2 : 1;
}

// sphere (centre c, radius R) vs the upright cylinder (axis through e, centre height e[1], half height hh, radius er): FUN_1403c9780
static bool kbreach_sph_cyl(const float c[3], float R, const float e[3], float er, float hh, float *dxz) {
    float dx = c[0] - e[0], dz = c[2] - e[2], dy = fabsf(c[1] - e[1]);
    *dxz = sqrtf(dx * dx + dz * dz);
    if (R + hh <= dy) return false;
    float rr = R;
    if (dy > hh) {
        float t = (dy - hh) / R, q = 1 - t * t;
        rr = R * sqrtf(q > 0 ? q : 0);
    }
    rr += er;
    return dx * dx + dz * dz < rr * rr;
}

// mode 2: the group's spheres of `own` on the segments prev -> now vs the enemy (feet pos, radius br, height bh). Fills
// g_sweep_hit (the contact, on the enemy's surface) and g_sweep_min (the closest sphere centre to the axis, u).
static bool kbreach_spheres(const KbreachRow *own, int grp, const float prev[6], const float now[6], const float pos[3], float br,
                            float bh, float to_u, int *hit_cm) {
    float e[3] = {pos[0], pos[1] + bh * 0.5f, pos[2]}, hh = bh * 0.5f, dn[3], dp[3];
    float ln = 0, lp = 0;
    for (int k = 0; k < 3; k++) dn[k] = now[3 + k] - now[k], dp[k] = prev[3 + k] - prev[k], ln += dn[k] * dn[k], lp += dp[k] * dp[k];
    ln = sqrtf(ln), lp = sqrtf(lp);
    g_sweep_min = 1e9f;
    if (ln < 1e-4f) return false;
    if (lp < 1e-4f) memcpy(dp, dn, sizeof dp), lp = ln;  // (no old direction: the new one)
    for (int i = 0; i < own->n; i++) {
        const KbreachSph *s = &KBREACH_SPH[own->first + i];
        if (s->grp != grp) continue;
        float R = s->r * to_u, cm = s->cm * to_u, cn[3], cp[3], d;
        for (int k = 0; k < 3; k++) cn[k] = now[k] + dn[k] / ln * cm, cp[k] = prev[k] + dp[k] / lp * cm;
        float c[3] = {cn[0], cn[1], cn[2]}, mv[3] = {cp[0] - cn[0], cp[1] - cn[1], cp[2] - cn[2]};
        float fv = sqrtf(mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2]);
        bool hit = kbreach_sph_cyl(c, R, e, br, hh, &d);
        if (d < g_sweep_min) g_sweep_min = d;
        if (!hit && R * 1.5f < fv) {  // KH2's sub-steps: every R from the new centre back toward the old one
            float st[3] = {mv[0] / fv * R, mv[1] / fv * R, mv[2] / fv * R};
            do {
                c[0] += st[0], c[1] += st[1], c[2] += st[2];
                hit = kbreach_sph_cyl(c, R, e, br, hh, &d);
                if (d < g_sweep_min) g_sweep_min = d;
                fv -= R;
            } while (!hit && R * 1.5f < fv);
        }
        if (!hit) continue;
        float dx = c[0] - e[0], dz = c[2] - e[2], dl = sqrtf(dx * dx + dz * dz), k = dl > br ? br / dl : 1;
        float y = c[1] < e[1] - hh ? e[1] - hh : c[1] > e[1] + hh ? e[1] + hh : c[1];
        g_sweep_hit[0] = e[0] + dx * k, g_sweep_hit[1] = y, g_sweep_hit[2] = e[2] + dz * k;
        if (hit_cm) *hit_cm = s->cm;
        return true;
    }
    return false;
}

// The enemy test of one blade window (sora.c blade_hits_one): pos = the enemy's feet, br / bh = its cylinder, wrad = the
// window's own shape radius (pad[0], u; 0 for most). Key 0 = exactly today's two lines.
static bool kbreach_hit(int blade, int swing, const HitWin *win, uint64_t actor, const float prev[6], const float now[6],
                        const float pos[3], float br, float bh, float wrad, float to_u, bool botw) {
    const KbreachRow *own = NULL, *ref = NULL;
    int mode = kbreach_mode(blade, win, botw, &own, &ref);
    float ep[3] = {pos[0], pos[1] - wrad, pos[2]};  // (today: s89 anti: wrad 0 = the old test)
    if (!mode) return sweep_hits_rh(prev, now, ep, br + wrad, bh + 2 * wrad);
    bool old = false;
    if (g_kbreach_log) old = sweep_hits_rh(prev, now, ep, br + wrad, bh + 2 * wrad);  // (for the log only: before ours, which sets the spark)
    KbreachGrp og = kbreach_grp(own, win->group), rg = kbreach_grp(ref, win->group);
    bool hit;
    int hit_cm = -999;
    float dreach = og.edge - rg.edge, dr = og.rmax - rg.rmax;
    if (mode == 1) {
        if (dreach == 0 && dr == 0) hit = sweep_hits_rh(prev, now, ep, br + wrad, bh + 2 * wrad);  // (the bake's own Keyblade: today exactly)
        else {
            float p2[6], n2[6], ext = (dreach - dr) * to_u, wr = wrad + dr * to_u;
            const float *src[2] = {prev, now};
            float *dst[2] = {p2, n2};
            for (int s = 0; s < 2; s++) {
                const float *a = src[s];
                float d[3] = {a[3] - a[0], a[4] - a[1], a[5] - a[2]}, l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                float f = l > 1e-4f ? (l + ext) / l : 1;
                if (f < 0.1f) f = 0.1f;
                for (int k = 0; k < 3; k++) dst[s][k] = a[k], dst[s][3 + k] = a[k] + d[k] * f;
            }
            if (br + wr < 0.05f) wr = 0.05f - br;
            float e2[3] = {pos[0], pos[1] - wr, pos[2]};
            hit = sweep_hits_rh(p2, n2, e2, br + wr, bh + 2 * wr);
        }
    } else
        hit = kbreach_spheres(own, win->group, prev, now, pos, br, bh, to_u, &hit_cm);
    if (g_kbreach_log) {
        static struct { int swing; const void *win; uint64_t actor; uint8_t blade, res; } seen[32];
        static int next;
        static int lswing = -1, lblade = -1;
        static const void *lwin;
        if (lswing != swing || lwin != (const void *)win || lblade != blade) {
            lswing = swing, lwin = win, lblade = blade;
            char d[80];
            kbreach_desc(own, win->group, d, sizeof d);
            LOG("[sora] kbreachlog swing %d blade %d: %s group %d atkp %d, mode %d: spheres %s cm (reach %.0f cm, %s %.0f)\n", swing,
                blade, kbreach_name(own->id), win->group, win->atkp, mode, d, og.edge, kbreach_name(ref->id), rg.edge);
        }
        uint8_t res = (uint8_t)(hit | old << 1);
        int k = 0;
        while (k < 32 && !(seen[k].swing == swing && seen[k].win == win && seen[k].actor == actor && seen[k].blade == blade && seen[k].res == res)) k++;
        if (res && k == 32) {
            seen[next].swing = swing, seen[next].win = win, seen[next].actor = actor, seen[next].blade = (uint8_t)blade, seen[next].res = res;
            next = (next + 1) % 32;
            float dx = pos[0] - now[0], dz = pos[2] - now[2];
            char by[40] = "";
            if (hit && hit_cm > -999) snprintf(by, sizeof by, " (the sphere at %d cm)", hit_cm);
            LOG("[sora] kbreachlog swing %d blade %d enemy %llx (%.2f u from the grip): new %s%s, old sweep %s\n", swing, blade,
                (unsigned long long)actor, sqrtf(dx * dx + dz * dz), hit ? "HIT" : "miss", by, old ? "HIT" : "miss");
        }
    }
    return hit;
}

// once per frame (after kbspan_tick): one line when the key changes
static void kbreach_tick(void) {
    static int last = -1;
    if (g_kbreach == last) return;
    if (last >= 0 || g_kbreach)
        LOG(g_kbreach >= 2 ? "[sora] kbreach 2: each Keyblade hits with KH2's own attack spheres (the Kingdom Key changes too)\n"
            : g_kbreach ? "[sora] kbreach 1: each Keyblade's reach differs from the Kingdom Key's as in KH2 (the Kingdom Key = today)\n"
                        : "[sora] kbreach 0: every Keyblade hits with the Kingdom Key's blade (today)\n");
    last = g_kbreach;
    memset(g_kbreach_used, 0, sizeof g_kbreach_used);
}
