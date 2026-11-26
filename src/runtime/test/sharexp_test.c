// (s270) host test of runtime/sora_sharexp.h (co-op item 5, shared EXP rows + the near rule)
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/sharexp_test.c -o build_h/sharexp_test.exe
#include <stdio.h>
#include <stdarg.h>
#include "sora_sharexp.h"

static int pass, fail;
#define CHECK(c) do { if (c) pass++; else { fail++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static int nlog;
static void lg(const char *f, ...) { va_list a; va_start(a, f); va_end(a); nlog++; }

static void reset(void) { memset(&SX, 0, sizeof SX); g_sharexp = true, g_sharexp_r = 50, g_sharexp_ttl = 10000; }

int main(void) {
    float d = -1;
    // near rule
    float me[3] = {0, 0, 0}, k1[3] = {30, 0, 40}, k2[3] = {30, 0, 41}, k3[3] = {0, 50, 0};
    CHECK(sx_near(me, k1, 50, &d) && d > 49.99f && d < 50.01f);  // (exactly 50 m = near)
    CHECK(!sx_near(me, k2, 50, &d) && d > 50);
    CHECK(sx_near(me, k3, 50, &d));                                // (height counts too: straight up 50 m)
    CHECK(sx_near(me, me, 50, &d) && d == 0);
    float bad[3] = {NAN, 0, 0}, inf[3] = {INFINITY, 0, 0};
    CHECK(!sx_near(me, bad, 50, &d));
    CHECK(!sx_near(me, inf, 50, &d));

    // note + take
    reset();
    SxRow r;
    sx_note(0x1234, 1, 12, 1000);
    CHECK(SX.noted == 1);
    CHECK(!sx_take(0x9999, 1500, &r));                      // other enemy
    CHECK(sx_take(0x1234, 1500, &r) && r.peer == 1 && r.dist == 12);
    CHECK(!sx_take(0x1234, 1600, &r));                      // taken once only
    sx_note(0, 0, 1, 1000);                                 // hid 0 (spawned) never notes
    CHECK(SX.noted == 1);
    CHECK(!sx_take(0, 1000, &r));

    // ttl
    reset();
    sx_note(0x55, 0, 5, 1000);
    CHECK(!sx_take(0x55, 1000 + 10001, &r) && SX.expired == 1);
    sx_note(0x56, 0, 5, 1000);
    CHECK(sx_take(0x56, 1000 + 10000, &r));                 // (exactly ttl = still fresh)

    // a repeat refreshes, no second row
    reset();
    sx_note(0x77, 0, 5, 1000);
    sx_note(0x77, 2, 9, 4000);
    int used = 0;
    for (int i = 0; i < SX_ROWS; i++) used += SX.r[i].used;
    CHECK(used == 1);
    CHECK(sx_take(0x77, 13500, &r) && r.peer == 2);         // (fresh from 4000)

    // full table: an expired row is reused first, else the oldest
    reset();
    for (int i = 0; i < SX_ROWS; i++) sx_note(0x100 + i, 0, 1, 1000 + (uint64_t)i);
    sx_note(0x200, 0, 1, 2000);                             // none expired: the oldest (0x100) goes
    CHECK(!sx_take(0x100, 2001, &r));
    CHECK(sx_take(0x200, 2001, &r));
    CHECK(sx_take(0x101, 2001, &r));
    reset();
    for (int i = 0; i < SX_ROWS; i++) sx_note(0x100 + i, 0, 1, i == 5 ? 1 : 20000);
    sx_note(0x300, 0, 1, 20000 + 5);                        // row of 0x105 (at 1) is expired: reused
    CHECK(!sx_take(0x105, 20006, &r));
    CHECK(sx_take(0x104, 20006, &r) && sx_take(0x106, 20006, &r) && sx_take(0x300, 20006, &r));

    // live keys
    reset();
    nlog = 0;
    CHECK(sx_live_set("sharexp", 0, lg) && !g_sharexp);
    CHECK(sx_live_set("sharexp", 1, lg) && g_sharexp);
    CHECK(sx_live_set("sharexpr", 0, lg) && g_sharexp_r == 1);
    CHECK(sx_live_set("sharexpr", 99999, lg) && g_sharexp_r == 1000);
    CHECK(sx_live_set("sharexpttl", 1, lg) && g_sharexp_ttl == 500);
    CHECK(sx_live_set("sharexplog", 1, lg) && g_sharexp_log);
    CHECK(!sx_live_set("sharexpx", 1, lg) && !sx_live_set("exp", 1, lg));
    CHECK(nlog == 6);
    sx_note(0x10, 0, 1, 1);
    CHECK(sx_live_set("sharexp", 0, NULL));                 // off clears the rows
    g_sharexp = true;
    CHECK(!sx_take(0x10, 2, &r));

    printf("sharexp_test: %d/%d passed\n", pass, pass + fail);
    return fail != 0;
}
