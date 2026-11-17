// host tests for runtime/sora_kbspan.h (s247 kbspan: each KH2 Keyblade's swing ribbon reaches its own tip):
//   clang -O1 -g -Wall -Wextra -Wno-unused-function -Wno-missing-field-initializers -Iruntime runtime/test/kbspan_test.c -o build/kbspan_test.exe && build/kbspan_test.exe
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "sora_kb.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

// ---- the stand-ins sora.c gives the headers (as kbfx_test.c)
static int g_logs;
static char g_last_log[256];
static void log_(const char *f, ...) {
    va_list ap;
    va_start(ap, f);
    vsnprintf(g_last_log, sizeof g_last_log, f, ap);
    va_end(ap);
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
static int sora_sfx_hit(int atkp, float gain, const float pos[3]) { (void)atkp, (void)gain, (void)pos; return 0; }
static int sora_sfx_motion(int slot, float a, float b, const float pos[3]) { (void)slot, (void)a, (void)b, (void)pos; return 0; }  // (s249 kbswing)

#include "sora_kbfx.h"
#include "sora_kbspan.h"

static float len3(const float *a, const float *b) {
    float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}
static int tip_of(int id) { int r = kbspan_row(id); return r < 0 ? -1 : KBSPAN_TAB[r].tip_cm; }
static bool same6(const float *a, const float *b) { for (int k = 0; k < 6; k++) if (a[k] != b[k]) return false; return true; }

int main(void) {
    kb_default(KB.kb);
    // 1. the table (KH2's tip markers, tools/kbspan_gen.py): the highlights
    CHECK(tip_of(0x029) == 78 && tip_of(0x051) == 78);  // Kingdom Key = the baked 78 cm: factor exactly 1
    CHECK(tip_of(0x02b) == 103);                        // Oblivion
    CHECK(tip_of(0x1f4) == 130 && tip_of(0x1f3) == 130);  // Ultima Weapon, Fenrir
    CHECK(tip_of(0x02a) == 81 && tip_of(0x1eb) == 120 && tip_of(0x220) == 127);  // Oathkeeper, Guardian Soul, Winner's Proof
    CHECK(tip_of(0x02c) == 130 && tip_of(0x02d) == 78);  // Detection Saber / Frontier of Ultima: their own models' markers
    CHECK(kbspan_factor(0x029) == 1.0f && fabsf(kbspan_factor(0x02b) - 103.0f / 78.0f) < 1e-6f);
    // every KH2 row of KBFX_TAB has exactly one span row; ours (0x7e01..04) none; values sane; ids unique
    for (int i = 0; i < KBFX_N; i++) {
        bool kh2 = !KBFX_TAB[i].set[0] || KBFX_TAB[i].set[1] == '0';
        CHECK(kh2 ? kbspan_row(KBFX_TAB[i].id) >= 0 : kbspan_row(KBFX_TAB[i].id) < 0);
    }
    { int dup = 0;
      for (int i = 0; i < KBSPAN_N; i++) {
          CHECK(KBSPAN_TAB[i].tip_cm >= 70 && KBSPAN_TAB[i].tip_cm <= 140 && kbfx_row(KBSPAN_TAB[i].id) >= 0);
          for (int j = i + 1; j < KBSPAN_N; j++) dup += KBSPAN_TAB[i].id == KBSPAN_TAB[j].id;
      }
      CHECK(dup == 0); }
    CHECK(KBSPAN_N == 30 && kbspan_row(0x7e01) < 0 && kbspan_row(0x7e04) < 0 && kbspan_factor(0x7777) == 1.0f);

    // 2. the maths on a world segment (any direction): grip kept, tip on the same line, length x tip / 78
    const float seg[6] = {10.0f, 2.0f, -3.0f, 10.0f + 0.78f * 2 / 7, 2.0f + 0.78f * 6 / 7, -3.0f - 0.78f * 3 / 7};  // 0.78 m long (to_u 0.01)
    float out[6];
    KB.kb[KB_MAIN] = 0x02b;  // Oblivion
    CHECK(g_kbspan == 1);  // (s249) default ON since the user verified it
    g_kbspan = 0;
    CHECK(kbspan_seg(0, seg, false, out) == 1.0f && same6(out, seg));  // kbspan 0: the old way (78 cm for all)
    g_kbspan = 1;
    float f = kbspan_seg(0, seg, false, out);
    CHECK(fabsf(f - 103.0f / 78.0f) < 1e-6f);
    CHECK(out[0] == seg[0] && out[1] == seg[1] && out[2] == seg[2]);  // the near end stays
    CHECK(fabsf(len3(out, out + 3) - 1.03f) < 1e-4f);                   // 1.03 m = 103 cm at to_u 0.01
    { float a[3] = {seg[3] - seg[0], seg[4] - seg[1], seg[5] - seg[2]}, b[3] = {out[3] - out[0], out[4] - out[1], out[5] - out[2]};
      float cx = a[1] * b[2] - a[2] * b[1], cy = a[2] * b[0] - a[0] * b[2], cz = a[0] * b[1] - a[1] * b[0];
      CHECK(sqrtf(cx * cx + cy * cy + cz * cz) < 1e-5f && a[0] * b[0] + a[1] * b[1] + a[2] * b[2] > 0); }  // same direction
    { int n = g_logs; kbspan_seg(0, seg, false, out); CHECK(g_logs == n); }  // the first-use line only once per Keyblade
    // a shorter baked segment (a mid-frame lerped tip) scales by the same factor: the ribbon keeps today's shape, just longer
    { float s2[6] = {0, 0, 0, 0, 0.5f, 0}; kbspan_seg(0, s2, false, out); CHECK(fabsf(out[4] - 0.5f * 103.0f / 78.0f) < 1e-6f); }
    // Ultima / Fenrir 130; the Kingdom Key exactly today's
    KB.kb[KB_MAIN] = 0x1f4; kbspan_seg(0, seg, false, out); CHECK(fabsf(len3(out, out + 3) - 1.30f) < 1e-4f);
    CHECK(strstr(g_last_log, "Ultima Weapon") && strstr(g_last_log, "130 cm"));
    KB.kb[KB_MAIN] = 0x1f3; kbspan_seg(0, seg, false, out); CHECK(fabsf(len3(out, out + 3) - 1.30f) < 1e-4f);
    KB.kb[KB_MAIN] = 0x029; CHECK(kbspan_seg(0, seg, false, out) == 1.0f && same6(out, seg));
    // our own Keyblades / an id without a row: today
    KB.kb[KB_MAIN] = 0x7e01; CHECK(kbspan_seg(0, seg, false, out) == 1.0f && same6(out, seg));
    KB.kb[KB_MAIN] = 0x7e03; CHECK(kbspan_seg(0, seg, false, out) == 1.0f && same6(out, seg));
    KB.kb[KB_MAIN] = 0x7777; CHECK(kbspan_seg(0, seg, false, out) == 1.0f && same6(out, seg));
    // the second blade: ITS Keyblade (Valor's left = Ultima while the main is the Kingdom Key), the main blade its own
    // (s249) the left hand's bake is Star Seeker's 103.64 cm blade (valor / master / final_hits_l*.bin), not 78: its factor is
    // tip / 103.64, so each Keyblade's LEFT ribbon ends at its own tip too (before: tip / 78 = 1.33x too long there)
    const float segl[6] = {10.0f, 2.0f, -3.0f, 10.0f + 1.0364f * 2 / 7, 2.0f + 1.0364f * 6 / 7, -3.0f - 1.0364f * 3 / 7};  // 1.0364 m
    CHECK(kbspan_baked(0) == 78.0f && kbspan_baked(1) == 103.64f);
    KB.kb[KB_MAIN] = 0x029, KB.kb[KB_VALOR] = 0x1f4;
    kbspan_seg(1, segl, false, out); CHECK(fabsf(len3(out, out + 3) - 1.30f) < 1e-4f);  // Ultima left: 130 cm
    CHECK(kbspan_seg(0, seg, false, out) == 1.0f);
    KB.kb[KB_MAIN] = 0x02b, KB.kb[KB_VALOR] = 0x029;
    kbspan_seg(1, segl, false, out); CHECK(fabsf(len3(out, out + 3) - 0.78f) < 1e-4f);   // Kingdom Key left: 78 cm (was 103.6)
    kbspan_seg(0, seg, false, out); CHECK(fabsf(len3(out, out + 3) - 1.03f) < 1e-4f);
    KB.kb[KB_MAIN] = 0x029, KB.kb[KB_VALOR] = 0x02b;
    kbspan_seg(1, segl, false, out); CHECK(fabsf(len3(out, out + 3) - 1.03f) < 1e-4f);   // Oblivion left: 103 cm (was 136.8)
    KB.kb[KB_VALOR] = 0x1e0;
    kbspan_seg(1, segl, false, out); CHECK(fabsf(len3(out, out + 3) - 0.98f) < 1e-4f);   // Star Seeker left: its own 98 cm marker
    CHECK(strstr(g_last_log, "Star Seeker (0x1e0, blade 1)") && strstr(g_last_log, "of the baked 103.64 cm"));
    CHECK(fabsf(kbspan_factor_b(0x029, 1) - 78.0f / 103.64f) < 1e-6f && kbspan_factor_b(0x7777, 1) == 1.0f);
    KB.kb[KB_MAIN] = 0x02b, KB.kb[KB_VALOR] = 0x029;
    // a BotW weapon in the main hand (its own reach), Mickey, kbfx 0, a bad blade: today
    CHECK(kbspan_seg(0, seg, true, out) == 1.0f && same6(out, seg));
    g_mickey = 1; CHECK(kbspan_seg(0, seg, false, out) == 1.0f && same6(out, seg)); g_mickey = 0;
    g_kbfx = 0; CHECK(kbspan_seg(0, seg, false, out) == 1.0f && same6(out, seg)); g_kbfx = 1;
    CHECK(kbspan_seg(2, seg, false, out) == 1.0f && kbspan_seg(-1, seg, false, out) == 1.0f && same6(out, seg));
    // live kbfxid forces the Keyblade (both blades), as for the look
    KB.kb[KB_MAIN] = 0x029; g_kbfx_id = 0x02c;
    kbspan_seg(0, seg, false, out); CHECK(fabsf(len3(out, out + 3) - 1.30f) < 1e-4f);  // Detection Saber 130
    g_kbfx_id = 0;
    // 3. the change log: one line per change, none while it stays
    g_kbspan = 0;
    { int n = g_logs; kbspan_tick(); CHECK(g_logs == n); }  // (0 at start: nothing to say)
    g_kbspan = 1; { int n = g_logs; kbspan_tick(); kbspan_tick(); CHECK(g_logs == n + 1 && strstr(g_last_log, "kbspan 1")); }
    g_kbspan = 0; { int n = g_logs; kbspan_tick(); CHECK(g_logs == n + 1 && strstr(g_last_log, "kbspan 0")); }

    printf("kbspan_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}
