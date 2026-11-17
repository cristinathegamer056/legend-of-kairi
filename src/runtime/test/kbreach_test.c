// host tests for runtime/sora_kbreach.h (s249 kbreach: each KH2 Keyblade's own hit reach from its attack spheres):
//   clang -O1 -g -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Iruntime runtime/test/kbreach_test.c -o build/kbreach_test.exe && build/kbreach_test.exe
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "sora_kb.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

// ---- the stand-ins sora.c gives the headers (as kbspan_test.c)
static int g_logs;
static char g_last_log[512], g_logbuf[8192];
static void log_(const char *f, ...) {
    va_list ap;
    va_start(ap, f);
    vsnprintf(g_last_log, sizeof g_last_log, f, ap);
    va_end(ap);
    if (strlen(g_logbuf) + strlen(g_last_log) < sizeof g_logbuf) strcat(g_logbuf, g_last_log);
    g_logs++;
}
#define LOG(...) log_(__VA_ARGS__)
static struct { int kb[KB_SLOTS]; } KB;
static int g_fmots_on, g_fform, g_anti, g_mickey;
#define IN_ANTI (g_anti)
#define IN_MICKEY (g_mickey)
static const char *mk_kbfx_name(const char *kind) { (void)kind; return NULL; }
static bool sora_fx_ready(void) { return true; }
static int sora_fx_id(const char *n) { (void)n; return -1; }
static void sora_fx_trail(const float g[3], const float t[3], bool on) { (void)g, (void)t, (void)on; }
static int sora_fx_spawn(int id, const float pos[3], float yaw, float scale) { (void)id, (void)pos, (void)yaw, (void)scale; return -1; }
static bool sora_fx_alive(int h) { (void)h; return false; }
static int sora_fx_count(void) { return 0; }
static bool sora_sfx_ready(void) { return true; }
static int sora_sfx_id(const char *n) { (void)n; return -1; }
static int sora_sfx_play_at(int id, float gain, const float pos[3]) { (void)id, (void)gain, (void)pos; return 0; }
static int sora_sfx_motion(int slot, float a, float b, const float pos[3]) { (void)slot, (void)a, (void)b, (void)pos; return 0; }  // (s249 kbswing, merge)
static int sora_sfx_hit(int atkp, float gain, const float pos[3]) { (void)atkp, (void)gain, (void)pos; return 0; }
#include "sora_kbfx.h"

// ---- sora.c's hit types + today's sweep, COPIED VERBATIM from runtime/sora.c (s249; keep in step if sora.c's changes)
typedef struct { uint16_t start, end, atkp, group, power, kind; uint8_t reaction, effect, pad[2]; } HitWin;
static int g_mkhit = 1;
static float g_mk_reach = 0.35f;
static float g_hit_pad = 0.3f;
static float g_sweep_min;
static float g_sweep_hit[3];
static bool sweep_hits_rh(const float a[6], const float b[6], const float e[3], float hit_r, float hit_h) {
    float r = hit_r + g_hit_pad + (IN_MICKEY && g_mkhit ? g_mk_reach : 0);  // (s110 mkhit: his blade is short, KH2 hit with a sphere around the blade)
    g_sweep_min = 1e9f;
    for (int ti = 0; ti < 4; ti++)
        for (int ui = 0; ui < 5; ui++) {
            float t = (float)ti / 3, u = (float)ui / 4, p[3];
            for (int c = 0; c < 3; c++) {
                float pa = a[c] + (a[3 + c] - a[c]) * u, pb = b[c] + (b[3 + c] - b[c]) * u;
                p[c] = pa + (pb - pa) * t;
            }
            float dx = p[0] - e[0], dz = p[2] - e[2];
            if (sqrtf(dx * dx + dz * dz) < g_sweep_min) g_sweep_min = sqrtf(dx * dx + dz * dz);
            if (dx * dx + dz * dz <= r * r && p[1] >= e[1] - g_hit_pad && p[1] <= e[1] + hit_h + g_hit_pad) {
                float d = sqrtf(dx * dx + dz * dz), k = d > hit_r ? 1 : d > 1e-4f ? hit_r / d : 0;
                g_sweep_hit[0] = e[0] + dx * k, g_sweep_hit[1] = p[1], g_sweep_hit[2] = e[2] + dz * k;
                if (k == 0) memcpy(g_sweep_hit, p, sizeof g_sweep_hit);  // (on the axis: the sample itself)
                return true;
            }
        }
    return false;
}
#include "sora_kbreach.h"

