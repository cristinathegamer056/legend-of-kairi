// host test for runtime/sora_warp_core.h + the NET_WARP packet in runtime/sora_net_proto.h (s269 warpcore, co-op item 4: BotW's
// own fast travel to a friend through the Travel Medallion slot): the gate order, the spot choice, the ask timing, the
// Medallion slot's restore state machine, its hot-reload env text, the note's fade, and the wire format.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/warp_test.c -o build_h/warp_test.exe
#include <stdio.h>
#include <stdlib.h>
#include "sora_net_proto.h"
#include "sora_warp_core.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)
#define NEAR(a, b, e) (fabsf((a) - (b)) <= (e))

static void gate_tests(void) {
    WgIn g = {0};
    CHECK(wg_gate(NULL) == WG_NO_SITES);
    CHECK(wg_gate(&g) == WG_NO_SITES);
    g.sites = true;
    CHECK(wg_gate(&g) == WG_NO_CPU);
    g.cpu = true;
    CHECK(wg_gate(&g) == WG_NO_UI);
    g.ui = true;
    CHECK(wg_gate(&g) == WG_LIFE_UNKNOWN);  // (all game checks pass, the life was not read)
    g.hart_ok = true, g.hart = 0;
    CHECK(wg_gate(&g) == WG_DEAD);
    g.hart = -4;
    CHECK(wg_gate(&g) == WG_DEAD);
    g.hart = 1;  // (a quarter heart is alive)
    CHECK(wg_gate(&g) == WG_OK);
    // the map's order: prohibition, then the event bit, then the transition, then the pause; the first block wins
    WgIn b = g;
    b.prohibited = b.event = b.trans = b.paused = true;
    CHECK(wg_gate(&b) == WG_PROHIBITED);
    b.prohibited = false;
    CHECK(wg_gate(&b) == WG_EVENT);
    b.event = false;
    CHECK(wg_gate(&b) == WG_TRANS);
    b.trans = false;
    CHECK(wg_gate(&b) == WG_PAUSED);
    b.paused = false;
    CHECK(wg_gate(&b) == WG_OK);
    // no UI manager blocks before anything that reads through it
    b.ui = false, b.event = true;
    CHECK(wg_gate(&b) == WG_NO_UI);
    for (int i = 0; i < WG_N; i++) CHECK(WG_WHY[i] && WG_WHY[i][0]);
}

static void spot_tests(void) {
    const float ok[3] = {-1200.5f, 245.0f, 1800.25f};
    CHECK(wsp_pos_ok(ok));
    CHECK(!wsp_pos_ok(NULL));
    float p[3] = {5000, 1800, -4000};
    CHECK(wsp_pos_ok(p));  // (the range's edges are in)
    p[0] = 5000.5f;
    CHECK(!wsp_pos_ok(p));
    p[0] = 0, p[1] = -100.5f;
    CHECK(!wsp_pos_ok(p));
    p[1] = 1800.5f;
    CHECK(!wsp_pos_ok(p));
    p[1] = 0, p[2] = 4001;
    CHECK(!wsp_pos_ok(p));
    p[2] = NAN;
    CHECK(!wsp_pos_ok(p));
    p[2] = INFINITY;
    CHECK(!wsp_pos_ok(p));

    CHECK(NEAR(wsp_deg_norm(0), 0, 1e-4f));
    CHECK(NEAR(wsp_deg_norm(180), -180, 1e-4f));
    CHECK(NEAR(wsp_deg_norm(-180), -180, 1e-4f));
    CHECK(NEAR(wsp_deg_norm(190), -170, 1e-3f));
    CHECK(NEAR(wsp_deg_norm(-190), 170, 1e-3f));
    CHECK(NEAR(wsp_deg_norm(720 + 45), 45, 1e-3f));
    CHECK(wsp_deg_norm(NAN) == 0 && wsp_deg_norm(INFINITY) == 0);

    const float live[3] = {100, 200, 300}, save[3] = {110, 205, 290};
    float o[3] = {0}, d = 99;
    // not on MainField: nothing, whatever else is there
    CHECK(wsp_choose(NWF_GROUND | NWF_SAVE, live, 10, save, 20, o, &d) == WSP_NONE);
    // on the ground: his feet, his facing - 180 (the game's own Medallion rule)
    CHECK(wsp_choose(NWF_MAINFIELD | NWF_GROUND | NWF_SAVE, live, 10, save, 20, o, &d) == WSP_LIVE);
    CHECK(o[0] == 100 && o[1] == 200 && o[2] == 300 && NEAR(d, -170, 1e-3f));
    // in the air / swimming: the last safe spot
    CHECK(wsp_choose(NWF_MAINFIELD | NWF_SAVE, live, 10, save, 20, o, &d) == WSP_SAVE);
    CHECK(o[0] == 110 && o[1] == 205 && o[2] == 290 && NEAR(d, -160, 1e-3f));
    // a bad live spot falls back to the safe one; a bad safe spot with no ground = nothing
    const float bad[3] = {NAN, 0, 0}, far[3] = {9000, 0, 0};
    CHECK(wsp_choose(NWF_MAINFIELD | NWF_GROUND | NWF_SAVE, bad, 10, save, 20, o, &d) == WSP_SAVE);
    CHECK(wsp_choose(NWF_MAINFIELD | NWF_GROUND | NWF_SAVE, far, 10, save, 20, o, &d) == WSP_SAVE);
    CHECK(wsp_choose(NWF_MAINFIELD | NWF_SAVE, live, 10, far, 20, o, &d) == WSP_NONE);
    CHECK(wsp_choose(NWF_MAINFIELD, live, 10, save, 20, o, &d) == WSP_NONE);  // (no ground, no save read)
    // the facing wraps: 170 - 180 = -10, -170 - 180 = -350 = 10
    CHECK(wsp_choose(NWF_MAINFIELD | NWF_GROUND, live, -170, save, 0, o, &d) == WSP_LIVE && NEAR(d, 10, 1e-3f));
    CHECK(wsp_choose(NWF_MAINFIELD | NWF_GROUND, live, NAN, save, 0, o, &d) == WSP_LIVE && d == 0);  // (a NaN facing: 0, the spot still good)
}

