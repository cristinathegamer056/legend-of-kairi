// sora_updraft.h (s288 port of PR #21 updraft, DLL side): BotW's updraft columns for Sora's KH2 glide.
// user: "Sora's glide cannot increase height in a stream of air that a paraglider would normally react to". BotW lifts only
// its paraglider in an updraft; Sora's glide is the mod's own (KH2 physics), so it never rose. The columns of the main map
// (actors AscendingCurrent*) are NOT in the source (user ruling s288: no game-derived data in the public repo): the
// installer builds them on the player's PC from his own romfs (tools/sorabuild/levels/gen_updrafts.py, step "updrafts")
// into data/levels/updrafts.txt, read here once (read only; the DLL never writes it). A box of size 2 * Scale standing on its
// position, pushing along its up axis at WindSpeed * 30 u/s (BotW 1.9.0 0x71005f0d80, from the PR).
// File: line 1 "updrafts v1 <count>", then <count> lines of 16 numbers: pos x y z, R rows (9: world = R * local), half
// size x y z, push u/s. At most UPDRAFT_MAX rows, finite numbers only, half sizes >= 0; anything else = no updrafts (one
// log line). Missing file = no updrafts (one log line): an old data folder is safe with this DLL, and the new file is
// never read by an old DLL.
// The table is MainField coordinates: a shrine / Divine Beast / trial (own maps, own coordinates) could match a box by
// accident, so the lift is gated to the main field (updraft_field_ok in sora.c).
// The pure part (updraft_parse, updraft_find) has a host test: runtime/test/updraft_test.c.
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float pos[3], r[9], half[3], push; } Updraft;
#define UPDRAFT_MAX 1024
#define UPDRAFT_FILE_MAX (1u << 20)  // bytes (76 rows = ~9.5 KB)

// one number of a row: s advanced past it; false = no number / not finite / not followed by a blank or the line's end
static bool updraft_num(const char **s, float *out) {
    const char *p = *s;
    while (*p == ' ' || *p == '\t') p++;
    char *e;
    float v = strtof(p, &e);
    if (e == p || !isfinite(v) || !(*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n' || *e == 0)) return false;
    *out = v, *s = e;
    return true;
}

// txt (NUL-terminated) -> rows in out (cap rows). Returns the row count, or -1 (why = a short reason) when the text is
// not a valid table: then out holds nothing usable.
static int updraft_parse(const char *txt, Updraft *out, int cap, const char **why) {
    const char *dummy;
    if (!why) why = &dummy;
    *why = "";
    if (!txt) return *why = "no text", -1;
    int count = -1, used = 0;
    if (strncmp(txt, "updrafts v1 ", 12)) return *why = "not an updrafts v1 file", -1;
    const char *p = txt + 12;
    char *e;
    long c = strtol(p, &e, 10);
    if (e == p || c < 0 || c > UPDRAFT_MAX || c > cap) return *why = "bad row count", -1;
    count = (int)c, p = e;
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    if (*p != '\n' && *p != 0) return *why = "bad header", -1;
    while (*p) {
        if (*p == '\n') { p++; continue; }
        const char *ln = p;
        while (*ln == ' ' || *ln == '\t' || *ln == '\r') ln++;
        if (*ln == '\n' || *ln == 0) { p = ln; continue; }  // (a blank line)
        if (used >= count) return *why = "more rows than the header says", -1;
        float v[16];
        const char *q = p;
        for (int i = 0; i < 16; i++)
            if (!updraft_num(&q, &v[i])) return *why = "a row is not 16 finite numbers", -1;
        while (*q == ' ' || *q == '\t' || *q == '\r') q++;
        if (*q != '\n' && *q != 0) return *why = "a row has more than 16 numbers", -1;
        Updraft *u = &out[used];
        memcpy(u->pos, v, 12), memcpy(u->r, v + 3, 36), memcpy(u->half, v + 12, 12), u->push = v[15];
        if (u->half[0] < 0 || u->half[1] < 0 || u->half[2] < 0) return *why = "a negative half size", -1;
        used++, p = q;
    }
    if (used != count) return *why = "fewer rows than the header says", -1;
    return used;
}

// the push (u/s, world) of the column p (u, world) is in: its index, else -1
static int updraft_find(const Updraft *t, int n, const float p[3], float push[3]) {
    for (int i = 0; i < n; i++) {
        const Updraft *u = &t[i];
        float d[3] = {p[0] - u->pos[0], p[1] - u->pos[1], p[2] - u->pos[2]};
        if (fabsf(d[0]) > u->half[0] + u->half[1] * 2 || fabsf(d[2]) > u->half[2] + u->half[1] * 2) continue;  // (cheap reject)
        float l[3];  // local = R^T * d (R rows: world = R * local)
        for (int k = 0; k < 3; k++) l[k] = u->r[0 + k] * d[0] + u->r[3 + k] * d[1] + u->r[6 + k] * d[2];
        if (fabsf(l[0]) > u->half[0] || fabsf(l[2]) > u->half[2] || l[1] < 0 || l[1] > 2 * u->half[1]) continue;
        for (int k = 0; k < 3; k++) push[k] = u->r[k * 3 + 1] * u->push;  // (R's y column = the column's up)
        return i;
    }
    return -1;
}

#ifndef SORA_UPDRAFT_TEST
// ---- the game side (sora.c: LOG, sora_path) ----
static Updraft *g_ud;
static int g_ud_n;
static bool g_ud_loaded;
// once, on the first glide with updraft on (OFF = the file is never opened). SAVAGEWIND_SORA_UPDRAFTS = another file.
static void updraft_load(void) {
    if (g_ud_loaded) return;
    g_ud_loaded = true;
    const char *p = getenv("SAVAGEWIND_SORA_UPDRAFTS");
    if (!p || !*p) p = "D:/SoraBotW/data/levels/updrafts.txt";
    p = sora_path(p);
    FILE *f = fopen(p, "rb");
    if (!f) { LOG("[sora] updraft: no column table %s (the installer builds it): no map updrafts\n", p); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *txt = sz > 0 && sz <= (long)UPDRAFT_FILE_MAX ? (char *)malloc((size_t)sz + 1) : NULL;
    Updraft *t = (Updraft *)malloc(sizeof(Updraft) * UPDRAFT_MAX);
    if (!txt || !t || fread(txt, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f), free(txt), free(t);
        LOG("[sora] updraft: could not read %s (%ld bytes): no map updrafts\n", p, sz);
        return;
    }
    fclose(f);
    txt[sz] = 0;
    const char *why;
    int n = updraft_parse(txt, t, UPDRAFT_MAX, &why);
    free(txt);
    if (n < 0) { free(t); LOG("[sora] updraft: %s is not a valid table (%s): no map updrafts\n", p, why); return; }
    g_ud = t, g_ud_n = n;
    LOG("[sora] updraft: %d columns from %s\n", n, p);
}
static int updraft_at(const float p[3], float push[3]) {
    updraft_load();
    return g_ud ? updraft_find(g_ud, g_ud_n, p, push) : -1;
}
#endif