#define TO_U 0.01f
static const float BR = 0.5f, BH = 1.6f;  // sora.c g_hit_r / g_hit_h: a small enemy's cylinder
// a blade lying along +x at height 1 (grip at the origin), `cm` long; prev = now (no motion) unless told
static void blade_x(float s[6], float cm) { s[0] = 0, s[1] = 1, s[2] = 0, s[3] = cm * TO_U, s[4] = 1, s[5] = 0; }
static HitWin win_g(int grp, int pad0) { HitWin w = {10, 20, 126, (uint16_t)grp, 100, 0, 0, 0, {(uint8_t)pad0, 0}}; return w; }
// today's two lines from sora.c (what kbreach 0 must equal)
static bool today(const float p[6], const float n[6], const float pos[3], float wrad) {
    float ep[3] = {pos[0], pos[1] - wrad, pos[2]};
    return sweep_hits_rh(p, n, ep, BR + wrad, BH + 2 * wrad);
}
static bool kh(int blade, const HitWin *w, const float p[6], const float n[6], const float pos[3], float wrad, bool botw) {
    return kbreach_hit(blade, 1, w, 0x1234, p, n, pos, BR, BH, wrad, TO_U, botw);
}
// the farthest x an enemy standing on the blade's line (feet 0.8 below the blade) is still hit at: tried at x + 0.5 cm (off
// the exact edges), so a reach of R cm returns floor(R - 0.5)
static int reach_cm(int blade, const HitWin *w, float blade_cm, bool use_today) {
    float s[6];
    blade_x(s, blade_cm);
    int last = -1;
    for (int x = 0; x <= 400; x++) {
        float pos[3] = {(x + 0.5f) * TO_U, 0.2f, 0};
        if (use_today ? today(s, s, pos, 0) : kh(blade, w, s, s, pos, 0, false)) last = x;
    }
    return last;
}
static const KbreachRow *row_of(int id) { int r = kbreach_row(id); return r < 0 ? NULL : &KBREACH_TAB[r]; }
static bool grp_is(int id, int grp, const int *cm, int n, int r) {  // the group's centres (file order) + radius
    const KbreachRow *k = row_of(id);
    if (!k) return false;
    int j = 0;
    for (int i = 0; i < k->n; i++) {
        const KbreachSph *s = &KBREACH_SPH[k->first + i];
        if (s->grp != grp) continue;
        if (j >= n || s->cm != cm[j] || s->r != r) return false;
        j++;
    }
    return j == n;
}