static void ask_tests(void) {
    uint64_t next = 0, t0 = 1000;
    // UDP: at once, then every WA_RESEND_MS, give up at WA_TIMEOUT_MS
    CHECK(wa_step(1000, t0, &next, false) == WAA_SEND && next == 1000 + WA_RESEND_MS);
    CHECK(wa_step(1100, t0, &next, false) == WAA_WAIT);
    CHECK(wa_step(1000 + WA_RESEND_MS, t0, &next, false) == WAA_SEND);
    int sends = 2;
    for (uint64_t t = 1000 + WA_RESEND_MS + 1; t < t0 + WA_TIMEOUT_MS; t += 16) sends += wa_step(t, t0, &next, false) == WAA_SEND;
    CHECK(sends == (WA_TIMEOUT_MS + WA_RESEND_MS - 1) / WA_RESEND_MS);  // (7 ASKs in 2.5 s at 400 ms)
    CHECK(wa_step(t0 + WA_TIMEOUT_MS, t0, &next, false) == WAA_GIVE_UP);
    // Melty (reliable): once, then only the timeout
    next = 0;
    CHECK(wa_step(1000, t0, &next, true) == WAA_SEND);
    int again = 0;
    for (uint64_t t = 1001; t < t0 + WA_TIMEOUT_MS; t += 16) again += wa_step(t, t0, &next, true) == WAA_SEND;
    CHECK(again == 0);
    CHECK(wa_step(t0 + WA_TIMEOUT_MS + 5, t0, &next, true) == WAA_GIVE_UP);
}

// one frame of the run machine
static int step(WarpRun *r, uint64_t now, bool loading, bool event, bool sw1, bool title, const char **why) {
    WrIn in = {.now = now, .loading = loading, .event = event, .sw1 = sw1, .title = title};
    return wr_step(r, &in, why);
}

