// (s269, review s268 F2) Is BotW's own minimap drawn? The pure part (no game headers: runtime/test/mm_test.c checks it
// on a fake pane tree in host memory). The exe's sora_hud_minimap_pane (sora_hud.c) fills SoraMmPane from the AppMap
// screen (screen slot 0x2a: the minimap in play, the full map in the pause menu, ui_layout.c); the DLL
// (sora_net_minimap.h, live "set mmhide 1") hides / fades the friends' dots with mmp_factor and logs it ("set mmprobe 1").
// Layout facts (the same as framepace.c shop_walk / bindings.c pane_child / pc_cursor.c pane_shown):
//   screen +0x30 its layout, layout +0x18 the root pane; screen +0x100 / +0x102 / +0xfe / +0xff = the native draw gate
//   (framepace.c screen_draws). A pane (nn::ui2d::Pane): +0x20 its children's list head, linked 8 bytes into each child
//   (+0x28 the first link, +8 in a link the next), +0x58 bit 0 visible, +0x59 its alpha, +0x5a its global alpha, +0xb8
//   its name (inline, 24 chars).
// GUESS (needs the one live look, recon/s269/S269_HELPER_MMHIDE.md): the pane is "N_MiniMap_00" (AppMap_00.bflyt, s267),
// and Pro HUD / no-minimap places / before the Slate hide it with a visible bit on its chain, its global alpha, or the
// screen's draw gate. Anything not found = "unknown" = the dots as today.
#pragma once
#include <stdint.h>
#include <string.h>

#define MMP_NAME "N_MiniMap_00"
#define MMP_DEPTH 24     // deepest chain kept (root .. N_MiniMap_00)
#define MMP_BUDGET 4096  // panes one search may look at (AppMap has many icon panes)
#define MMP_RETRY 30     // calls between two searches while it is not found

// what the exe reports (both sides from this header; size = sizeof, set by the caller, checked by the exe)
typedef struct {
    uint32_t size;
    int32_t screen;      // 1 = the AppMap screen (slot 0x2a) is there
    int32_t draws;       // 1 = its native draw gate is open
    int32_t mode;        // ui_layout_map_mode: -1 not known, 0 minimap, 1 full map
    int32_t found;       // 1 = N_MiniMap_00 found under its root (cached; looked up again when the chain breaks)
    int32_t vis;         // N_MiniMap_00's own visible bit
    int32_t chain_vis;   // it and every pane above it visible
    int32_t alpha;       // +0x59, 0..255
    int32_t glb_alpha;   // +0x5a, 0..255
    int32_t depth;       // panes from the root to it (root = 0)
    int32_t walked;      // panes looked at by the last search (0 = the cache held)
    int32_t searches;    // searches so far
    char hidden_at[24];  // the first hidden pane on its chain ("" = none)
} SoraMmPane;

typedef struct {
    uint64_t screen, layout, root, pane;
    uint64_t chain[MMP_DEPTH];  // root .. pane
    int depth;                  // index of pane in chain
    int wait;                   // calls left before the next search
} MmpCache;

static inline int mmp_in(uint64_t p, uint64_t n, uint64_t lo, uint64_t hi) { return p >= lo && p < hi && n <= hi - p; }
static inline uint64_t mmp_u64(uint64_t a) { return *(const uint64_t *)(uintptr_t)a; }
static inline uint8_t mmp_u8(uint64_t a) { return *(const uint8_t *)(uintptr_t)a; }

// depth-first search under pane for name; fills chain[0..depth] on success and returns the pane (0 = not found)
static inline uint64_t mmp_find(uint64_t pane, const char *name, uint64_t lo, uint64_t hi, int depth, int *budget, uint64_t *chain,
                         int *out_depth) {
    if (!mmp_in(pane, 0xd0, lo, hi) || depth >= MMP_DEPTH || --*budget < 0) return 0;
    chain[depth] = pane;
    if (!strncmp((const char *)(uintptr_t)(pane + 0xb8), name, 24)) {
        *out_depth = depth;
        return pane;
    }
    uint64_t head = pane + 0x20, link = mmp_u64(pane + 0x28);
    for (int k = 0; k < 512 && link != head && mmp_in(link, 16, lo, hi) && *budget > 0; k++) {
        uint64_t f = mmp_find(link - 8, name, lo, hi, depth + 1, budget, chain, out_depth);
        if (f) return f;
        link = mmp_u64(link + 8);
    }
    return 0;
}

static inline int mmp_screen_draws(uint64_t s) {
    return mmp_u8(s + 0x100) && !mmp_u8(s + 0x102) && mmp_u8(s + 0xfe) && mmp_u8(s + 0xff) != 2;
}

