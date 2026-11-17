// host tests for runtime/sora_kbfx.h (s216 kbfx: our two Keyblades' own sets / hit sounds and their fallbacks; s247: + Silent
// Princess / Calamity's Gaze and the bits shed off the blade):
//   clang -O1 -g -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Iruntime runtime/test/kbfx_test.c -o build/kbfx_test.exe && build/kbfx_test.exe
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "sora_kb.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

// ---- the stand-ins sora.c gives the header
static void log_(const char *f, ...) { (void)f; }  // (s247: the arguments are compiled, as with the real LOG)
#define LOG(...) log_(__VA_ARGS__)
static struct { int kb[KB_SLOTS]; } KB;
static int g_fmots_on, g_fform, g_anti, g_mickey;  // (g_mfkb: sora_kb.h)
#define IN_ANTI (g_anti)
#define IN_MICKEY (g_mickey)
static const char *mk_kbfx_name(const char *kind) { return g_mickey ? (!strcmp(kind, "hit") ? "mk_hit" : NULL) : NULL; }
// fx.bin: a name list (index = id)
static const char *const *g_fx;
static int g_nfx;
static bool sora_fx_ready(void) { return true; }
static int sora_fx_id(const char *n) { for (int i = 0; i < g_nfx; i++) if (!strcmp(g_fx[i], n)) return i; return -1; }
static void sora_fx_trail(const float g[3], const float t[3], bool on) { (void)g, (void)t, (void)on; }
// (s247) the shed emitter's fx calls + its clock
static int g_spawned, g_spawn_id[256], g_alive_all = 1, g_fx_live;
static uint64_t g_now_ms = 1000;
#define KBSHED_NOW_MS() (g_now_ms)
static int sora_fx_spawn(int id, const float pos[3], float yaw, float scale) {
    (void)pos, (void)yaw, (void)scale;
    if (g_spawned < 256) g_spawn_id[g_spawned] = id;
    return g_spawned++ % 32;
}
static bool sora_fx_alive(int h) { (void)h; return g_alive_all != 0; }
static int sora_fx_count(void) { return g_fx_live; }
// sfx.bin
static const char *const *g_sfx;
static int g_nsfx, g_played, g_hit_atkp;
static bool sora_sfx_ready(void) { return true; }
static int g_sfx_asks;  // (s247 kbse) sora_sfx_id calls (the cache test)
static int sora_sfx_id(const char *n) { g_sfx_asks++; for (int i = 0; i < g_nsfx; i++) if (!strcmp(g_sfx[i], n)) return i; return -1; }
static int sora_sfx_play_at(int id, float gain, const float pos[3]) { (void)gain, (void)pos; g_played = id, g_hit_atkp = -1; return 0; }
static int sora_sfx_hit(int atkp, float gain, const float pos[3]) { (void)gain, (void)pos; g_played = -2, g_hit_atkp = atkp; return 0; }
// (s249 kbswing) the exe's trigger player: records the slots asked for, each returns g_mot_ret sounds
static int g_nmot, g_mot[8], g_mot_ret = 1;
static int sora_sfx_motion(int slot, float prev, float clock, const float pos[3]) {
    (void)prev, (void)clock, (void)pos;
    if (g_nmot < 8) g_mot[g_nmot] = slot;
    g_nmot++;
    return g_mot_ret;
}

#include "sora_kbfx.h"

static void fresh(const char *const *fx, int nfx, const char *const *sfx, int nsfx) {  // a new pack: the caches start over
    g_fx = fx, g_nfx = nfx, g_sfx = sfx, g_nsfx = nsfx;
    memset(g_kbfx_ids, 0, sizeof g_kbfx_ids), memset(g_kbfx_ids_alt, 0, sizeof g_kbfx_ids_alt), memset(g_kbse_sfx, 0, sizeof g_kbse_sfx);
    g_kbfx_pack = -1, g_kbown = 1, g_kbse = 1, g_kbfx = 1, g_kbfx_id = 0, g_kbfx_blade = 0, g_anti = g_mickey = 0;
    g_kbshed = 1, g_kbshed_rate = 16, g_spawned = 0, g_alive_all = 1, g_fx_live = 0;
    memset(g_kbshed_live, 0, sizeof g_kbshed_live), memset(g_kbshed_acc, 0, sizeof g_kbshed_acc);
    g_kbhitse = 0, g_kbhitse_pack = -1, g_kbhitse_n = 0, memset(g_kbhitse_used, 0, sizeof g_kbhitse_used);  // (s247 kbse)
    g_kbswingse = g_kbwallse = 0, g_kbsw_pack = g_kbwl_pack = -1, g_kbwl_base = -2, g_nmot = 0, g_mot_ret = 1;  // (s249 kbswing)
    memset(g_kbsw_set, 0, sizeof g_kbsw_set), memset(g_kbwl_sfx, 0, sizeof g_kbwl_sfx);
    memset(g_kbsw_used, 0, sizeof g_kbsw_used), memset(g_kbwl_used, 0, sizeof g_kbwl_used);
}
// (s249) one kbsw_motion call: how many trigger slots it asked the exe for (their slots in g_mot) and the sound count
static int g_mot_sum;
static int mot(int slot, bool botw) { g_nmot = 0; g_mot_sum = kbsw_motion(slot, 0, 1, NULL, botw); return g_nmot; }
#define HS(k, hand, slot) (KBSW_ALT + ((1 + 2 * (k) + (hand)) << 16) + (slot))
// (s247) blade `blade` swings for `sec` seconds at `fps` with its ribbon window open; returns the bits dropped
static int swing(int blade, float sec, float fps) {
    const float g[3] = {0, 0, 0}, t[3] = {0, 1, 0};
    int n = 0, frames = (int)(sec * fps + 0.5f);
    for (int i = 0; i < frames; i++) n += kbfx_shed(blade, g, t, true, 0.01f, 60.0f / fps);
    return n;
}
static int trail_of(int item) { const KbfxIds *d = kbfx_use(kbfx_row(item), NULL); return d ? d->trail : -99; }

