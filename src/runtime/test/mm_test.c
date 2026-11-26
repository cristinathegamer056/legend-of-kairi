// host test for runtime/sora_hud_mm.h (s268, co-op item 3: friends on BotW's minimap): the disc's place / size for the
// window shapes + HUD sizes, the camera turn, the dot / rim-arrow choice. Numbers from recon/s267/S267_HELPER_MINIMAP.md.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/mm_test.c -o build_h/mm_test.exe
// (s269) + runtime/sora_hud_mmpane.h: the N_MiniMap_00 lookup on a fake pane tree in host memory, its cache / retry,
// bad pointers, and the DLL's hide / fade / dim rule.
#include <stdio.h>
#include <stdlib.h>
#include "sora_hud_mm.h"
#include "sora_hud_mmpane.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)
#define NEAR(a, b, e) (fabsf((a) - (b)) <= (e))

// fake guest memory: the screen at +0, its layout at +0x4000, panes from +0x5000 (0x100 apart)
static uint8_t *g_mem;
static uint64_t g_lo, g_hi;
#define AT(off) (g_lo + (off))
static void w64(uint64_t a, uint64_t v) { *(uint64_t *)(uintptr_t)a = v; }
static uint64_t mk_pane(int i, const char *name, int vis, int glb) {
    uint64_t p = AT(0x5000 + 0x100 * (uint64_t)i);
    memset((void *)(uintptr_t)p, 0, 0x100);
    w64(p + 0x20, p + 0x20), w64(p + 0x28, p + 0x20);  // (no children: the head links to itself)
    *(uint8_t *)(uintptr_t)(p + 0x58) = (uint8_t)(vis ? 1 : 0);
    *(uint8_t *)(uintptr_t)(p + 0x59) = 255, *(uint8_t *)(uintptr_t)(p + 0x5a) = (uint8_t)glb;
    strncpy((char *)(uintptr_t)(p + 0xb8), name, 24);
    return p;
}
static void kids(uint64_t parent, const uint64_t *c, int n) {  // (parent +0x28 = first link = child + 8; link + 8 = next)
    uint64_t head = parent + 0x20;
    w64(head + 8, n ? c[0] + 8 : head);
    for (int i = 0; i < n; i++) w64(c[i] + 8 + 8, i + 1 < n ? c[i + 1] + 8 : head);
}
static void vis(uint64_t p, int v) { *(uint8_t *)(uintptr_t)(p + 0x58) = (uint8_t)(v ? 1 : 0); }
static void mmpane_tests(void) {
    g_mem = calloc(1, 0x10000);
    g_lo = (uint64_t)(uintptr_t)g_mem, g_hi = g_lo + 0x10000;
    uint64_t scr = AT(0), lay = AT(0x4000);
    w64(scr + 0x30, lay);
    *(uint8_t *)(uintptr_t)(scr + 0x100) = 1, *(uint8_t *)(uintptr_t)(scr + 0xfe) = 1;  // (the draw gate open)
    uint64_t root = mk_pane(0, "RootPane", 1, 255), io = mk_pane(1, "N_InOut_00", 1, 255), big = mk_pane(2, "N_MapBig_00", 1, 255),
             mm = mk_pane(3, MMP_NAME, 1, 255), icon = mk_pane(4, "N_MapIcon_00", 1, 255);
    w64(lay + 0x18, root);
    uint64_t r_k[2] = {big, io}, io_k[2] = {icon, mm};
    kids(root, r_k, 2), kids(io, io_k, 2);
    MmpCache c = {0};
    SoraMmPane p = {.size = sizeof p};
    // found under root > N_InOut_00, everything shown: dots as today
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && p.screen && p.draws && p.found && p.vis && p.chain_vis && p.depth == 2);
    CHECK(p.walked == 5 && p.searches == 1 && p.glb_alpha == 255 && p.mode == 0 && p.size == sizeof p);
    CHECK(mmp_factor(1, &p, MMP_ALL) == 1);
    // the next call uses the cache (no walk)
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && p.walked == 0 && p.searches == 1);
    // a pane above it hidden (Pro HUD GUESS): no dots, and its name is told
    vis(io, 0);
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && p.vis && !p.chain_vis && !strcmp(p.hidden_at, "N_InOut_00"));
    CHECK(mmp_factor(1, &p, MMP_ALL) == 0 && mmp_factor(1, &p, MMP_SCREEN | MMP_ALPHA) == 1);
    vis(io, 1);
    // its own bit
    vis(mm, 0);
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && !p.vis && !p.chain_vis && !strcmp(p.hidden_at, MMP_NAME) && mmp_factor(1, &p, MMP_PANE) == 0);
    vis(mm, 1);
    // its global alpha: faded with it (only with MMP_ALPHA)
    *(uint8_t *)(uintptr_t)(mm + 0x5a) = 102;
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && p.glb_alpha == 102);
    CHECK(NEAR(mmp_factor(1, &p, MMP_ALL), 0.4f, 1e-4f) && mmp_factor(1, &p, MMP_PANE | MMP_SCREEN) == 1);
    *(uint8_t *)(uintptr_t)(mm + 0x5a) = 255;
    // the screen's draw gate closed (+0x102 set): no dots with MMP_SCREEN
    *(uint8_t *)(uintptr_t)(scr + 0x102) = 1;
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && !p.draws && mmp_factor(1, &p, MMP_ALL) == 0 && mmp_factor(1, &p, MMP_PANE) == 1);
    *(uint8_t *)(uintptr_t)(scr + 0x102) = 0;
    *(uint8_t *)(uintptr_t)(scr + 0xff) = 2;
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && !p.draws);
    *(uint8_t *)(uintptr_t)(scr + 0xff) = 0;
    // no screen / a screen outside memory: nothing known = dots as today
    CHECK(mmp_probe(&c, 0, -1, g_lo, g_hi, &p) == 0 && !p.screen && !p.found && p.mode == -1 && mmp_factor(1, &p, MMP_ALL) == 1);
    CHECK(mmp_probe(&c, g_hi - 8, -1, g_lo, g_hi, &p) == 0 && !p.screen && mmp_factor(1, &p, MMP_ALL) == 1);
    CHECK(mmp_factor(0, &p, MMP_ALL) == 1 && mmp_factor(1, NULL, MMP_ALL) == 1);
    // the screen back: searched again, found again
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && p.searches == 2 && p.walked == 5);
    // the pane goes (its name gone): not found = as today, then no new search for MMP_RETRY calls, then one
    memset((void *)(uintptr_t)(mm + 0xb8), 0, 24);
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 0 && p.screen && !p.found && p.searches == 3 && mmp_factor(1, &p, MMP_ALL) == 1);
    int extra = 0;
    for (int i = 0; i < MMP_RETRY; i++) mmp_probe(&c, scr, 0, g_lo, g_hi, &p), extra += p.walked != 0;
    CHECK(extra == 0 && p.searches == 3);
    strncpy((char *)(uintptr_t)(mm + 0xb8), MMP_NAME, 24);
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && p.searches == 4);
    // a new layout (the screen rebuilt): searched at once
    uint64_t lay2 = AT(0x4800);
    w64(lay2 + 0x18, root), w64(scr + 0x30, lay2);
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && p.searches == 5);
    // bad pointers: a root outside memory, a child link outside memory, a link loop: no crash, bounded
    w64(lay2 + 0x18, 0x1234);
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 0 && !p.found);
    w64(lay2 + 0x18, root);
    c.wait = 0;
    w64(io + 0x28, 0xdead0000);
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 0 && !p.found);
    w64(io + 0x28, icon + 8), w64(icon + 0x10, icon + 8);  // (icon's next = itself)
    c.wait = 0;
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 0 && !p.found && p.walked <= MMP_BUDGET + 1);
    kids(io, io_k, 2);
    c.wait = 0;
    CHECK(mmp_probe(&c, scr, 0, g_lo, g_hi, &p) == 1 && p.found);
    // the dim: 0xRRGGBB x (1 - dim)
    CHECK(mmp_dim_rgb(0xE83A3A, 0) == 0xE83A3A && mmp_dim_rgb(0xE83A3A, 0.5f) == 0x741D1D && mmp_dim_rgb(0xFFFFFF, 1) == 0);
    CHECK(mmp_dim_rgb(0x3CC850, -1) == 0x3CC850);
    free(g_mem);
}