// screen = the AppMap screen pointer (0 = none), mode = ui_layout_map_mode(screen); [lo, hi) = the memory panes may be in
// (the guest heap). Fills o (keeps o->size); returns 1 when the pane was found, 0 when not (o still says what is known).
static inline int mmp_probe(MmpCache *c, uint64_t screen, int mode, uint64_t lo, uint64_t hi, SoraMmPane *o) {
    uint32_t size = o->size;
    int searches = o->searches;
    memset(o, 0, sizeof *o);
    o->size = size, o->searches = searches, o->mode = mode, o->depth = -1;
    if (!mmp_in(screen, 0x108, lo, hi)) { c->pane = 0; return 0; }
    o->screen = 1;
    o->draws = mmp_screen_draws(screen);
    uint64_t layout = mmp_u64(screen + 0x30);
    uint64_t root = mmp_in(layout, 0x20, lo, hi) ? mmp_u64(layout + 0x18) : 0;
    int ok = c->pane && c->screen == screen && c->layout == layout && c->root == root && c->depth >= 0 && c->depth < MMP_DEPTH &&
             c->chain[0] == root && c->chain[c->depth] == c->pane && mmp_in(c->pane, 0xd0, lo, hi) &&
             !strncmp((const char *)(uintptr_t)(c->pane + 0xb8), MMP_NAME, 24);
    for (int i = 0; ok && i <= c->depth; i++) ok = mmp_in(c->chain[i], 0xd0, lo, hi);
    if (!ok) {
        c->pane = 0;
        if (c->wait > 0 && c->screen == screen && c->layout == layout && c->root == root) { c->wait--; return 0; }
        c->screen = screen, c->layout = layout, c->root = root, c->wait = MMP_RETRY;
        int budget = MMP_BUDGET, d = -1;
        uint64_t p = mmp_find(root, MMP_NAME, lo, hi, 0, &budget, c->chain, &d);
        o->walked = MMP_BUDGET - (budget < 0 ? 0 : budget), o->searches = ++searches;
        if (!p) return 0;
        c->pane = p, c->depth = d, c->wait = 0;
    }
    uint64_t p = c->pane;
    o->found = 1, o->depth = c->depth;
    o->vis = mmp_u8(p + 0x58) & 1;
    o->alpha = mmp_u8(p + 0x59), o->glb_alpha = mmp_u8(p + 0x5a);
    o->chain_vis = 1;
    for (int i = 0; i <= c->depth; i++)
        if (!(mmp_u8(c->chain[i] + 0x58) & 1)) {
            o->chain_vis = 0;
            memcpy(o->hidden_at, (const char *)(uintptr_t)(c->chain[i] + 0xb8), 23);
            o->hidden_at[23] = 0;
            break;
        }
    return 1;
}

// The DLL's rule: the dots' alpha factor 0..1 (1 = as today). mask: which of BotW's signs to follow.
#define MMP_PANE 1    // N_MiniMap_00 or a pane above it hidden -> no dots
#define MMP_SCREEN 2  // the AppMap screen's draw gate closed -> no dots
#define MMP_ALPHA 4   // fade with N_MiniMap_00's global alpha
#define MMP_DIM 8     // (DLL) darken with Sora's s71 dim, as it darkens BotW's minimap under it
#define MMP_ALL 15
// ok = the exe answered (an older exe / no screen / not found = only what is known counts; nothing known = 1)
static inline float mmp_factor(int ok, const SoraMmPane *p, unsigned mask) {
    if (!ok || !p) return 1;
    if ((mask & MMP_SCREEN) && p->screen && !p->draws) return 0;
    if (!p->found) return 1;
    if ((mask & MMP_PANE) && !p->chain_vis) return 0;
    if (mask & MMP_ALPHA) return (float)(p->glb_alpha & 0xFF) / 255.0f;
    return 1;
}

// (MMP_DIM) 0xRRGGBB times (1 - dim), dim 0..1 (the exe's dim = a black quad of alpha dim over the game picture)
static inline uint32_t mmp_dim_rgb(uint32_t rgb, float dim) {
    if (!(dim > 0)) return rgb;
    float k = dim >= 1 ? 0 : 1 - dim;
    uint32_t r = (uint32_t)((float)(rgb >> 16 & 0xFF) * k + 0.5f), g = (uint32_t)((float)(rgb >> 8 & 0xFF) * k + 0.5f),
             b = (uint32_t)((float)(rgb & 0xFF) * k + 0.5f);
    return r << 16 | g << 8 | b;
}