int main(void) {
    static const char *const FX_NEW[] = {"hit", "hit_finish", "hit_guard", "kb00_trail", "kb20_hit", "kb20_hit_finish", "kb20_hit_guard",
                                         "kb20_trail", "kbm0_hit", "kbm0_hit_finish", "kbm0_hit_guard", "kbm0_trail", "kb90_hit",
                                         "kb90_hit_finish", "kb90_hit_guard", "kb90_trail", "kbs1_hit", "kbs1_hit_finish", "kbs1_hit_guard",
                                         "kbs1_trail", "kbg1_hit", "kbg1_hit_finish", "kbg1_hit_guard", "kbg1_trail", "kbs1_beam", "kbs1_beam_hit"};
    static const char *const FX_OLD[] = {"hit", "hit_finish", "hit_guard", "kb00_trail", "kb20_hit", "kb20_hit_finish", "kb20_hit_guard",
                                         "kb20_trail", "kbm0_hit", "kbm0_hit_finish", "kbm0_hit_guard", "kbm0_trail", "kb90_hit",
                                         "kb90_hit_finish", "kb90_hit_guard", "kb90_trail"};
    static const char *const FX_S89[] = {"hit", "hit_finish", "hit_guard"};  // before the per-Keyblade sets
    static const char *const SFX_NEW[] = {"hit", "wall_hit", "kbs1_hit", "kbg1_hit", "kbs1_beam", "kbs1_beam_hit"};
    static const char *const SFX_OLD[] = {"hit", "wall_hit"};
    const float p[3] = {0, 0, 0};
    kb_default(KB.kb);

    // the table: our rows point at their own sets, with the s214 placeholders as alt; KH2's rows have no alt / no own sound
    int rs = kbfx_row(0x7e01), rg = kbfx_row(0x7e02);
    CHECK(rs > 0 && !strcmp(KBFX_TAB[rs].set, "s1") && !strcmp(KBFX_TAB[rs].alt, "m0") && !strcmp(KBFX_TAB[rs].se, "kbs1_hit"));
    CHECK(rg > 0 && !strcmp(KBFX_TAB[rg].set, "g1") && !strcmp(KBFX_TAB[rg].alt, "90") && !strcmp(KBFX_TAB[rg].se, "kbg1_hit"));
    int rp = kbfx_row(0x7e03), rc = kbfx_row(0x7e04);  // (s247)
    CHECK(rp > 0 && !strcmp(KBFX_TAB[rp].set, "p1") && !strcmp(KBFX_TAB[rp].alt, "m0") && !strcmp(KBFX_TAB[rp].se, "kbp1_hit"));
    CHECK(rc > 0 && !strcmp(KBFX_TAB[rc].set, "c1") && !strcmp(KBFX_TAB[rc].alt, "20") && !strcmp(KBFX_TAB[rc].se, "kbc1_hit"));
    for (int i = 0; i < KBFX_N; i++)
        if (i != rs && i != rg && i != rp && i != rc) CHECK(!KBFX_TAB[i].alt[0] && !KBFX_TAB[i].se && (!KBFX_TAB[i].set[0] || KBFX_TAB[i].set[1] == '0'));
        else CHECK(KBFX_TAB[i].set[0] && KBFX_TAB[i].set[1] != '0' && KBFX_TAB[i].set[2] == 0);  // (ours never end in '0')

    // 1. the new packs: own sets + own sound
    fresh(FX_NEW, 26, SFX_NEW, 6);
    KB.kb[KB_MAIN] = 0x7e01;
    CHECK(!strcmp(kbfx_name("hit"), "kbs1_hit") && !strcmp(kbfx_name("hit_finish"), "kbs1_hit_finish") && !strcmp(kbfx_name("hit_guard"), "kbs1_hit_guard"));
    CHECK(trail_of(0x7e01) == 19 && trail_of(0x7e02) == 23);
    kbse_hit(126, 1, p);
    CHECK(g_played == 2);
    KB.kb[KB_MAIN] = 0x7e02;
    CHECK(!strcmp(kbfx_name("hit"), "kbg1_hit"));
    kbse_hit(126, 1, p);
    CHECK(g_played == 3);
    // a KH2 Keyblade keeps its own set and the KH2 hit; the Kingdom Key the plain names
    KB.kb[KB_MAIN] = 0x2b;
    CHECK(!strcmp(kbfx_name("hit"), "kb20_hit") && trail_of(0x2b) == 7);
    kbse_hit(127, 1, p);
    CHECK(g_played == -2 && g_hit_atkp == 127);
    KB.kb[KB_MAIN] = 0x29;
    CHECK(!strcmp(kbfx_name("hit"), "hit") && trail_of(0x29) == 3);
    kbse_hit(126, 1, p);
    CHECK(g_played == -2 && g_hit_atkp == 126);
    // the second blade (Valor's left / Master's / Final's): its own Keyblade's sound and sparks
    KB.kb[KB_MAIN] = 0x29, KB.kb[KB_VALOR] = 0x7e02, g_kbfx_blade = 1;
    CHECK(!strcmp(kbfx_name("hit"), "kbg1_hit"));
    kbse_hit(126, 1, p);
    CHECK(g_played == 3);
    g_kbfx_blade = 0;
    kbse_hit(126, 1, p);
    CHECK(g_played == -2);
    // live kbfxid forces both
    g_kbfx_id = 0x7e01;
    CHECK(!strcmp(kbfx_name("hit_guard"), "kbs1_hit_guard"));
    kbse_hit(126, 1, p);
    CHECK(g_played == 2);
    g_kbfx_id = 0;
    // Mickey / Anti / kbfx 0 / kbse 0: the normal hit sound
    KB.kb[KB_MAIN] = 0x7e01;
    g_mickey = 1; kbse_hit(130, 1, p); CHECK(g_played == -2 && g_hit_atkp == 130); CHECK(!strcmp(kbfx_name("hit"), "mk_hit")); g_mickey = 0;
    g_anti = 1; kbse_hit(130, 1, p); CHECK(g_played == -2); CHECK(!strcmp(kbfx_name("hit"), "hit")); g_anti = 0;
    g_kbfx = 0; kbse_hit(130, 1, p); CHECK(g_played == -2); CHECK(!strcmp(kbfx_name("hit"), "hit")); g_kbfx = 1;
    g_kbse = 0; kbse_hit(130, 1, p); CHECK(g_played == -2); CHECK(!strcmp(kbfx_name("hit"), "kbs1_hit")); g_kbse = 1;
    kbse_hit(130, 1, p); CHECK(g_played == 2);
    // live kbown 0: the placeholders (sparks + ribbon), the sound stays its own
    g_kbown = 0;
    CHECK(!strcmp(kbfx_name("hit"), "kbm0_hit") && trail_of(0x7e01) == 11 && trail_of(0x7e02) == 15);
    KB.kb[KB_MAIN] = 0x7e02;
    CHECK(!strcmp(kbfx_name("hit_finish"), "kb90_hit_finish"));
    kbse_hit(126, 1, p); CHECK(g_played == 3);
    g_kbown = 1;
    CHECK(!strcmp(kbfx_name("hit_finish"), "kbg1_hit_finish") && trail_of(0x7e02) == 23);

    // 2. an older fx.bin (s89 sets, no s1 / g1) + an older sfx.bin: the placeholders, NOT the Kingdom Key's; the normal hit
    fresh(FX_OLD, 16, SFX_OLD, 2);
    KB.kb[KB_MAIN] = 0x7e01;
    CHECK(!strcmp(kbfx_name("hit"), "kbm0_hit") && trail_of(0x7e01) == 11);
    kbse_hit(126, 1, p); CHECK(g_played == -2 && g_hit_atkp == 126);
    kbse_hit(127, 1, p); CHECK(g_played == -2 && g_hit_atkp == 127);  // (asked once, cached)
    KB.kb[KB_MAIN] = 0x7e02;
    CHECK(!strcmp(kbfx_name("hit_guard"), "kb90_hit_guard") && trail_of(0x7e02) == 15);
    // 3. fx.bin before the per-Keyblade sets: everything the Kingdom Key's (as for every Keyblade then)
    fresh(FX_S89, 3, SFX_OLD, 2);
    KB.kb[KB_MAIN] = 0x7e01;
    CHECK(!strcmp(kbfx_name("hit"), "hit") && trail_of(0x7e01) == -1);
    // 4. new sfx.bin with an old fx.bin (or the other way round): each falls back on its own
    fresh(FX_OLD, 16, SFX_NEW, 6);
    CHECK(!strcmp(kbfx_name("hit"), "kbm0_hit"));
    kbse_hit(126, 1, p); CHECK(g_played == 2);
    fresh(FX_NEW, 26, SFX_OLD, 2);
    CHECK(!strcmp(kbfx_name("hit"), "kbs1_hit"));
    kbse_hit(126, 1, p); CHECK(g_played == -2);

    // 5. (s247) the packs with Silent Princess + Calamity's Gaze (appended after the s216 rows)
    static const char *const FX_247[] = {"hit", "hit_finish", "hit_guard", "kb00_trail", "kb20_hit", "kb20_hit_finish", "kb20_hit_guard",
                                         "kb20_trail", "kbm0_hit", "kbm0_hit_finish", "kbm0_hit_guard", "kbm0_trail", "kb90_hit",
                                         "kb90_hit_finish", "kb90_hit_guard", "kb90_trail", "kbs1_hit", "kbs1_hit_finish", "kbs1_hit_guard",
                                         "kbs1_trail", "kbg1_hit", "kbg1_hit_finish", "kbg1_hit_guard", "kbg1_trail", "kbs1_beam", "kbs1_beam_hit",
                                         "kbp1_hit", "kbp1_hit_finish", "kbp1_hit_guard", "kbp1_trail", "kbp1_shed_0", "kbp1_shed_1",
                                         "kbp1_shed_2", "kbc1_hit", "kbc1_hit_finish", "kbc1_hit_guard", "kbc1_trail", "kbc1_shed_0",
                                         "kbc1_shed_1", "kbc1_shed_2"};  // p1 = 26..32, c1 = 33..39
    static const char *const SFX_247[] = {"hit", "wall_hit", "kbs1_hit", "kbg1_hit", "kbs1_beam", "kbs1_beam_hit", "kbp1_hit", "kbc1_hit"};
    fresh(FX_247, 40, SFX_247, 8);
    KB.kb[KB_MAIN] = 0x7e03;
    CHECK(!strcmp(kbfx_name("hit"), "kbp1_hit") && !strcmp(kbfx_name("hit_finish"), "kbp1_hit_finish") && !strcmp(kbfx_name("hit_guard"), "kbp1_hit_guard"));
    CHECK(trail_of(0x7e03) == 29 && trail_of(0x7e04) == 36);
    kbse_hit(126, 1, p); CHECK(g_played == 6);
    KB.kb[KB_MAIN] = 0x7e04;
    CHECK(!strcmp(kbfx_name("hit_finish"), "kbc1_hit_finish"));
    g_kbsecg = 1; kbse_hit(126, 1, p); CHECK(g_played == 7); g_kbsecg = 0;  // (s250: its own sound only with kbsecg 1)
    kbse_hit(126, 1, p); CHECK(g_played == -2);                             // (the default: the normal KH2 hit)
    { const KbfxIds *d = kbfx_use(rp, NULL); CHECK(d && d->nshed == 3 && d->shed[0] == 30 && d->shed[2] == 32); }
    { const KbfxIds *d = kbfx_use(rc, NULL); CHECK(d && d->nshed == 3 && d->shed[0] == 37 && d->shed[2] == 39); }
    { const KbfxIds *d = kbfx_use(rs, NULL); CHECK(d && d->nshed == 0); }  // (Sealing Blade: no shed bits)
    // the rate is per second: 16 a second at 144, 86 and 30 fps (bits die at once here, so the cap never bites)
    g_alive_all = 0;
    KB.kb[KB_MAIN] = 0x7e03;
    int n144 = swing(0, 2.0f, 144), n86 = swing(0, 2.0f, 86), n30 = swing(0, 2.0f, 30);
    CHECK(n144 >= 31 && n144 <= 33 && n86 >= 31 && n86 <= 33 && n30 >= 31 && n30 <= 33);
    printf("  shed in 2 s: %d at 144 fps, %d at 86 fps, %d at 30 fps\n", n144, n86, n30);
    { int v[3] = {0, 0, 0}, ok = 1;  // every spawn is one of p1's three variants, each used
      for (int i = 0; i < g_spawned && i < 256; i++) { int k = g_spawn_id[i] - 30; if (k < 0 || k > 2) ok = 0; else v[k]++; }
      CHECK(ok && v[0] > 0 && v[1] > 0 && v[2] > 0); }
    g_kbshed_rate = 30; g_spawned = 0;
    CHECK(swing(0, 1.0f, 120) >= 29);  // (kbshedrate)
    g_kbshed_rate = 16;
    // a closed window: nothing, and the next window starts soon (acc 0.75: the first bit within a quarter interval)
    { const float g[3] = {0, 0, 0}, t[3] = {0, 1, 0}; CHECK(kbfx_shed(0, g, t, false, 0.01f, 1) == 0); }
    CHECK(swing(0, 0.017f, 60) == 1);  // (one 60 fps frame = 0.27 of an interval)
    // a hitch (kh 100) drops at most 2 in one frame
    { const float g[3] = {0, 0, 0}, t[3] = {0, 1, 0}; CHECK(kbfx_shed(0, g, t, true, 0.01f, 100) <= 2); }
    // the cap: bits stay alive -> at most KBSHED_CAP; once they are older than their life, more come again
    fresh(FX_247, 40, SFX_247, 8);
    KB.kb[KB_MAIN] = 0x7e04;
    CHECK(swing(0, 3.0f, 144) == KBSHED_CAP);
    g_now_ms += KBSHED_LIFE_MS + 1;
    CHECK(swing(0, 1.0f, 144) >= 12);
    // the fx table nearly full (the hit sparks come first): nothing
    fresh(FX_247, 40, SFX_247, 8);
    KB.kb[KB_MAIN] = 0x7e04, g_alive_all = 0, g_fx_live = 25;
    CHECK(swing(0, 1.0f, 60) == 0);
    g_fx_live = 24;
    CHECK(swing(0, 1.0f, 60) >= 15);
    // never for: another Keyblade (KH2's, Sealing Blade), the placeholders (kbown 0 / an older fx.bin), kbshed 0, rate 0, kbfx 0,
    // Anti, Mickey, a blade number out of range
    fresh(FX_247, 40, SFX_247, 8); g_alive_all = 0;
    KB.kb[KB_MAIN] = 0x2b; CHECK(swing(0, 1.0f, 60) == 0);
    KB.kb[KB_MAIN] = 0x7e01; CHECK(swing(0, 1.0f, 60) == 0);
    KB.kb[KB_MAIN] = 0x29; CHECK(swing(0, 1.0f, 60) == 0);
    KB.kb[KB_MAIN] = 0x7e03;
    g_kbown = 0; CHECK(swing(0, 1.0f, 60) == 0); CHECK(!strcmp(kbfx_name("hit"), "kbm0_hit")); g_kbown = 1;
    g_kbshed = 0; CHECK(swing(0, 1.0f, 60) == 0); g_kbshed = 1;
    g_kbshed_rate = 0; CHECK(swing(0, 1.0f, 60) == 0); g_kbshed_rate = 16;
    g_kbfx = 0; CHECK(swing(0, 1.0f, 60) == 0); g_kbfx = 1;
    g_anti = 1; CHECK(swing(0, 1.0f, 60) == 0); g_anti = 0;
    g_mickey = 1; CHECK(swing(0, 1.0f, 60) == 0); g_mickey = 0;
    CHECK(swing(2, 1.0f, 60) == 0 && swing(-1, 1.0f, 60) == 0);
    CHECK(swing(0, 1.0f, 60) >= 15);  // (and on again)
    // the second blade (Valor's left / Master's / Final's 2nd) sheds its OWN Keyblade's bits, the main blade's does not
    fresh(FX_247, 40, SFX_247, 8); g_alive_all = 0;
    KB.kb[KB_MAIN] = 0x29, KB.kb[KB_VALOR] = 0x7e04;
    CHECK(swing(0, 1.0f, 60) == 0);
    { int n = swing(1, 1.0f, 60), ok = n >= 15;
      for (int i = 0; i < g_spawned && i < 256; i++) if (g_spawn_id[i] < 37 || g_spawn_id[i] > 39) ok = 0;
      CHECK(ok); }
    // (s250) Calamity's Gaze hits play the normal KH2 hit by default; "set kbsecg 1" = its own sound (SFX_247 #7); Silent
    // Princess keeps its own (#6)
    g_kbfx_blade = 1; CHECK(g_kbsecg == 0 && kbse_id() == -1);
    g_kbsecg = 1; CHECK(kbse_id() == 7); g_kbsecg = 0; CHECK(kbse_id() == -1);
    KB.kb[KB_VALOR] = 0x7e03; CHECK(kbse_id() == 6); KB.kb[KB_VALOR] = 0x7e04; g_kbfx_blade = 0;
    // live kbfxid forces the look (and the bits) on any Keyblade
    g_kbfx_id = 0x7e03; g_spawned = 0;
    CHECK(swing(0, 1.0f, 60) >= 15 && g_spawn_id[0] >= 30 && g_spawn_id[0] <= 32);
    g_kbfx_id = 0;
    // 6. (s247) the s216 packs (no p1 / c1): the placeholders Fenrir / Oblivion, the normal hit, no bits
    fresh(FX_NEW, 26, SFX_NEW, 6); g_alive_all = 0;
    KB.kb[KB_MAIN] = 0x7e03;
    CHECK(!strcmp(kbfx_name("hit"), "kbm0_hit") && trail_of(0x7e03) == 11 && swing(0, 1.0f, 60) == 0);
    kbse_hit(126, 1, p); CHECK(g_played == -2 && g_hit_atkp == 126);
    KB.kb[KB_MAIN] = 0x7e04;
    CHECK(!strcmp(kbfx_name("hit_guard"), "kb20_hit_guard") && trail_of(0x7e04) == 7 && swing(0, 1.0f, 60) == 0);
    kbse_hit(127, 1, p); CHECK(g_played == -2 && g_hit_atkp == 127);

    // 7. (s247 kbse) each KH2 Keyblade its own KH2 hit sounds: "kbh<set>_<atkp>" + marker kbhitse_v1 (fx/kbhitse.py)
    static const char *const SFX_KBH[] = {"hit", "wall_hit", "kbs1_hit", "kbg1_hit", "kbs1_beam", "kbs1_beam_hit", "kbp1_hit", "kbc1_hit",
                                          "kbhitse_v1", "kbh20_126", "kbh20_127", "kbhn0_126", "kbhn0_1857", "kbhy0_126", "kbhm0_131"};
    //                                     8             9            10           11           12            13           14
    fresh(FX_247, 40, SFX_KBH, 15);
    KB.kb[KB_MAIN] = 0x2b, KB.kb[KB_VALOR] = 0x29;  // Oblivion in the main hand
    kbse_hit(126, 1, p); CHECK(g_played == -2 && g_hit_atkp == 126);  // the default (kbhitse 0): the Kingdom Key's, as before
    g_kbhitse = 1;
    kbse_hit(126, 1, p); CHECK(g_played == 9);
    kbse_hit(127, 1, p); CHECK(g_played == 10);
    kbse_hit(999, 1, p); CHECK(g_played == -2 && g_hit_atkp == 999);  // a row the pack has no name for (not a se500 hit): today's
    { int a0 = g_sfx_asks; kbse_hit(126, 1, p); kbse_hit(999, 1, p); CHECK(g_sfx_asks == a0 && g_played == -2); }  // cached, misses too
    kbse_hit(126, 1, p); CHECK(g_played == 9);
    CHECK(g_kbhitse_used[kbfx_row(0x2b)] == 1);
    kbse_hit(0x10000, 1, p); CHECK(g_played == -2 && g_hit_atkp == 0x10000);  // (out of the u16 cache range)
    kbse_hit(-1, 1, p); CHECK(g_played == -2);
    // other Keyblades: Ultima its own; Detection Saber its own set name (y0: Oblivion's bank in sfx.bin); Fenrir only where named
    KB.kb[KB_MAIN] = 0x1f4; kbse_hit(126, 1, p); CHECK(g_played == 11); kbse_hit(1857, 1, p); CHECK(g_played == 12);
    KB.kb[KB_MAIN] = 0x02c; kbse_hit(126, 1, p); CHECK(g_played == 13);
    KB.kb[KB_MAIN] = 0x1f3; kbse_hit(131, 1, p); CHECK(g_played == 14); kbse_hit(126, 1, p); CHECK(g_played == -2 && g_hit_atkp == 126);
    // the Kingdom Key (se500 is its own bank) and a Keyblade without a kbfx row: today's
    KB.kb[KB_MAIN] = 0x29; kbse_hit(126, 1, p); CHECK(g_played == -2 && g_hit_atkp == 126);
    KB.kb[KB_MAIN] = 0x51; kbse_hit(126, 1, p); CHECK(g_played == -2);
    KB.kb[KB_MAIN] = 0x7777; kbse_hit(126, 1, p); CHECK(g_played == -2);
    // our own Keyblades keep their own sound (their `se` comes first), with kbse 0 the normal hit (never a bank lookup)
    KB.kb[KB_MAIN] = 0x7e01; kbse_hit(126, 1, p); CHECK(g_played == 2);
    g_kbse = 0; kbse_hit(126, 1, p); CHECK(g_played == -2 && g_hit_atkp == 126); g_kbse = 1;
    KB.kb[KB_MAIN] = 0x7e04; g_kbsecg = 1; kbse_hit(126, 1, p); CHECK(g_played == 7); g_kbsecg = 0;  // (s250)
    // the second blade (Valor's left / Master's / Final's 2nd): ITS Keyblade's bank; the main blade its own
    KB.kb[KB_MAIN] = 0x29, KB.kb[KB_VALOR] = 0x1f4;
    g_kbfx_blade = 1; kbse_hit(126, 1, p); CHECK(g_played == 11);
    g_kbfx_blade = 0; kbse_hit(126, 1, p); CHECK(g_played == -2);
    KB.kb[KB_MAIN] = 0x2b, KB.kb[KB_VALOR] = 0x29;
    g_kbfx_blade = 1; kbse_hit(126, 1, p); CHECK(g_played == -2);
    g_kbfx_blade = 0; kbse_hit(126, 1, p); CHECK(g_played == 9);
    // live kbfxid forces both blades' Keyblade
    g_kbfx_id = 0x1f4; g_kbfx_blade = 1; kbse_hit(126, 1, p); CHECK(g_played == 11); g_kbfx_blade = 0; g_kbfx_id = 0;
    // Mickey / Anti / kbfx 0 / kbhitse 0: today's
    g_mickey = 1; kbse_hit(126, 1, p); CHECK(g_played == -2); g_mickey = 0;
    g_anti = 1; kbse_hit(126, 1, p); CHECK(g_played == -2); g_anti = 0;
    g_kbfx = 0; kbse_hit(126, 1, p); CHECK(g_played == -2); g_kbfx = 1;
    g_kbhitse = 0; kbse_hit(126, 1, p); CHECK(g_played == -2); g_kbhitse = 1;
    kbse_hit(126, 1, p); CHECK(g_played == 9);
    // an older sfx.bin (no marker): today's for every Keyblade, asked once
    fresh(FX_247, 40, SFX_247, 8);
    g_kbhitse = 1, KB.kb[KB_MAIN] = 0x2b;
    kbse_hit(126, 1, p); CHECK(g_played == -2 && g_hit_atkp == 126 && g_kbhitse_pack == 0);
    { int a0 = g_sfx_asks; kbse_hit(127, 1, p); CHECK(g_played == -2 && g_sfx_asks == a0); }
    // a pack with the names but WITHOUT the marker (half-built): off
    static const char *const SFX_NOMARK[] = {"hit", "kbh20_126"};
    fresh(FX_247, 40, SFX_NOMARK, 2);
    g_kbhitse = 1, KB.kb[KB_MAIN] = 0x2b;
    kbse_hit(126, 1, p); CHECK(g_played == -2);
    // the cache filling up: still right (asks again)
    fresh(FX_247, 40, SFX_KBH, 15);
    g_kbhitse = 1, KB.kb[KB_MAIN] = 0x2b;
    for (int i = 0; i < KBHITSE_CACHE + 10; i++) kbse_hit(2000 + i, 1, p);
    CHECK(g_kbhitse_n == KBHITSE_CACHE);
    kbse_hit(126, 1, p); CHECK(g_played == 9);
    kbse_hit(127, 1, p); CHECK(g_played == 10);
    kbfx_tick();  // (compiles + runs with the new key)

    // 8. (s249 kbswing) each hand's own KH2 whooshes: sfx.bin trigger slots KBSW_ALT + s (body rows) and KBSW_ALT + ((1 + 2k + hand)
    // << 16) + s (that hand's whooshes, set k), names kbswingse_v1 + kbsw<set>; the wall clank kbwallse_v1 + kbwl<set>
    static const char *const SFX_SW[] = {"hit", "wall_hit", "kbs1_hit", "kbg1_hit", "kbs1_beam", "kbs1_beam_hit", "kbp1_hit", "kbc1_hit",
                                         "kbswingse_v1", "kbsw20", "kbswn0", "kbswy0", "kbsw10", "kbwallse_v1", "kbwl20", "kbwln0", "kbwly0"};
    //                                    8               9         10        11        12        13             14        15        16
    CHECK(HS(0, 0, 0) == 0x1010000 && HS(2, 0, 5) == 0x1050005 && HS(23, 1, 10005) == 0x1300000 + 10005 && HS(35, 1, 0xffff) < 0x7fffffff);
    fresh(FX_247, 40, SFX_SW, 17);
    KB.kb[KB_MAIN] = 0x2b, KB.kb[KB_VALOR] = 0x1f4;  // Oblivion in the main hand, Ultima Weapon in the second
    // the default (keys 0): one call, the old slot, as today
    CHECK(mot(100, false) == 1 && g_mot[0] == 100 && g_mot_sum == 1);
    CHECK(g_kbsw_last[0] == -1 && g_kbsw_last[1] == -1);
    kbse_hit(126, 1, p); CHECK(g_played == -2);  // (kbhitse untouched)
    g_kbswingse = 1;
    // key on: the body copy + each hand's own set slot; the sum of the three
    CHECK(mot(100, false) == 3 && g_mot[0] == KBSW_ALT + 100 && g_mot[1] == HS(2, 0, 100) && g_mot[2] == HS(23, 1, 100) && g_mot_sum == 3);
    CHECK(g_kbsw_used[kbfx_row(0x2b)] == 1 && g_kbsw_used[kbfx_row(0x1f4)] == 1);
    // a form's slot (Valor 10000 + s) the same way; the cache: no new name lookups
    { int a0 = g_sfx_asks; CHECK(mot(10005, false) == 3 && g_mot[1] == HS(2, 0, 10005) && g_mot[2] == HS(23, 1, 10005)); CHECK(g_sfx_asks == a0); }
    // the Kingdom Key in a hand = set 0 (its own se500 rows); item 81 too
    KB.kb[KB_MAIN] = 0x29; CHECK(mot(7, false) == 3 && g_mot[1] == HS(0, 0, 7) && g_mot[2] == HS(23, 1, 7));
    KB.kb[KB_MAIN] = 0x51; CHECK(mot(7, false) == 3 && g_mot[1] == HS(0, 0, 7));
    // Detection Saber = its own set name y0 (Oblivion's bank in the pack), Oathkeeper 10
    KB.kb[KB_MAIN] = 0x02c; CHECK(mot(7, false) == 3 && g_mot[1] == HS(34, 0, 7));
    KB.kb[KB_MAIN] = 0x02a; CHECK(mot(7, false) == 3 && g_mot[1] == HS(1, 0, 7));
    // a set the pack lacks (Fenrir m0): the Kingdom Key's rows, asked once
    KB.kb[KB_MAIN] = 0x1f3;
    { CHECK(mot(7, false) == 3 && g_mot[1] == HS(0, 0, 7)); int a0 = g_sfx_asks; CHECK(mot(8, false) == 3 && g_mot[1] == HS(0, 0, 8) && g_sfx_asks == a0); }
    CHECK(g_kbsw_set[kbfx_row(0x1f3)] == -1);
    // our own Keyblades (no KH2 bank) and an id without a kbfx row: the Kingdom Key's
    for (int id = 0x7e01; id <= 0x7e04; id++) { KB.kb[KB_MAIN] = id; CHECK(mot(7, false) == 3 && g_mot[1] == HS(0, 0, 7)); }
    KB.kb[KB_MAIN] = 0x7777; CHECK(mot(7, false) == 3 && g_mot[1] == HS(0, 0, 7));
    // the second blade its own Keyblade: Kingdom Key there = set 0, Oblivion there = 2
    KB.kb[KB_MAIN] = 0x1f4, KB.kb[KB_VALOR] = 0x29; CHECK(mot(7, false) == 3 && g_mot[1] == HS(23, 0, 7) && g_mot[2] == HS(0, 1, 7));
    KB.kb[KB_VALOR] = 0x2b; CHECK(mot(7, false) == 3 && g_mot[2] == HS(2, 1, 7));
    // a BotW weapon in the main hand: that hand the Kingdom Key's, the second blade still its own
    CHECK(mot(7, true) == 3 && g_mot[1] == HS(0, 0, 7) && g_mot[2] == HS(2, 1, 7));
    // live kbfxid forces both hands
    g_kbfx_id = 0x02c; CHECK(mot(7, false) == 3 && g_mot[1] == HS(34, 0, 7) && g_mot[2] == HS(34, 1, 7)); g_kbfx_id = 0;
    // today's single call: Anti, Mickey, kbfx 0, slots of Roxas / Mickey / the summons (>= 0x10000), no slot, key 0
    g_anti = 1; CHECK(mot(60005, false) == 1 && g_mot[0] == 60005); g_anti = 0;
    g_mickey = 1; CHECK(mot(80005, false) == 1 && g_mot[0] == 80005); g_mickey = 0;
    g_kbfx = 0; CHECK(mot(7, false) == 1 && g_mot[0] == 7); g_kbfx = 1;
    CHECK(mot(70005, false) == 1 && g_mot[0] == 70005 && mot(95003, false) == 1 && g_mot[0] == 95003 && mot(0x10000, false) == 1);
    CHECK(mot(-1, false) == 1 && g_mot[0] == -1);
    g_kbswingse = 0; CHECK(mot(7, false) == 1 && g_mot[0] == 7); g_kbswingse = 1;
    CHECK(mot(7, false) == 3);
    // what the exe returns adds up (g_play_trig / fm_drive's voice check): 0 + 0 + 0 = 0, 2 + 2 + 2 = 6
    g_mot_ret = 0; CHECK(mot(7, false) == 3 && g_mot_sum == 0); g_mot_ret = 2; CHECK(mot(7, false) == 3 && g_mot_sum == 6); g_mot_ret = 1;
    // an older sfx.bin (no kbswingse_v1): today's single call, asked once
    fresh(FX_247, 40, SFX_KBH, 15);
    g_kbswingse = 1, KB.kb[KB_MAIN] = 0x2b;
    CHECK(mot(7, false) == 1 && g_mot[0] == 7 && g_kbsw_pack == 0);
    { int a0 = g_sfx_asks; CHECK(mot(8, false) == 1 && g_mot[0] == 8 && g_sfx_asks == a0); }
    // names without the marker (half-built pack): off
    static const char *const SFX_SWNOMARK[] = {"hit", "wall_hit", "kbsw20", "kbwl20"};
    fresh(FX_247, 40, SFX_SWNOMARK, 4);
    g_kbswingse = g_kbwallse = 1, KB.kb[KB_MAIN] = 0x2b;
    CHECK(mot(7, false) == 1 && g_mot[0] == 7);
    kbwall_play(126, p, false); CHECK(g_played == 1);

    // 9. (s249 kbswing) the wall clank: key 0 = "wall_hit" (#1) for every Keyblade
    fresh(FX_247, 40, SFX_SW, 17);
    KB.kb[KB_MAIN] = 0x2b, KB.kb[KB_VALOR] = 0x1f4;
    kbwall_play(126, p, false); CHECK(g_played == 1);
    g_kbwallse = 1;
    kbwall_play(126, p, false); CHECK(g_played == 14);  // Oblivion: its bank's cue 10
    g_kbfx_blade = 1; kbwall_play(126, p, false); CHECK(g_played == 15); g_kbfx_blade = 0;  // the second blade: Ultima's
    KB.kb[KB_MAIN] = 0x02c; kbwall_play(126, p, false); CHECK(g_played == 16);
    KB.kb[KB_MAIN] = 0x1f3; kbwall_play(126, p, false); CHECK(g_played == 1);  // (a set the pack lacks: the Kingdom Key's)
    { int a0 = g_sfx_asks; kbwall_play(126, p, false); CHECK(g_played == 1 && g_sfx_asks == a0); }
    KB.kb[KB_MAIN] = 0x29; kbwall_play(126, p, false); CHECK(g_played == 1);
    for (int id = 0x7e01; id <= 0x7e04; id++) { KB.kb[KB_MAIN] = id; kbwall_play(126, p, false); CHECK(g_played == 1); }
    KB.kb[KB_MAIN] = 0x2b;
    kbwall_play(126, p, true); CHECK(g_played == 1);  // (a BotW weapon in the main hand)
    g_kbfx_id = 0x1f4; kbwall_play(126, p, false); CHECK(g_played == 15); g_kbfx_id = 0;
    g_anti = 1; kbwall_play(126, p, false); CHECK(g_played == 1); g_anti = 0;
    g_mickey = 1; kbwall_play(126, p, false); CHECK(g_played == 1); g_mickey = 0;
    g_kbfx = 0; kbwall_play(126, p, false); CHECK(g_played == 1); g_kbfx = 1;
    g_kbwallse = 0; kbwall_play(126, p, false); CHECK(g_played == 1); g_kbwallse = 1;
    kbwall_play(126, p, false); CHECK(g_played == 14);
    CHECK(g_kbwl_used[kbfx_row(0x2b)] == 1);
    // an older pack: no kbwallse_v1 -> "wall_hit"; one before s52 (no "wall_hit") -> the atkp hit, as before
    fresh(FX_247, 40, SFX_KBH, 15); g_kbwallse = 1, KB.kb[KB_MAIN] = 0x2b;
    kbwall_play(126, p, false); CHECK(g_played == 1 && g_kbwl_pack == 0);
    static const char *const SFX_NOWALL[] = {"hit"};
    fresh(FX_247, 40, SFX_NOWALL, 1); g_kbwallse = 1;
    kbwall_play(131, p, false); CHECK(g_played == -2 && g_hit_atkp == 131);
    g_kbwallse = 0; kbwall_play(132, p, false); CHECK(g_played == -2 && g_hit_atkp == 132);
    g_kbswlog = 1; fresh(FX_247, 40, SFX_SW, 17); g_kbswingse = g_kbwallse = 1, KB.kb[KB_MAIN] = 0x2b;
    CHECK(mot(7, false) == 3); kbwall_play(126, p, false); CHECK(g_played == 14); g_kbswlog = 0;  // (the log lines compile + run)
    kbfx_tick();

    printf("kbfx_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}
