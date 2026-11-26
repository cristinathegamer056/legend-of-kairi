// host test for runtime/sora_duel_core.h + the NET_DUEL packet in runtime/sora_net_proto.h (s272 duel, co-op item 6: duels
// between two players): the gate order, KH2's damage formula with a player victim, the 1 HP floor, the time-limit judge, the ring
// and its leave rule, the countdown, the duel state machine (asker / friend), the peer-gone rule, the hit de-dupe, the body
// tests, the wire format of every op, and NF_DUEL riding a SNAP.
//   clang -O1 -g -Wall -Wextra -Iruntime runtime/test/duel_test.c -o build_h/duel_test.exe
//   (UBSan: + -fsanitize=undefined,bounds -fsanitize-trap=all)
#include <stdio.h>
#include <stdlib.h>
#include "sora_net_proto.h"
#include "sora_duel_core.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)
#define NEAR(a, b, e) (fabsf((a) - (b)) <= (e))
#define T0 1000000ull  // a GetTickCount64-like start (never 0)

static void gate_tests(void) {
    DuGateIn g = {0};
    CHECK(du_gate(NULL, true) == DUG_OFF);
    CHECK(du_gate(&g, true) == DUG_OFF);
    g.on = true, g.enemy_m = g.chase_m = -1, g.now = T0;
    CHECK(du_gate(&g, false) == DUG_NOT_DRIVEN);
    g.driven = true;
    CHECK(du_gate(&g, false) == DUG_OK);    // (not strict: in the air / a menu are fine when an ASK comes in)
    CHECK(du_gate(&g, true) == DUG_AIR);    // (strict: standing on the ground)
    g.ground = true;
    CHECK(du_gate(&g, true) == DUG_OK);
    // the order: busy, cool, menu, form, summon, board, air, enemy, chase, hit; the first block wins
    DuGateIn b = g;
    b.busy = b.menu = b.form = b.summon = b.board = true, b.ground = false, b.cool_until = T0 + 1, b.enemy_m = 5, b.chase_m = 40, b.last_hit_ms = T0 - 1;
    CHECK(du_gate(&b, true) == DUG_BUSY);
    b.busy = false;
    CHECK(du_gate(&b, true) == DUG_COOL);
    b.cool_until = T0;  // (now == the end: over)
    CHECK(du_gate(&b, true) == DUG_MENU);
    CHECK(du_gate(&b, false) == DUG_FORM);  // (not strict: the menu does not refuse)
    b.menu = false;
    CHECK(du_gate(&b, true) == DUG_FORM);
    b.form = false;
    CHECK(du_gate(&b, true) == DUG_SUMMON);
    b.summon = false;
    CHECK(du_gate(&b, true) == DUG_BOARD);
    CHECK(du_gate(&b, false) == DUG_ENEMY);  // (not strict: board / air are fine)
    b.board = false;
    CHECK(du_gate(&b, true) == DUG_AIR);
    b.ground = true;
    CHECK(du_gate(&b, true) == DUG_ENEMY);
    b.enemy_m = 30.0f;  // (the edge is in)
    CHECK(du_gate(&b, true) == DUG_ENEMY);
    b.enemy_m = 30.01f;
    CHECK(du_gate(&b, true) == DUG_CHASE);
    b.chase_m = 60.5f;
    CHECK(du_gate(&b, true) == DUG_HIT);
    b.last_hit_ms = T0 - DU_HIT_HOLD_MS;  // (10 s ago exactly: over)
    CHECK(du_gate(&b, true) == DUG_OK);
    b.last_hit_ms = T0 - DU_HIT_HOLD_MS + 1;
    CHECK(du_gate(&b, true) == DUG_HIT);
    b.last_hit_ms = 0;  // (never hit)
    CHECK(du_gate(&b, true) == DUG_OK);
    b.on = false;
    CHECK(du_gate(&b, true) == DUG_OFF);
    for (int i = 0; i < DUG_N; i++) CHECK(DUG_WHY[i] && DUG_WHY[i][0]);
    // enemies vs animals; BotW's target states
    CHECK(du_enemy_name("Enemy_Bokoblin_Junior") && du_enemy_name("Enemy_Lynel_Dark"));
    CHECK(!du_enemy_name("Animal_Fox_A") && !du_enemy_name("Animal_Horse") && !du_enemy_name(NULL) && !du_enemy_name("Enemy") && !du_enemy_name(""));
    CHECK(du_chasing_state(2) && du_chasing_state(4));
    CHECK(!du_chasing_state(0) && !du_chasing_state(1) && !du_chasing_state(3) && !du_chasing_state(5) && !du_chasing_state(-1));
}

