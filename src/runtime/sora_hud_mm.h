// (s268, co-op item 3) friends on BotW's minimap: the pure math (exe sora_hud.c draws it; runtime/test/mm_test.c checks it).
// Measured in s267 (recon/s267/S267_HELPER_MINIMAP.md) from 14 F5 marks at 2560 x 1440: the disc's centre (Link's arrow)
// = 260.5 px from the 16:9 band's right edge, 240 px from its bottom, radius 158 px, 1 px per metre; all of it scales
// with the band's height (1440 = 1) and with Gameplay > HUD size (anchor = the band's bottom-right corner, as
// hud_scale.h's hud_scale_span does it for the minimap's mask). The map turns with the camera: the DLL sends offsets
// already turned (x right, y down, metres from Link), see mm_turn.
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define MM_RIGHT_PX 260.5f  // at 1440p, HUD size 100: the disc centre from the band's right edge
#define MM_BOTTOM_PX 240.0f //   ... from its bottom edge
#define MM_RADIUS_PX 158.0f //   the disc's radius (= 158 m)
#define MM_DOT_R 10.0f      // the friend's dot: 20 px across at 1440p, its dark rim included (user s268: 10 px, then "double it")
#define MM_DOT_RIM 3.0f     //   the dark rim's width
#define MM_ARROW_LEN 20.0f  // the edge arrow (a friend off the disc): tip to base, on the rim
#define MM_ARROW_HALF 9.0f  //   half its base

typedef struct { float cx, cy, r, ppm; } MmGeo;  // window px: disc centre, radius, px per metre (= px per 1440p px)

// (vx, vy, vw, vh) = the game image in the window (device.c); hud_s = HUD size / 100; edges_tall = "HUD at edges" on a
// window taller than 16:9 (the bottom-right zone then sits at the window's bottom edge, hud_edge_dy)
static inline MmGeo mm_geo(float vx, float vy, float vw, float vh, float hud_s, bool edges_tall) {
    float bw = fminf(vw, vh * 16.0f / 9.0f), bh = fminf(vh, vw * 9.0f / 16.0f);
    float right = vx + vw * 0.5f + bw * 0.5f, bottom = edges_tall ? vy + vh : vy + vh * 0.5f + bh * 0.5f;
    if (!(hud_s > 0)) hud_s = 1;
    float u = bh / 1440.0f * hud_s;
    MmGeo g = {right - MM_RIGHT_PX * u, bottom - MM_BOTTOM_PX * u, MM_RADIUS_PX * u, u};
    return g;
}

// World offset (dx, dz: friend - Link, BotW metres, x east, z south) -> the minimap's frame (x right, y down, metres),
// camera forward flattened (fx, fz). Camera facing north (0, -1) = north up: (dx, dz) unchanged.
static inline bool mm_turn(float dx, float dz, float fx, float fz, float *mx, float *my) {
    float l = sqrtf(fx * fx + fz * fz);
    if (!(l > 1e-4f)) return false;
    fx /= l, fz /= l;
    *mx = -dx * fz + dz * fx, *my = -(dx * fx + dz * fz);
    return true;
}

// Where a dot goes: false = inside the disc at (*px, *py) (window px); true = off the disc: an arrow on the rim at the
// same angle, (*px, *py) = its centre, (*ux, *uy) = the outward unit direction
static inline bool mm_place(const MmGeo *g, float mx, float my, float *px, float *py, float *ux, float *uy) {
    float x = mx * g->ppm, y = my * g->ppm, d = sqrtf(x * x + y * y);
    float edge = g->r - (MM_DOT_R + 1.0f) * g->ppm;
    if (d <= edge || d < 1e-3f) { *px = g->cx + x, *py = g->cy + y, *ux = 0, *uy = -1; return false; }
    *ux = x / d, *uy = y / d;
    float c = g->r - MM_ARROW_LEN * 0.35f * g->ppm;  // (a little inside the rim; its tip pokes out like BotW's rim pins)
    *px = g->cx + *ux * c, *py = g->cy + *uy * c;
    return true;
}

// The friends' colours (user s268): slot 0 red, 1 green, 2 purple (0xRRGGBB)
static inline uint32_t mm_colour(int slot) {
    static const uint32_t C[3] = {0xE83A3A, 0x3CC850, 0xA855F0};
    return C[((slot % 3) + 3) % 3];
}