static void run_tests(void) {
    WarpRun r;
    const char *why = NULL;
    memset(&r, 0, sizeof r);
    CHECK(step(&r, 5, false, true, true, false, &why) == WRA_NONE);  // (idle: nothing)
    CHECK(wr_step(NULL, NULL, NULL) == WRA_NONE);

    // the real warp: call, warp-out (event + 0x1), the card, the arrival 0x1 -> restore at the arrival, not before
    wr_begin(&r, 10000);
    CHECK(r.st == WR_OUT && r.t0 == 10000);
    uint64_t t = 10000;
    CHECK(step(&r, t += 16, false, false, false, false, &why) == WRA_NONE);  // (the event starts a frame later)
    for (int i = 0; i < 200; i++) CHECK(step(&r, t += 16, false, true, true, false, &why) == WRA_NONE);  // (Demo005_0, 120 + frames)
    CHECK(r.ev_seen);
    for (int i = 0; i < 60; i++) CHECK(step(&r, t += 16, false, false, false, false, &why) == WRA_NONE);  // (the black fade: no card yet)
    CHECK(r.st == WR_OUT);
    for (int i = 0; i < 300; i++) CHECK(step(&r, t += 16, true, false, i & 1, false, &why) == WRA_NONE);  // (loading: NEVER a restore, even with 0x1 noise)
    CHECK(r.st == WR_LOAD && r.load_seen);
    CHECK(step(&r, t += 16, false, false, false, false, &why) == WRA_NONE);  // (card gone, Link not spawned yet)
    CHECK(r.st == WR_ARRIVE);
    CHECK(step(&r, t += 16, false, false, false, false, &why) == WRA_NONE);
    CHECK(step(&r, t += 16, false, false, true, false, &why) == WRA_RESTORE);  // (Demo005_1: Link 0x1)
    CHECK(strstr(why, "arrival") != NULL);
    wr_end(&r);
    CHECK(r.st == WR_IDLE);

    // the card goes and 0x1 is up in that same frame
    wr_begin(&r, 1);
    step(&r, 100, true, false, false, false, &why);
    CHECK(step(&r, 116, false, false, true, false, &why) == WRA_RESTORE);

    // the game did not take the call: nothing for 5 s -> restore (no load ever happened, the slot was never read)
    wr_begin(&r, 20000);
    t = 20000;
    while (t < 20000 + WR_START_MS - 16) CHECK(step(&r, t += 16, false, false, false, false, &why) == WRA_NONE);
    CHECK(step(&r, 20000 + WR_START_MS, false, false, false, false, &why) == WRA_RESTORE && strstr(why, "did not take") != NULL);

    // a borrow for BotW's own Travel dialog (4b): a longer start window, then the same rules
    wr_begin_ms(&r, 20000, 30000);
    CHECK(step(&r, 20000 + WR_START_MS + 1, false, false, false, false, &why) == WRA_NONE);
    CHECK(step(&r, 20000 + 29999, false, false, false, false, &why) == WRA_NONE);
    CHECK(step(&r, 20000 + 30000, false, false, false, false, &why) == WRA_RESTORE);
    wr_begin_ms(&r, 20000, 30000);
    step(&r, 23000, false, true, true, false, &why);  // (BotW's own e81a30 after the menu closed: the warp-out)
    step(&r, 24000, true, false, false, false, &why);
    CHECK(step(&r, 30000, false, false, true, false, &why) == WRA_RESTORE && strstr(why, "arrival") != NULL);

    // the warp-out started but ended without a load: restore WR_NOLOAD_MS after the last event frame
    wr_begin(&r, 30000);
    step(&r, 30100, false, true, false, false, &why);
    step(&r, 31000, false, true, false, false, &why);
    CHECK(step(&r, 31000 + WR_NOLOAD_MS - 1, false, false, false, false, &why) == WRA_NONE);
    CHECK(step(&r, 31000 + WR_NOLOAD_MS, false, false, false, false, &why) == WRA_RESTORE && strstr(why, "without a load") != NULL);

    // a long warp-out (event bit every frame) is never cut by the 5 s rule
    wr_begin(&r, 40000);
    for (t = 40000; t < 40000 + 3 * WR_START_MS; t += 16) CHECK(step(&r, t, false, true, false, false, &why) == WRA_NONE);

    // load over but no 0x1 in WR_ARRIVE_MS: restore anyway (the stage is loaded)
    wr_begin(&r, 50000);
    step(&r, 50016, true, false, false, false, &why);
    step(&r, 60000, false, false, false, false, &why);
    CHECK(r.st == WR_ARRIVE && r.t_load == 60000);
    CHECK(step(&r, 60000 + WR_ARRIVE_MS - 1, false, false, false, false, &why) == WRA_NONE);
    CHECK(step(&r, 60000 + WR_ARRIVE_MS, false, false, false, false, &why) == WRA_RESTORE && strstr(why, "anyway") != NULL);

    // a second load during the arrival goes back to loading (no restore inside it), then the arrival rule again
    wr_begin(&r, 70000);
    step(&r, 70016, true, false, false, false, &why);
    step(&r, 71000, false, false, false, false, &why);
    CHECK(step(&r, 71016, true, false, true, false, &why) == WRA_NONE && r.st == WR_LOAD);
    CHECK(step(&r, 72000, false, false, true, false, &why) == WRA_RESTORE);

    // the title: dropped, nothing written (even while loading)
    wr_begin(&r, 80000);
    step(&r, 80016, true, false, false, false, &why);
    CHECK(step(&r, 80032, true, false, false, true, &why) == WRA_DROP && strstr(why, "title") != NULL);

    // the 180 s cap: put back at the first frame that is not a load
    wr_begin(&r, 90000);
    step(&r, 90016, false, true, false, false, &why);
    CHECK(step(&r, 90000 + WR_MAX_MS, true, true, false, false, &why) == WRA_NONE);  // (loading: still not)
    CHECK(step(&r, 90000 + WR_MAX_MS + 16, false, true, false, false, &why) == WRA_RESTORE && strstr(why, "timeout") != NULL);

    // now 0 is still a run
    wr_begin(&r, 0);
    CHECK(r.st == WR_OUT && r.t0 == 1);
}