static void damage_tests(void) {
    int v = -1;
    // KH2 rows (sora_levels.h SL_LV, VERIFIED lvup): LV20 Str 12 / Def 13; the Kingdom Key's Str +3 (GUESS row, any key: S = Str + key)
    CHECK(du_dmg(12 + 3, 13, 100, 0, 1, &v) == 2 && v == 2);   // V 2 x 100 % = 2 quarter hearts
    CHECK(du_dmg(12 + 3, 13, 100, 0, 0, &v) == 1);             // Beginner: x0.5, ceil -> 1
    CHECK(du_dmg(12 + 3, 13, 100, 0, 2, &v) == 4);             // Proud: x2
    // (s286) dueldmg: x pct, rounded to the nearest, at least 1; 100 = unchanged; 0 / negative damage untouched
    CHECK(du_dmg_scale(3, 100) == 3 && du_dmg_scale(3, 50) == 2 && du_dmg_scale(2, 50) == 1 && du_dmg_scale(1, 50) == 1);
    CHECK(du_dmg_scale(6, 50) == 3 && du_dmg_scale(9, 50) == 5 && du_dmg_scale(1, 1) == 1 && du_dmg_scale(4, 0) == 1);
    CHECK(du_dmg_scale(0, 50) == 0 && du_dmg_scale(-1, 50) == -1 && du_dmg_scale(5000, 5000) == 50000);
    CHECK(du_dmg(12 + 3, 13, 100, 0, 3, &v) == 4);             // Critical: x2 (taken)
    CHECK(du_dmg(10, 13, 100, 0, 1, &v) == 1 && v == 1);       // S < Def: V = Min 1 (GUESS for a player victim)
    CHECK(du_dmg(13, 13, 100, 0, 1, &v) == 1 && v == 1);       // S == Def: V 0 -> 1
    CHECK(du_dmg(18, 13, 250, 0, 1, &v) == 13 && v == 5);      // a spell Power 250: ceil(12.5) = 13
    CHECK(du_dmg(16, 13, 150, 0, 1, &v) == 5);                 // Fire's last wave 150: ceil(4.5) = 5
    CHECK(du_dmg(16, 13, 50, 0, 1, &v) == 2);                  // Fire's tick 50: ceil(1.5) = 2
    CHECK(du_dmg(15, 13, 100, 20, 1, &v) == 3);                // + 20 % boost: 2 + ceil(0.4) = 3 (ab_boost)
    CHECK(du_dmg(15, 13, 0, 0, 1, &v) == 0);                   // Power 0 (a guard window) = no damage
    CHECK(du_dmg(15, 13, 100, 0, -5, &v) == 1 && du_dmg(15, 13, 100, 0, 9, &v) == 4);  // (difficulty clamped 0..3)
    CHECK(du_dmg(52 + 3, 13, 100, 0, 1, &v) == 42);            // LV99 Str vs LV20 Def: no Cap (GUESS)
    CHECK(du_dmg(15, 13, 1, 0, 1, NULL) == 1);                 // (never 0 for a real hit)
    // the floor
    bool ko = true;
    CHECK(du_life_after(40, 3, &ko) == 37 && !ko);
    CHECK(du_life_after(3, 1, &ko) == 2 && !ko);
    CHECK(du_life_after(3, 2, &ko) == 1 && ko);   // (to 1 exactly = the KO)
    CHECK(du_life_after(3, 30, &ko) == 1 && ko);  // (overkill: still 1, never 0)
    CHECK(du_life_after(1, 1, &ko) == 1 && ko);   // (at 1 already: any hit is the KO)
    CHECK(du_life_after(0, 5, &ko) == 0 && !ko);  // (dead / unknown: untouched)
    CHECK(du_life_after(20, 0, &ko) == 20 && !ko);
    // HP permille + the judge
    CHECK(du_pm(48, 48) == 1000 && du_pm(24, 48) == 500 && du_pm(1, 48) == 20 && du_pm(0, 48) == 0 && du_pm(5, 0) == 0 && du_pm(60, 48) == 1000);
    CHECK(du_time_result(600, 500) == DUR_WIN && du_time_result(400, 500) == DUR_LOSE && du_time_result(500, 500) == DUR_DRAW);
    CHECK(du_result_from_peer(DUE_KO, DUR_LOSE) == DUR_WIN);
    CHECK(du_result_from_peer(DUE_FORFEIT, DUR_LOSE) == DUR_WIN);
    CHECK(du_result_from_peer(DUE_GONE, DUR_WIN) == DUR_LOSE);
    CHECK(du_result_from_peer(DUE_KO, DUR_DRAW) == DUR_DRAW);
    CHECK(du_result_from_peer(DUE_ENEMY, DUR_CANCEL) == DUR_CANCEL && du_result_from_peer(DUE_CANCEL, DUR_NONE) == DUR_CANCEL);
    for (int i = 0; i < DUR_N; i++) CHECK(DUR_NAME[i] && DUR_NAME[i][0]);
    for (int i = 0; i < DUE_N; i++) CHECK(DUE_NAME[i] && DUE_NAME[i][0]);
    for (int i = 0; i < DU_NPH; i++) CHECK(DU_PH_NAME[i] && DU_PH_NAME[i][0]);
}