int main(void) {
    kb_default(KB.kb);
    // 1. the table (KH2's type-6 attack spheres, tools/kbreach_gen.py)
    { static const int kk1[] = {40, 0, 34, 68}, kk2[] = {0, 34, 68}, ss1[] = {40, 0, 67, 94}, ss2[] = {0, 47, 94};
      static const int fn1[] = {40, 10, 75, 110}, fn2[] = {10, 60, 110}, rr1[] = {40, 10, 34, 58};
      CHECK(grp_is(0x029, 1, kk1, 4, 40) && grp_is(0x029, 2, kk2, 3, 60));  // Kingdom Key
      CHECK(grp_is(0x1e0, 1, ss1, 4, 40) && grp_is(0x1e0, 2, ss2, 3, 60));  // Star Seeker
      CHECK(grp_is(0x1f3, 1, fn1, 4, 50) && grp_is(0x1f3, 2, fn2, 3, 70));  // Fenrir
      CHECK(grp_is(0x1f4, 1, fn1, 4, 50) && grp_is(0x1f4, 2, fn2, 3, 70));  // Ultima Weapon (= Fenrir's)
      CHECK(grp_is(0x02b, 1, kk1, 4, 40) && grp_is(0x02b, 2, kk2, 3, 60));  // Oblivion: KH2 gives it the Kingdom Key's spheres
      CHECK(grp_is(0x1ea, 1, rr1, 4, 50));                                  // Rumbling Rose: short + fat
      for (int id = 0x7e01; id <= 0x7e04; id++)                             // ours: the Kingdom Key's, marked
          CHECK(row_of(id) && row_of(id)->first == row_of(0x029)->first && row_of(id)->n == row_of(0x029)->n && !row_of(id)->own);
      CHECK(row_of(0x029)->own && row_of(0x02c)->own); }
    // every KBFX_TAB row and every KB_TAB row has one; ids unique; ranges sane; groups 3..7 the same in every model
    for (int i = 0; i < KBFX_N; i++) CHECK(kbreach_row(KBFX_TAB[i].id) >= 0);
    for (int i = 0; i < KB_N; i++) CHECK(kbreach_row(KB_TAB[i].id) >= 0);
    CHECK(KBREACH_N == KBFX_N);
    { int dup = 0, bad = 0, g37 = 0;
      const KbreachRow *kk = row_of(0x029);
      for (int i = 0; i < KBREACH_N; i++) {
          const KbreachRow *k = &KBREACH_TAB[i];
          bad += k->first + k->n > KBREACH_NSPH || k->n != 16;
          for (int j = i + 1; j < KBREACH_N; j++) dup += k->id == KBREACH_TAB[j].id;
          for (int s = 0; s < k->n; s++) {
              const KbreachSph *a = &KBREACH_SPH[k->first + s], *b = &KBREACH_SPH[kk->first + s];
              bad += a->grp < 1 || a->grp > 7 || a->r < 40 || a->r > 140 || a->cm < -50 || a->cm > 380;
              if (a->grp >= 3) g37 += a->grp != b->grp || a->cm != b->cm || a->r != b->r;
          }
      }
      CHECK(dup == 0 && bad == 0 && g37 == 0); }
    { KbreachGrp a = kbreach_grp(row_of(0x029), 1), b = kbreach_grp(row_of(0x029), 2), c = kbreach_grp(row_of(0x1f3), 1),
                 d = kbreach_grp(row_of(0x1f3), 2), e = kbreach_grp(row_of(0x1e0), 1), f = kbreach_grp(row_of(0x029), 20);
      CHECK(a.n == 4 && a.edge == 108 && a.rmax == 40 && b.n == 3 && b.edge == 128 && b.rmax == 60);
      CHECK(c.edge == 160 && c.rmax == 50 && d.edge == 180 && d.rmax == 70 && e.edge == 134 && f.n == 0); }
    CHECK(kbreach_of(0x7777) == row_of(0x029));  // an unknown id: the Kingdom Key's

    // 2. key 0 (the default until s250; the compiled default is 1 since the user verified it) = the old two lines exactly,
    //    for every Keyblade, both blades, with and without a window radius
    CHECK(g_kbreach == 1 && g_kbreach_log == 0);
    g_kbreach = 0;
    HitWin w1 = win_g(1, 0), w2 = win_g(2, 0);
    { int same = 0, n = 0, ids[] = {0x029, 0x1f3, 0x02b, 0x1ea, 0x7e04};
      float p[6], s[6];
      blade_x(s, 78), blade_x(p, 78), p[3] = 0, p[5] = 0.78f;  // (prev: the blade along +z, a quarter turn back)
      for (int k = 0; k < 5; k++)
          for (int b = 0; b < 2; b++)
              for (int x = -20; x <= 20; x++)
                  for (int z = -20; z <= 20; z += 4) {
                      KB.kb[KB_MAIN] = KB.kb[KB_VALOR] = ids[k];
                      float pos[3] = {x * 0.1f, 0.2f, z * 0.1f}, h1[3], h2[3], wr = (x & 1) ? 0.4f : 0;
                      bool a = today(p, s, pos, wr);
                      memcpy(h1, g_sweep_hit, sizeof h1);
                      bool c = kh(b, (z & 4) ? &w1 : &w2, p, s, pos, wr, false);
                      memcpy(h2, g_sweep_hit, sizeof h2);
                      same += a == c && (!a || !memcmp(h1, h2, sizeof h1)), n++;
                  }
      CHECK(same == n && n > 4000); }
    KB.kb[KB_MAIN] = 0x029, KB.kb[KB_VALOR] = 0x1e0;

    // 3. today's reach along the blade line: 78 cm + pad 30 + enemy 50 = 158 cm (the reference for the rest)
    CHECK(reach_cm(0, &w1, 78, true) == 157);

    // 4. key 1: the Kingdom Key in the main hand = today EXACTLY (any point, sparks too); others by KH2's difference
    g_kbreach = 1;
    { int same = 0, n = 0;
      float p[6], s[6];
      blade_x(s, 78), blade_x(p, 78), p[3] = 0, p[5] = 0.78f;
      for (int x = -20; x <= 20; x++)
          for (int z = -20; z <= 20; z += 2) {
              float pos[3] = {x * 0.1f, 0.2f + (z & 2) * 0.3f, z * 0.1f}, h1[3], h2[3], wr = (x & 1) ? 0.3f : 0;
              HitWin wf = win_g(2, (x & 1) ? 30 : 0);  // (also a Final-style RADIUS window: pad[0] 30 = the group's r60)
              bool a = today(p, s, pos, wr);
              memcpy(h1, g_sweep_hit, sizeof h1);
              bool c = kh(0, (z & 4) ? &w1 : &wf, p, s, pos, wr, false);
              memcpy(h2, g_sweep_hit, sizeof h2);
              same += a == c && (!a || !memcmp(h1, h2, sizeof h1)), n++;
          }
      CHECK(same == n); }
    CHECK(reach_cm(0, &w1, 78, false) == 157 && reach_cm(0, &w2, 78, false) == 157);  // Kingdom Key: today
    KB.kb[KB_MAIN] = 0x1f3;  // Fenrir: g1 160 vs 108 = +52 cm, g2 180 vs 128 = +52
    CHECK(reach_cm(0, &w1, 78, false) == 209 && reach_cm(0, &w2, 78, false) == 209);
    CHECK(strstr(g_last_log, "kbreach 1: Fenrir (0x1f3, blade 0)") && strstr(g_last_log, "reach 160 cm (Kingdom Key 108)"));
    { int n = g_logs; reach_cm(0, &w1, 78, false); CHECK(g_logs == n); }  // the first-use line once per Keyblade
    KB.kb[KB_MAIN] = 0x1e0; CHECK(reach_cm(0, &w1, 78, false) == 183);  // Star Seeker: 134 vs 108 = +26
    KB.kb[KB_MAIN] = 0x1ea; CHECK(reach_cm(0, &w1, 78, false) == 157);  // Rumbling Rose: 108 = the same reach (fatter by 10)
    { float s[6], pos[3] = {0.5f, 0.2f, 0.88f}; blade_x(s, 78);         // 88 cm beside the blade's middle: today misses, the fat one hits
      CHECK(!today(s, s, pos, 0) && kh(0, &w1, s, s, pos, 0, false)); }
    KB.kb[KB_MAIN] = 0x02b; CHECK(reach_cm(0, &w1, 78, false) == 157);  // Oblivion: KH2's spheres = the Kingdom Key's
    KB.kb[KB_MAIN] = 0x7e03; CHECK(reach_cm(0, &w1, 78, false) == 157);  // Silent Princess (ours): the Kingdom Key's
    // the left hand (Valor / Master / Final): its bake is Star Seeker's 103.6 cm blade; Star Seeker there = today, others by the difference
    KB.kb[KB_MAIN] = 0x029;
    KB.kb[KB_VALOR] = 0x1e0; CHECK(reach_cm(1, &w1, 103.6f, false) == reach_cm(1, &w1, 103.6f, true));
    KB.kb[KB_VALOR] = 0x1f4; CHECK(reach_cm(1, &w1, 103.6f, false) == reach_cm(1, &w1, 103.6f, true) + 26);  // Ultima: 160 vs 134
    KB.kb[KB_VALOR] = 0x029; CHECK(reach_cm(1, &w1, 103.6f, false) == reach_cm(1, &w1, 103.6f, true) - 26);  // Kingdom Key left: 108 vs 134
    CHECK(reach_cm(0, &w1, 78, false) == 157);  // the main blade keeps its own (Kingdom Key)
    KB.kb[KB_VALOR] = 0x1e0;

    // 5. key 2: KH2's own spheres (the Kingdom Key changes: report)
    g_kbreach = 2;
    KB.kb[KB_MAIN] = 0x029;
    CHECK(reach_cm(0, &w1, 78, false) == 157);  // 68 + 40 + 50 = 158 cm (KH2's strict "<")
    CHECK(reach_cm(0, &w2, 78, false) == 177);  // group 2: 68 + 60 + 50 = 178: 20 cm farther than today
    KB.kb[KB_MAIN] = 0x1f3; CHECK(reach_cm(0, &w1, 78, false) == 209 && reach_cm(0, &w2, 78, false) == 229);  // Fenrir 110 + 50 / 70 + 50
    KB.kb[KB_MAIN] = 0x1e0; CHECK(reach_cm(0, &w1, 78, false) == 183);  // Star Seeker 94 + 40 + 50
    // the spheres sit along the baked direction, not on its length: a 103.6 cm (left-hand) bake gives the same reach
    KB.kb[KB_VALOR] = 0x1f3; CHECK(reach_cm(1, &w1, 103.6f, false) == 209); KB.kb[KB_VALOR] = 0x1e0;
    KB.kb[KB_MAIN] = 0x029;
    // height: the 68 cm sphere (r 40) over an enemy right below it: KH2's cap rule (the sphere's slice at the cylinder's top)
    { float s[6]; blade_x(s, 78);
      float lo[3] = {0.68f, 1.0f - 0.4f - 1.6f - 0.01f, 0}, lo2[3] = {0.68f, 1.0f - 0.4f - 1.6f + 0.01f, 0};
      CHECK(!kh(0, &w1, s, s, lo, 0, false) && kh(0, &w1, s, s, lo2, 0, false));
      float d; float c[3] = {0, 1.19f, 0}, e[3] = {0.55f, 0, 0};   // dy 1.19 vs half height 0.8: slice r = 0.4 sqrt(1 - 0.975^2) = 0.089
      CHECK(kbreach_sph_cyl(c, 0.4f, e, 0.5f, 0.8f, &d) && !(e[0] = 0.6f, kbreach_sph_cyl(c, 0.4f, e, 0.5f, 0.8f, &d))); }
    // KH2's sub-steps: group 3's 380 cm sphere turning a quarter (now +x, prev +z) meets an enemy at 45 degrees only on its path
    { HitWin w3 = win_g(3, 0);
      float s[6], p[6], pos[3] = {2.4f, 0.2f, 2.4f};
      blade_x(s, 78), blade_x(p, 78), p[3] = 0, p[5] = 0.78f;
      float sbr = 0.05f;
      CHECK(!kbreach_hit(0, 2, &w3, 1, s, s, pos, sbr, BH, 0, TO_U, false));  // no motion: no sphere there
      CHECK(kbreach_hit(0, 3, &w3, 1, p, s, pos, sbr, BH, 0, TO_U, false));   // the path: hit
      CHECK(fabsf(hypotf(g_sweep_hit[0] - 2.4f, g_sweep_hit[2] - 2.4f) - sbr) < 1e-4f); }  // the spark on the enemy's surface
    // the spark sits on the enemy's cylinder, toward the sphere
    { float s[6], pos[3] = {1.2f, 0.2f, 0}; blade_x(s, 78);
      CHECK(kh(0, &w1, s, s, pos, 0, false) && fabsf(g_sweep_hit[0] - 0.7f) < 1e-5f && fabsf(g_sweep_hit[1] - 1.0f) < 1e-5f); }

    // 6. today's test, whatever the key, for: a BotW weapon, Mickey, Anti, kbfx 0, a bad blade, a group the Keyblade lacks
    //    (Limit's Zantetsuken 20), a body-shape window (pad[0] 75 = Final's body sphere, not the Keyblade's r60)
    { float s[6], p[6]; blade_x(s, 78), blade_x(p, 78), p[3] = 0, p[5] = 0.78f;
      HitWin w20 = win_g(20, 0), wb = win_g(2, 75);
      int same = 0, n = 0;
      KB.kb[KB_MAIN] = 0x1f3;
      for (int c = 0; c < 7; c++)
          for (int x = -20; x <= 20; x++) {
              float pos[3] = {x * 0.1f, 0.2f, 0.3f}, wr = c == 6 ? 0.75f : 0;
              const HitWin *w = c == 5 ? &w20 : c == 6 ? &wb : &w1;
              bool botw = c == 0;
              g_mickey = c == 1, g_anti = c == 2, g_kbfx = c != 3;
              int b = c == 4 ? 2 : 0;
              bool a = today(p, s, pos, wr), k = kh(b, w, p, s, pos, wr, botw);
              same += a == k, n++;
          }
      g_mickey = g_anti = 0, g_kbfx = 1;
      CHECK(same == n);
      KB.kb[KB_MAIN] = 0x029; }
    // live kbfxid forces the Keyblade (both blades), as the look / ribbon
    g_kbfx_id = 0x02c; CHECK(reach_cm(0, &w1, 78, false) == 209); g_kbfx_id = 0;  // Detection Saber = Fenrir's spheres

    // 7. logs: the key change once; the debug lines per window / per enemy met
    g_kbreach = 0;
    { int n = g_logs; kbreach_tick(); CHECK(g_logs == n); }  // (0 at start: nothing to say)
    g_kbreach = 2; { int n = g_logs; kbreach_tick(); kbreach_tick(); CHECK(g_logs == n + 1 && strstr(g_last_log, "kbreach 2")); }
    g_kbreach = 1; { int n = g_logs; kbreach_tick(); CHECK(g_logs == n + 1 && strstr(g_last_log, "Kingdom Key = today")); }
    g_kbreach = 0; { int n = g_logs; kbreach_tick(); CHECK(g_logs == n + 1 && strstr(g_last_log, "kbreach 0")); }
    g_kbreach = 2, g_kbreach_log = 1; kbreach_tick();
    { float s[6], pos[3] = {1.7f, 0.2f, 0}; blade_x(s, 78);  // group 2 reaches 1.78: new HIT, the old sweep (1.58) missed
      g_logbuf[0] = 0;
      CHECK(kbreach_hit(0, 77, &w2, 0xabc, s, s, pos, BR, BH, 0, TO_U, false));
      CHECK(strstr(g_logbuf, "kbreachlog swing 77 blade 0: Kingdom Key group 2 atkp 126, mode 2: spheres 0/34/68 r60 cm (reach 128 cm"));
      CHECK(strstr(g_logbuf, "enemy abc (1.70 u from the grip): new HIT (the sphere at 68 cm), old sweep miss"));
      int n = g_logs; kbreach_hit(0, 77, &w2, 0xabc, s, s, pos, BR, BH, 0, TO_U, false); CHECK(g_logs == n);  // once per window + enemy
      float away[3] = {3.0f, 0.2f, 0}; n = g_logs; kbreach_hit(0, 77, &w2, 0xdef, s, s, away, BR, BH, 0, TO_U, false); CHECK(g_logs == n); }  // (neither hit: quiet)
    g_kbreach = 0, g_kbreach_log = 0;

    printf("kbreach_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}