static void env_tests(void) {
    char b[96];
    const float op[3] = {-1234.5f, 210.25f, 987.125f};
    int n = wr_env_fmt(b, sizeof b, WR_LOAD, op, -45.5f);
    CHECK(n > 0 && n < (int)sizeof b);
    int ph = 0;
    float p[3], d;
    CHECK(wr_env_parse(b, &ph, p, &d));
    CHECK(ph == WR_LOAD && p[0] == op[0] && p[1] == op[1] && p[2] == op[2] && d == -45.5f);  // (s271 L2: exact)
    const float odd[3] = {1837.0813f, 218.46179f, -996.88123f};  // (floats %.6g would round)
    wr_env_fmt(b, sizeof b, WR_ARRIVE, odd, 179.99991f);
    CHECK(wr_env_parse(b, &ph, p, &d) && p[0] == odd[0] && p[1] == odd[1] && p[2] == odd[2] && d == 179.99991f);
    const float zero[3] = {0, 0, 0};  // (a save with no Medallion: the gamedata init)
    wr_env_fmt(b, sizeof b, WR_OUT, zero, 0);
    CHECK(wr_env_parse(b, &ph, p, &d) && ph == WR_OUT && p[0] == 0 && d == 0);
    CHECK(!wr_env_parse(NULL, &ph, p, &d));
    CHECK(!wr_env_parse("", &ph, p, &d));
    CHECK(!wr_env_parse("0 1 2 3 4", &ph, p, &d));  // (idle is not a run)
    CHECK(!wr_env_parse("4 1 2 3 4", &ph, p, &d));
    CHECK(!wr_env_parse("2 1 2 3", &ph, p, &d));
    CHECK(!wr_env_parse("2 nan 2 3 4", &ph, p, &d));
    CHECK(!wr_env_parse("2 1 inf 3 4", &ph, p, &d));
    CHECK(!wr_env_parse("garbage", &ph, p, &d));
    // resume: OUT waits for the load (the warp-out taken as seen), LOAD / ARRIVE wait for the card to go
    WarpRun r;
    const char *why;
    wr_resume(&r, WR_OUT, 5000);
    CHECK(r.st == WR_OUT && r.ev_seen);
    CHECK(step(&r, 5000 + WR_START_MS + 100, false, false, false, false, &why) == WRA_NONE);  // (not the "did not take it" rule)
    CHECK(step(&r, 5000 + WR_NOLOAD_MS, false, false, false, false, &why) == WRA_RESTORE);
    wr_resume(&r, WR_ARRIVE, 6000);
    CHECK(r.st == WR_LOAD);
    CHECK(step(&r, 6016, true, false, false, false, &why) == WRA_NONE);
    CHECK(step(&r, 6032, false, false, true, false, &why) == WRA_RESTORE);
}

static void note_tests(void) {
    CHECK(wn_alpha(100, 0) == 0);
    CHECK(wn_alpha(50, 100) == 0);  // (a clock behind the start)
    CHECK(wn_alpha(100, 100) == 1);
    CHECK(wn_alpha(100 + WN_SHOW_MS - WN_FADE_MS, 100) == 1);
    float h = wn_alpha(100 + WN_SHOW_MS - WN_FADE_MS / 2, 100);
    CHECK(NEAR(h, 0.5f, 0.01f));
    CHECK(wn_alpha(100 + WN_SHOW_MS, 100) == 0);
}