static void ring_tests(void) {
    const float a[3] = {100, 20, 300}, b[3] = {106, 22, 300};
    float c[3];
    du_mid(a, b, c);
    CHECK(NEAR(c[0], 103, 1e-4f) && NEAR(c[1], 21, 1e-4f) && NEAR(c[2], 300, 1e-4f));
    CHECK(NEAR(du_hdist(a, b), 6, 1e-4f));
    float p[3] = {103 + 9.99f, 21, 300};
    CHECK(du_inside(c, 10, p));
    p[0] = 103 + 10.01f;
    CHECK(!du_inside(c, 10, p));
    p[0] = 103, p[2] = 300 - 10;  // (on the line: in)
    CHECK(du_inside(c, 10, p));
    p[2] = 300, p[1] = 21 + DU_RING_DY + 0.1f;  // (above the ring: out)
    CHECK(!du_inside(c, 10, p));
    p[1] = 21 - DU_RING_DY;  // (the edge below: in)
    CHECK(du_inside(c, 10, p));
    p[1] = NAN;
    CHECK(!du_inside(c, 10, p));
    // 0.5 s outside in a row
    DuOut o = {0};
    CHECK(!du_out_step(&o, true, T0));
    CHECK(!du_out_step(&o, true, T0 + DU_OUT_MS - 1));
    CHECK(du_out_step(&o, true, T0 + DU_OUT_MS));
    CHECK(!du_out_step(&o, false, T0 + 600));  // (back in: starts over)
    CHECK(!du_out_step(&o, true, T0 + 700));
    CHECK(!du_out_step(&o, true, T0 + 1100));
    CHECK(du_out_step(&o, true, T0 + 1200));
    // the countdown + the clock
    CHECK(du_count_digit(0, T0) == -1 && du_count_digit(T0, T0 - 1) == -1);
    CHECK(du_count_digit(T0, T0) == 3 && du_count_digit(T0, T0 + 999) == 3 && du_count_digit(T0, T0 + 1000) == 2);
    CHECK(du_count_digit(T0, T0 + 2000) == 1 && du_count_digit(T0, T0 + 2999) == 1);
    CHECK(du_count_digit(T0, T0 + 3000) == 0 && du_count_digit(T0, T0 + 3000 + DU_GO_MS - 1) == 0);
    CHECK(du_count_digit(T0, T0 + 3000 + DU_GO_MS) == -1);
    char t[16];
    du_clock_txt(180000, t, sizeof t);
    CHECK(!strcmp(t, "3:00"));
    du_clock_txt(179000, t, sizeof t);
    CHECK(!strcmp(t, "2:59"));
    du_clock_txt(179001, t, sizeof t);
    CHECK(!strcmp(t, "3:00"));  // (rounded up: 0:00 only at the end)
    du_clock_txt(1, t, sizeof t);
    CHECK(!strcmp(t, "0:01"));
    du_clock_txt(0, t, sizeof t);
    CHECK(!strcmp(t, "0:00"));
    du_clock_txt(65000, t, sizeof t);
    CHECK(!strcmp(t, "1:05"));
    // the body tests
    const float feet[3] = {0, 10, 0};
    float q[3] = {1.5f, 10.5f, 0};
    CHECK(du_body_near(feet, q, 2.0f, -1, 2));
    q[0] = 2.1f;
    CHECK(!du_body_near(feet, q, 2.0f, -1, 2));
    q[0] = 0, q[1] = 12.5f;  // (dy 2.5 > hi 2)
    CHECK(!du_body_near(feet, q, 2.0f, -1, 2));
    q[1] = 8.9f;  // (dy -1.1 < lo -1)
    CHECK(!du_body_near(feet, q, 2.0f, -1, 2));
    const float body[3] = {0, 0, 0}, ahead[3] = {0, 0, 5}, behind[3] = {0, 0, -5}, side[3] = {5, 0, 0};
    CHECK(du_from_front(body, 0, ahead) && !du_from_front(body, 0, behind));
    CHECK(du_from_front(body, 3.14159265f, behind) && !du_from_front(body, 3.14159265f, ahead));
    CHECK(du_from_front(body, 1.5707963f, side));  // (yaw 90 deg faces +x)
    const float fw0[2] = {0, 1}, f45[3] = {3, 0, 3}, f70[3] = {3, 0, 1.0f}, fb[3] = {0, 0, -3}, fon[3] = {0.2f, 0, -0.2f};
    CHECK(du_facing(body, fw0, ahead) && du_facing(body, fw0, f45) && !du_facing(body, fw0, f70) && !du_facing(body, fw0, fb));
    CHECK(du_facing(body, fw0, fon));  // (on top of him)
    CHECK(DU_BODY_Y0_CM + DU_BODY_H_CM == 125.0f && NEAR((DU_BODY_Y0_CM + DU_BODY_H_CM * 0.5f), 70.0f, 1e-4f) && DU_BODY_R_CM == 55.0f);  // (KH2's sphere: centre 70, r 55)
}