int main(void) {
    // 2560 x 1440, HUD size 100: the 14 marks' centre (2299.5, 1200), radius 158, 1 px / m
    MmGeo g = mm_geo(0, 0, 2560, 1440, 1, false);
    CHECK(NEAR(g.cx, 2299.5f, 0.01f) && NEAR(g.cy, 1200, 0.01f) && NEAR(g.r, 158, 0.01f) && NEAR(g.ppm, 1, 1e-5f));
    // 1920 x 1080 = s57's numbers scaled (0.75)
    g = mm_geo(0, 0, 1920, 1080, 1, false);
    CHECK(NEAR(g.cx, 1920 - 195.375f, 0.01f) && NEAR(g.cy, 900, 0.01f) && NEAR(g.r, 118.5f, 0.01f));
    // HUD size 80: from the bottom-right corner, everything x 0.8
    g = mm_geo(0, 0, 2560, 1440, 0.8f, false);
    CHECK(NEAR(g.cx, 2560 - 208.4f, 0.01f) && NEAR(g.cy, 1440 - 192, 0.01f) && NEAR(g.r, 126.4f, 0.01f) && NEAR(g.ppm, 0.8f, 1e-5f));
    // 21:9 3440 x 1440: the HUD's 16:9 band is centred (its right edge at 1720 + 1280 = 3000)
    g = mm_geo(0, 0, 3440, 1440, 1, false);
    CHECK(NEAR(g.cx, 3000 - 260.5f, 0.01f) && NEAR(g.cy, 1200, 0.01f) && NEAR(g.r, 158, 0.01f));
    // a game image letterboxed in the window (vx, vy): moves with it
    g = mm_geo(100, 50, 2560, 1440, 1, false);
    CHECK(NEAR(g.cx, 2399.5f, 0.01f) && NEAR(g.cy, 1250, 0.01f));
    // 4:3 1920 x 1440 (taller): band 1920 x 1080 centred; HUD at edges = the window's bottom edge
    g = mm_geo(0, 0, 1920, 1440, 1, false);
    CHECK(NEAR(g.cx, 1920 - 195.375f, 0.01f) && NEAR(g.cy, 1260 - 180, 0.01f) && NEAR(g.r, 118.5f, 0.01f));
    g = mm_geo(0, 0, 1920, 1440, 1, true);
    CHECK(NEAR(g.cy, 1440 - 180, 0.01f));
    // a bad HUD size = 100
    MmGeo g0 = mm_geo(0, 0, 2560, 1440, 0, false);
    CHECK(NEAR(g0.r, 158, 0.01f));

    // the turn: camera north (0, -1) = north up, east right; camera east (1, 0) = east up, south right
    float mx, my;
    CHECK(mm_turn(10, -20, 0, -1, &mx, &my) && NEAR(mx, 10, 1e-4f) && NEAR(my, -20, 1e-4f));
    CHECK(mm_turn(30, 0, 1, 0, &mx, &my) && NEAR(mx, 0, 1e-4f) && NEAR(my, -30, 1e-4f));  // (east, camera east: straight up)
    CHECK(mm_turn(0, 30, 1, 0, &mx, &my) && NEAR(mx, 30, 1e-4f) && NEAR(my, 0, 1e-4f));   // (south, camera east: right)
    CHECK(mm_turn(0, 30, 0, 1, &mx, &my) && NEAR(mx, 0, 1e-4f) && NEAR(my, -30, 1e-4f));  // (south, camera south: up)
    CHECK(mm_turn(5, 5, 0, -7, &mx, &my) && NEAR(mx, 5, 1e-4f));  // (forward not unit: normalised)
    CHECK(!mm_turn(5, 5, 0, 0, &mx, &my));                          // (no forward: no dot)
    // the length is kept for any camera: mark_150038_1, Link (495.33, 3407.92) -> Lakeside Stable (529.6, 3450.9) = 55.0 px
    for (int a = 0; a < 360; a += 15) {
        float r = a * 3.14159265f / 180;
        CHECK(mm_turn(529.6f - 495.33f, 3450.9f - 3407.92f, sinf(r), -cosf(r), &mx, &my) && NEAR(sqrtf(mx * mx + my * my), 54.97f, 0.02f));
    }
    // the s267 fit for marks 2 / 3: the same stable at 84.9 m turns by the camera's turn (11.1 deg)
    {
        float dx = 529.6f - 491.64f, dz = 3450.9f - 3374.95f, x1, y1, x2, y2;
        float r1 = 0.3f, r2 = 0.3f + 11.1f * 3.14159265f / 180;
        mm_turn(dx, dz, sinf(r1), -cosf(r1), &x1, &y1), mm_turn(dx, dz, sinf(r2), -cosf(r2), &x2, &y2);
        float t = acosf((x1 * x2 + y1 * y2) / (sqrtf(x1 * x1 + y1 * y1) * sqrtf(x2 * x2 + y2 * y2))) * 180 / 3.14159265f;
        CHECK(NEAR(t, 11.1f, 0.05f) && NEAR(sqrtf(x1 * x1 + y1 * y1), 84.91f, 0.05f));
    }

    // dot or arrow: 1440p, radius 158, edge = 158 - 11 = 147; the arrow's centre 158 - 7 = 151
    g = mm_geo(0, 0, 2560, 1440, 1, false);
    float px, py, ux, uy;
    CHECK(MM_DOT_R == 10 && MM_ARROW_LEN == 20);  // (user s268: 20 px dots)
    CHECK(!mm_place(&g, 100, 0, &px, &py, &ux, &uy) && NEAR(px, 2399.5f, 0.01f) && NEAR(py, 1200, 0.01f));
    CHECK(!mm_place(&g, 0, -147, &px, &py, &ux, &uy) && NEAR(py, 1053, 0.01f));
    CHECK(!mm_place(&g, 0, 0, &px, &py, &ux, &uy) && NEAR(px, g.cx, 0.01f));  // (on top of Link: a dot on the arrow)
    CHECK(mm_place(&g, 0, -148, &px, &py, &ux, &uy) && NEAR(ux, 0, 1e-5f) && NEAR(uy, -1, 1e-5f) && NEAR(py, 1200 - 151, 0.01f));
    CHECK(mm_place(&g, 3000, 4000, &px, &py, &ux, &uy) && NEAR(ux, 0.6f, 1e-5f) && NEAR(uy, 0.8f, 1e-5f) &&
          NEAR(px, 2299.5f + 0.6f * 151, 0.01f) && NEAR(py, 1200 + 0.8f * 151, 0.01f));
    // the arrow's tip pokes past the rim by a few px only (151 + 10 = 161)
    CHECK(151 + MM_ARROW_LEN * 0.5f - 158 > 0 && 151 + MM_ARROW_LEN * 0.5f - 158 <= 4);
    // HUD size 80: the edge and the arrow scale too (edge 126.4 - 8.8 = 117.6)
    g = mm_geo(0, 0, 2560, 1440, 0.8f, false);
    CHECK(!mm_place(&g, 0, -146, &px, &py, &ux, &uy) && NEAR(py, g.cy - 116.8f, 0.01f));  // (146 m = 116.8 px)
    CHECK(mm_place(&g, 0, -148, &px, &py, &ux, &uy));

    // colours: red, green, purple, then again
    CHECK(mm_colour(0) == 0xE83A3A && mm_colour(1) == 0x3CC850 && mm_colour(2) == 0xA855F0 && mm_colour(3) == 0xE83A3A && mm_colour(-1) == 0xA855F0);

    mmpane_tests();  // (s269)
    printf("mm_test: %d/%d passed\n", runs - fails, runs);
    return fails != 0;
}