static void wire_tests(void) {
    uint8_t b[NET_PKT_MAX];
    NetWarpMsg m = {.op = NWO_ASK, .target = 77, .nonce = 0xdeadbeef};
    int n = net_pack_warp(5, 300, &m, b, (int)sizeof b);
    CHECK(n == NET_WARP_HEAD && n == 13);
    int t = 0;
    uint8_t pl = 0;
    uint16_t sq = 0;
    CHECK(net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL) && t == NET_WARP && pl == 5 && sq == 300);
    NetWarpMsg o;
    CHECK(net_unpack_warp(b, n, &pl, &sq, &o) && o.op == NWO_ASK && o.target == 77 && o.nonce == 0xdeadbeef);
    CHECK(!net_unpack_warp(b, n - 1, &pl, &sq, &o));  // (short)
    b[2] = NET_VERSION + 1;
    CHECK(!net_unpack_warp(b, n, &pl, &sq, &o));  // (another protocol version)
    b[2] = NET_VERSION;

    NetWarpMsg s = {.op = NWO_SPOT, .target = 5, .nonce = 42, .flags = NWF_MAINFIELD | NWF_GROUND | NWF_SAVE,
                    .live = {-1500.25f, 250.5f, 1234.75f}, .live_deg = 33.5f, .save = {-1499, 250, 1230}, .save_deg = -120};
    n = net_pack_warp(9, 1, &s, b, (int)sizeof b);
    CHECK(n == NET_WARP_SPOT_LEN && n == 46);
    CHECK(net_unpack_warp(b, n, &pl, &sq, &o) && o.op == NWO_SPOT && o.target == 5 && o.nonce == 42 && o.flags == s.flags);
    CHECK(memcmp(o.live, s.live, 12) == 0 && memcmp(o.save, s.save, 12) == 0 && o.live_deg == 33.5f && o.save_deg == -120);
    CHECK(!net_unpack_ex(b, n + 1, &t, &pl, &sq, NULL, NULL, NULL, NULL));  // (a SPOT is exactly 46)
    CHECK(!net_unpack_ex(b, NET_WARP_HEAD, &t, &pl, &sq, NULL, NULL, NULL, NULL));  // (a SPOT cut to an ASK's size)
    CHECK(net_pack_warp(9, 1, &s, b, NET_WARP_SPOT_LEN - 1) == -1);  // (no room)

    NetWarpMsg w = {.op = NWO_NOTE, .target = 3, .nonce = 7};
    n = net_pack_warp(1, 2, &w, b, (int)sizeof b);
    CHECK(n == NET_WARP_HEAD && net_unpack_warp(b, n, &pl, &sq, &o) && o.op == NWO_NOTE && o.nonce == 7);
    // nonce 0, an unknown op: refused both ways
    w.nonce = 0;
    CHECK(net_pack_warp(1, 2, &w, b, (int)sizeof b) == -1);
    w.nonce = 7;
    n = net_pack_warp(1, 2, &w, b, (int)sizeof b);
    b[NET_HDR + 2] = b[NET_HDR + 3] = b[NET_HDR + 4] = b[NET_HDR + 5] = 0;
    CHECK(net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));  // (the size is fine)
    CHECK(!net_unpack_warp(b, n, &pl, &sq, &o));                       // (the nonce is not)
    n = net_pack_warp(1, 2, &(NetWarpMsg){.op = NWO_NOTE, .target = 3, .nonce = 7}, b, (int)sizeof b);
    b[NET_HDR] = 9;
    CHECK(!net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    CHECK(net_pack_warp(1, 2, &(NetWarpMsg){.op = 4, .nonce = 1}, b, (int)sizeof b) == -1);
    CHECK(net_pack_warp(1, 2, NULL, b, (int)sizeof b) == -1);
    // another type's packet is not a warp packet
    n = net_pack_area(1, 2, "MainField", b, (int)sizeof b);
    CHECK(n > 0 && !net_unpack_warp(b, n, &pl, &sq, &o));
    // the NaN in a SPOT comes through as it is (the receiver's wsp_choose drops it)
    s.live[0] = NAN;
    n = net_pack_warp(9, 1, &s, b, (int)sizeof b);
    CHECK(net_unpack_warp(b, n, &pl, &sq, &o) && isnan(o.live[0]));
    float pos[3], d;
    CHECK(wsp_choose(o.flags, o.live, o.live_deg, o.save, o.save_deg, pos, &d) == WSP_SAVE);
    // NET_VERSION unchanged by this type (an older peer keeps playing: it drops NET_WARP as a bad packet)
    CHECK(NET_VERSION == 12);
}

int main(void) {
    gate_tests();
    spot_tests();
    ask_tests();
    run_tests();
    env_tests();
    note_tests();
    wire_tests();
    printf("warp_test: %d / %d checks pass%s\n", runs - fails, runs, fails ? " - FAIL" : "");
    return fails != 0;
}