static void sm_tests(void) {
    DuSm m = {0};
    m.limit_ms = 60000;
    // asker: ask, no answer
    CHECK(!du_sm_ask(&m, 0, T0));  // (id 0 is not valid)
    CHECK(du_sm_ask(&m, 0x1234, T0) && m.phase == DU_ASKING && m.role == DU_ROLE_ASKER && m.limit_ms == 60000);
    CHECK(!du_sm_ask(&m, 0x5678, T0) && !du_sm_asked(&m, 0x5678, T0));  // (busy)
    CHECK(du_sm_busy(&m) && !du_sm_live(&m));
    CHECK(du_sm_step(&m, T0 + DU_ACCEPT_MS + DU_ASK_SLACK_MS - 1) == DUS_NONE);
    CHECK(du_sm_step(&m, T0 + DU_ACCEPT_MS + DU_ASK_SLACK_MS) == DUS_ASK_TIMEOUT);
    du_sm_reset(&m);
    CHECK(m.phase == DU_IDLE && m.limit_ms == 60000 && !du_sm_busy(&m));  // (the live limit outlives a reset)
    // friend: asked, the 10 s run out
    CHECK(du_sm_asked(&m, 0x99, T0) && m.phase == DU_ASKED && m.role == DU_ROLE_FRIEND);
    CHECK(du_sm_step(&m, T0 + DU_ACCEPT_MS - 1) == DUS_NONE);
    CHECK(du_sm_step(&m, T0 + DU_ACCEPT_MS) == DUS_PROMPT_TIMEOUT);
    // friend accepts at +4 s: countdown, fight, time up, the judge, over, idle
    du_sm_reset(&m);
    CHECK(!du_sm_count(&m, T0));  // (nothing to accept)
    du_sm_asked(&m, 0x99, T0);
    uint64_t a = T0 + 4000;
    CHECK(du_sm_count(&m, a) && m.phase == DU_COUNT && m.count0 == a && du_sm_live(&m));
    CHECK(!du_sm_count(&m, a));  // (once)
    CHECK(du_sm_left(&m, a) == 0);  // (no clock during the count)
    CHECK(du_sm_step(&m, a + DU_COUNT_MS - 1) == DUS_NONE && m.phase == DU_COUNT);
    CHECK(du_sm_step(&m, a + DU_COUNT_MS) == DUS_FIGHT && m.phase == DU_FIGHT);
    CHECK(du_sm_left(&m, a + DU_COUNT_MS) == 60000 && du_sm_left(&m, a + DU_COUNT_MS + 59000) == 1000);
    du_sm_time_his(&m, 400);  // (his clock ran ahead: his TIME came during our fight)
    CHECK(!du_sm_time_ready(&m));
    CHECK(du_sm_step(&m, a + DU_COUNT_MS + 59999) == DUS_NONE);
    CHECK(du_sm_step(&m, a + DU_COUNT_MS + 60000) == DUS_TIMEUP && m.phase == DU_TIMEUP && du_sm_live(&m));
    CHECK(!du_sm_time_ready(&m));
    du_sm_time_mine(&m, 700);
    CHECK(du_sm_time_ready(&m) && du_time_result(m.my_pm, m.his_pm) == DUR_WIN);
    uint64_t e = a + DU_COUNT_MS + 60100;
    du_sm_end(&m, DUR_WIN, DUE_TIME, e);
    CHECK(m.phase == DU_OVER && m.result == DUR_WIN && m.reason == DUE_TIME && !du_sm_live(&m) && du_sm_busy(&m));
    CHECK(du_sm_step(&m, e + DU_OVER_MS - 1) == DUS_NONE);
    CHECK(du_sm_step(&m, e + DU_OVER_MS) == DUS_IDLE && m.phase == DU_IDLE && m.id == 0 && m.limit_ms == 60000);
    // the time wait: no TIME from him in 3 s
    du_sm_ask(&m, 7, T0), du_sm_count(&m, T0 + 100), du_sm_step(&m, T0 + 100 + DU_COUNT_MS);
    CHECK(du_sm_step(&m, T0 + 100 + DU_COUNT_MS + 60000) == DUS_TIMEUP);
    uint64_t tu = m.t0;
    CHECK(du_sm_step(&m, tu + DU_TIME_WAIT_MS - 1) == DUS_NONE);
    CHECK(du_sm_step(&m, tu + DU_TIME_WAIT_MS) == DUS_TIME_DECIDE);
    // the default limit = 3 min
    du_sm_reset(&m);
    m.limit_ms = 0;
    du_sm_ask(&m, 8, T0), du_sm_count(&m, T0), du_sm_step(&m, T0 + DU_COUNT_MS);
    CHECK(du_limit(&m) == 180000 && du_sm_left(&m, T0 + DU_COUNT_MS) == 180000);
    CHECK(du_sm_step(&m, T0 + DU_COUNT_MS + 179999) == DUS_NONE && du_sm_step(&m, T0 + DU_COUNT_MS + 180000) == DUS_TIMEUP);
    // an end from the asking phase
    du_sm_reset(&m), du_sm_ask(&m, 9, T0), du_sm_end(&m, DUR_CANCEL, DUE_CANCEL, T0 + 5);
    CHECK(m.phase == DU_OVER && !du_sm_live(&m));
    // the other game gone: no NF_DUEL after the grace / for 3 s
    CHECK(!du_peer_gone(0, 0, T0));
    CHECK(!du_peer_gone(T0, 0, T0 + DU_PEER_GRACE_MS - 1));
    CHECK(du_peer_gone(T0, 0, T0 + DU_PEER_GRACE_MS));       // (never seen since the accept)
    CHECK(du_peer_gone(T0, T0 - 50, T0 + DU_PEER_GRACE_MS)); // (a flag from before the accept does not count)
    CHECK(!du_peer_gone(T0, T0 + 2000, T0 + 4999));
    CHECK(du_peer_gone(T0, T0 + 2000, T0 + 2000 + DU_PEER_MS));
    CHECK(!du_peer_gone(T0, T0 + 9000, T0 + 9500));
}

static void seen_tests(void) {
    DuSeen d = {0};
    CHECK(du_seen_add(&d, 1) && !du_seen_add(&d, 1) && du_seen_add(&d, 2) && !du_seen_add(&d, 2) && !du_seen_add(&d, 1));
    for (uint16_t i = 3; i < 40; i++) CHECK(du_seen_add(&d, i));
    CHECK(d.n == 32);
    CHECK(!du_seen_add(&d, 39) && !du_seen_add(&d, 8));  // (the last 32 are kept)
    CHECK(du_seen_add(&d, 1));                           // (an old one that fell out counts again: a new duel resets the list anyway)
    CHECK(du_seen_add(&d, 0) && du_seen_add(&d, 65535));
}

static int pack(const NetDuelMsg *m, uint8_t *b) { return net_pack_duel(0x42, 0x0102, m, b, NET_PKT_MAX); }
static void wire_tests(void) {
    uint8_t b[NET_PKT_MAX];
    NetDuelMsg m = {0}, r;
    uint8_t pl = 0;
    uint16_t sq = 0;
    int t = 0;
    CHECK(NET_DUEL == 11 && NET_WARP == 10 && NET_VERSION == 12);  // (a new type, no version bump)
    CHECK(NF_DUEL == 2048 && !(NF_DUEL & (NF_WEAPON | NF_AIR | NF_GHOST | NF_GUARD | NF_DEAD | NF_LOADING | NF_BONEPOSE | NF_BONES | NF_SKATE | 512 | 1024)));
    CHECK(NET_DUEL_HEAD == 13 && NET_DUEL_ANSWER_LEN == 29 && NET_DUEL_HIT_LEN == 51 && NET_DUEL_END_LEN == 17 && NET_DUEL_TIME_LEN == 15 && NET_DUEL_RING_LEN == 31);
    CHECK(NET_DUEL_HIT_LEN <= NET_PKT_MAX);
    // bad inputs
    m.op = NDO_ASK;
    CHECK(pack(&m, b) < 0);  // (id 0)
    m.id = 0xDEADBEEF, m.op = 0;
    CHECK(pack(&m, b) < 0);
    m.op = 7;
    CHECK(pack(&m, b) < 0);
    m.op = NDO_HIT;
    CHECK(net_pack_duel(1, 1, &m, b, NET_DUEL_HIT_LEN - 1) < 0 && net_pack_duel(1, 1, NULL, b, 64) < 0);
    // ASK
    memset(&m, 0, sizeof m), m.op = NDO_ASK, m.target = 0x77, m.id = 0xDEADBEEF;
    int n = pack(&m, b);
    CHECK(n == 13 && b[3] == NET_DUEL);
    CHECK(net_unpack_ex(b, n, &t, &pl, &sq, NULL, NULL, NULL, NULL) && t == NET_DUEL && pl == 0x42 && sq == 0x0102);
    CHECK(net_unpack_duel(b, n, &pl, &sq, &r) && r.op == NDO_ASK && r.target == 0x77 && r.id == 0xDEADBEEF);
    CHECK(!net_unpack_duel(b, n - 1, &pl, &sq, &r) && !net_unpack_duel(b, n + 1, &pl, &sq, &r));  // (exact size)
    // ANSWER
    memset(&m, 0, sizeof m), m.op = NDO_ANSWER, m.target = 3, m.id = 99, m.ans = DUA_ACCEPT, m.why = 0, m.c[0] = -1234.5f, m.c[1] = 210.25f, m.c[2] = 2048.75f, m.radius_cm = 1000;
    n = pack(&m, b);
    CHECK(n == 29 && net_unpack_duel(b, n, &pl, &sq, &r) && r.ans == DUA_ACCEPT && r.c[0] == -1234.5f && r.c[1] == 210.25f && r.c[2] == 2048.75f && r.radius_cm == 1000);
    m.ans = DUA_BUSY, m.why = DUG_ENEMY;
    n = pack(&m, b);
    CHECK(net_unpack_duel(b, n, &pl, &sq, &r) && r.ans == DUA_BUSY && r.why == DUG_ENEMY);
    m.ans = 0, n = pack(&m, b);
    CHECK(!net_unpack_duel(b, n, &pl, &sq, &r));  // (answer 0)
    m.ans = 4, n = pack(&m, b);
    CHECK(!net_unpack_duel(b, n, &pl, &sq, &r));
    m.ans = DUA_ACCEPT, m.c[1] = NAN, n = pack(&m, b);
    CHECK(!net_unpack_duel(b, n, &pl, &sq, &r));  // (a NaN centre)
    m.c[1] = INFINITY, n = pack(&m, b);
    CHECK(!net_unpack_duel(b, n, &pl, &sq, &r));
    // HIT
    memset(&m, 0, sizeof m), m.op = NDO_HIT, m.target = 9, m.id = 5, m.hseq = 65535, m.power = 250, m.stat = 31, m.flags = NDH_SPELL | NDH_FINISH, m.src = 3;
    m.boost = 20, m.react = 7, m.atkp = 502;
    for (int j = 0; j < 3; j++) m.from[j] = 1.5f * (float)(j + 1), m.at[j] = -2.25f * (float)(j + 1);
    n = pack(&m, b);
    CHECK(n == 51 && net_unpack_duel(b, n, &pl, &sq, &r));
    CHECK(r.hseq == 65535 && r.power == 250 && r.stat == 31 && r.flags == (NDH_SPELL | NDH_FINISH) && r.src == 3 && r.boost == 20 && r.react == 7 && r.atkp == 502);
    CHECK(r.from[0] == 1.5f && r.from[2] == 4.5f && r.at[1] == -4.5f && r.at[2] == -6.75f);
    m.at[2] = NAN, n = pack(&m, b);
    CHECK(!net_unpack_duel(b, n, &pl, &sq, &r));
    // END / TIME
    memset(&m, 0, sizeof m), m.op = NDO_END, m.id = 6, m.reason = DUE_KO, m.result = DUR_LOSE, m.pm = 20;
    n = pack(&m, b);
    CHECK(n == 17 && net_unpack_duel(b, n, &pl, &sq, &r) && r.reason == DUE_KO && r.result == DUR_LOSE && r.pm == 20);
    memset(&m, 0, sizeof m), m.op = NDO_TIME, m.id = 6, m.pm = 1000;
    n = pack(&m, b);
    CHECK(n == 15 && net_unpack_duel(b, n, &pl, &sq, &r) && r.op == NDO_TIME && r.pm == 1000);
    // RING
    memset(&m, 0, sizeof m), m.op = NDO_RING, m.id = 77, m.a = 200, m.b = 13, m.phase = 2, m.c[0] = 10, m.c[1] = 20, m.c[2] = 30, m.radius_cm = 1500;
    n = pack(&m, b);
    CHECK(n == 31 && net_unpack_duel(b, n, &pl, &sq, &r) && r.a == 200 && r.b == 13 && r.phase == 2 && r.c[2] == 30 && r.radius_cm == 1500 && r.target == 0);
    // id 0 on the wire, an unknown op, a wrong length for the op: refused by net_unpack_ex itself
    uint8_t c[NET_PKT_MAX];
    memcpy(c, b, (size_t)n);
    c[NET_HDR + 2] = c[NET_HDR + 3] = c[NET_HDR + 4] = c[NET_HDR + 5] = 0;
    CHECK(net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL) && !net_unpack_duel(c, n, &pl, &sq, &r));
    memcpy(c, b, (size_t)n), c[NET_HDR] = 7;
    CHECK(!net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    memcpy(c, b, (size_t)n), c[NET_HDR] = 0;
    CHECK(!net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    memcpy(c, b, (size_t)n), c[NET_HDR] = NDO_ASK;  // (a RING-sized ASK)
    CHECK(!net_unpack_ex(c, n, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    CHECK(!net_unpack_ex(b, NET_DUEL_HEAD - 1, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    // every op's exact size only
    for (int op = NDO_ASK; op <= NDO_RING; op++) {
        memset(&m, 0, sizeof m), m.op = (uint8_t)op, m.id = 3, m.ans = 1;
        n = pack(&m, b);
        CHECK(n == NET_DUEL_LEN[op] && net_unpack_duel(b, n, &pl, &sq, &r) && r.op == op);
        for (int l = NET_HDR; l <= NET_DUEL_HIT_LEN + 2; l++) if (l != n) CHECK(!net_unpack_ex(b, l, &t, &pl, &sq, NULL, NULL, NULL, NULL));
    }
    // NF_DUEL rides a SNAP (the flags are copied raw)
    NetSnap s, sr;
    memset(&s, 0, sizeof s), s.player = 5, s.seq = 9, s.flags = NF_DUEL | NF_AIR;
    n = net_pack_snap(&s, b, sizeof b);
    char name[NET_NAME_MAX + 1];
    memset(&sr, 0xcc, sizeof sr);
    CHECK(n > 0 && net_unpack(b, n, &t, &pl, &sq, &sr, name) && t == NET_SNAP && sr.flags == (NF_DUEL | NF_AIR));
}

// (s288 port) du_block_why: why a friend's duel hit is blocked (du_take_hit). User ruling 2026-11-25: the Dodge Roll's
// (rollinv, via g_rc_inv) and the Aerial Dodge block's (adblock) frames block duel hits too; a paused player takes the hit.
static void block_tests(void) {
    //                      paused guard  reflect inv    adodge limit
    CHECK(du_block_why(false, false, false, false, false, false) == DUB_NONE);
    CHECK(du_block_why(false, true, false, false, false, false) == DUB_GUARD);
    CHECK(du_block_why(false, false, true, false, false, false) == DUB_REFLECT);
    CHECK(du_block_why(false, false, false, true, false, false) == DUB_INV);      // (Dodge Roll / Quick Run / post-block frames)
    CHECK(du_block_why(false, false, false, false, true, false) == DUB_ADODGE);   // (the Aerial Dodge block)
    CHECK(du_block_why(false, false, false, false, false, true) == DUB_LIMIT);
    CHECK(du_block_why(true, true, true, true, true, true) == DUB_NONE);          // (paused: every reason off, s284)
    CHECK(du_block_why(true, false, false, false, true, false) == DUB_NONE);
    // the old log order: Guard, then Reflect, then the invulnerable frames, then Limit; the Aerial Dodge after the frames
    CHECK(du_block_why(false, true, true, true, true, true) == DUB_GUARD);
    CHECK(du_block_why(false, false, true, true, true, true) == DUB_REFLECT);
    CHECK(du_block_why(false, false, false, true, true, true) == DUB_INV);
    CHECK(du_block_why(false, false, false, false, true, true) == DUB_ADODGE);
    // the log text: both dodges read "invulnerable frames" (the main session greps "blocked: invulnerable frames")
    CHECK(!strcmp(du_block_name(DUB_INV), "invulnerable frames"));
    CHECK(!strncmp(du_block_name(DUB_ADODGE), "invulnerable frames", 19));
    CHECK(!strcmp(du_block_name(DUB_GUARD), "Guard") && !strcmp(du_block_name(DUB_REFLECT), "Reflect") &&
          !strcmp(du_block_name(DUB_LIMIT), "Limit"));
}

int main(void) {
    block_tests();
    gate_tests();
    damage_tests();
    ring_tests();
    sm_tests();
    seen_tests();
    wire_tests();
    printf("duel_test: %d / %d checks pass\n", runs - fails, runs);
    return fails ? 1 : 0;
}
