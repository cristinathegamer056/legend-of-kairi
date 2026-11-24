// sora_qrun.h: (qrun) KH2's Quick Run, the growth ability of Wisdom Form (user: "in KH2 ... if you held the square button,
// it would do the quick run ability, but if you pressed the square button quickly, you would perform a dodge roll").
// KH2 (Steam FM exe, Ghidra kh2steam; notes D:/KairiWork/ghidra/qr1-4.c, RESUME 2026-11-24):
//   * Square handler re:0x3A81F0: Dodge Roll level (growth 4) > 0 -> the roll (motion 209, A180); else Quick Run level (growth 1)
//     > 0 -> Quick Run at once (re:0x3A96C0).
//   * The hold: A180's frame trigger type 0x22 at FRAME 10 (trigger handler re:0x40DD70 case 0x22): Square still held -> Quick Run
//     along the roll's direction (re:0x3A9830). So tap = roll, hold = the roll turns into Quick Run.
//   * Start (re:0x3A9830): motion 203 (A150, slot 814), ACTION 0x10, velocity = direction * AirSlideSpeed, timer AirSlideTime,
//     invulnerable AirSlideInvulnerableFrame (re:0x3D7E40), LV > 1 also an effect (id LV + 0x6c: NOT ported, unknown which).
//   * Tick (re:0x423500, once per physics step): the timer counts down by 1 while Square is held (+0x124 bit 2, GUESS: cleared at
//     the release = the timer drops to 0 at once); at 0 -> brake = AirSlideStopBrake + motion 204 (A151, slot 818), blend 8.
//   * Physics (re:0x423480): velocity *= brake per physics step, move = velocity * step.
//   * fmab (03system.bin @0x17A70, stride 0x44, row = level - 1; OpenKH's AirSlide* names) below; base Sora's level = Wisdom's
//     growth (fmlv byte 1,1,2,2,3,3,4) - 1 (as High Jump from Valor, Aerial Dodge from Master); Wisdom Form uses its own growth.
// No stamina (KH2 has none; the roll before a hold already paid its own). Only base Sora and Wisdom Form (both bakes have slots
// 814 / 818; other forms would fall back to base Sora's body).
// Option: SAVAGEWIND_SORA_QUICK_RUN=0 / live "set qrun 0" = off. On by default (user tested: "It works as it should"). Debug Menu row "Quick Run" = g_dbm_qrun
// (sora.c, Off / LV1-3, base Sora at least that). Log lines "[qrun]".
#pragma once
enum { MOT_QRUN = 203, MOT_QRUN_END = 204 };
#define QR_HOLD_FRAME 10.0f  // A180 frame trigger 0x22
static int g_qrun = 1;
static const uint8_t QR_GROWTH[7] = {1, 1, 2, 2, 3, 3, 4};  // Wisdom fmlv growth byte per form level
static const struct { float time, speed, brake, stop, inv; } QR_ROW[4] = {
    {14, 30, 0.94f, 0.93f, 0}, {18, 34, 0.95f, 0.93f, 8}, {40, 38, 0.96f, 0.93f, 12}, {40, 38, 0.98f, 0.93f, 16}};
static struct {
    int ph;          // 0 off, 1 the dash (A150), 2 the brake-out (A151)
    int lv;
    float t, vel[2], brake, dist;
    int swing;       // S.swing of the motion it owns (anything else played = it is over)
} QR;

static int qr_lv(void) {  // 0 = none
    if (!g_qrun || IN_GUEST) return 0;
    wl_exp_get();
    int g = QR_GROWTH[wl_lv() - 1];
    if (IN_WISDOM) return g;
    if (fm_in_form()) return 0;
    int lv = g - 1;
    if (g_dbm_qrun > lv) lv = g_dbm_qrun > 3 ? 3 : g_dbm_qrun;  // (the Debug Menu's Quick Run row)
    return lv < 0 ? 0 : lv;
}
static void qr_start(float dx, float dz, int lv, const char *why, const float p[3]) {
    const Mot *m = mot_of(MOT_QRUN);
    if (!m) { LOG("[qrun] motion %d not in the bake: no Quick Run\n", MOT_QRUN); return; }
    float n = hypotf(dx, dz);
    if (n < 1e-4f) dx = sinf(S.face), dz = cosf(S.face), n = 1;
    dx /= n, dz /= n;
    S.face = atan2f(dx, dz);
    play(MOT_QRUN), S.special = SP_ROLL;
    QR.ph = 1, QR.lv = lv, QR.swing = S.swing, QR.dist = 0;
    QR.t = QR_ROW[lv - 1].time, QR.brake = QR_ROW[lv - 1].brake;
    QR.vel[0] = dx * QR_ROW[lv - 1].speed, QR.vel[1] = dz * QR_ROW[lv - 1].speed;
    if (QR_ROW[lv - 1].inv > g_rc_inv) g_rc_inv = QR_ROW[lv - 1].inv;  // (the exe drops hits while it runs, sora_guard.h)
    LOG("[qrun] QUICK RUN LV%d (%s): %.0f cm/frame, up to %.0f steps, %.0f invulnerable frames, face %.1f at %.3f %.3f %.3f\n", lv, why,
        QR_ROW[lv - 1].speed, QR.t, QR_ROW[lv - 1].inv, S.face * 180 / PI, p[0], p[1], p[2]);
}
// after the roll press (sora.c): the roll -> Quick Run hold, Quick Run at once without the roll, the dash's timer
static void qr_tick(uint64_t press, uint64_t held, const float sdir[2], float sspeed, bool is_free, bool guard_took, float clock_old,
                    float ratio, const float p[3]) {
    if (QR.ph && (S.special != SP_ROLL || S.swing != QR.swing || (S.motion != MOT_QRUN && S.motion != MOT_QRUN_END))) {
        LOG("[qrun] cut off (motion %d special %d) after %.0f cm\n", S.motion, S.special, QR.dist);
        QR.ph = 0;
    }
    if (QR.ph == 1) {
        QR.t = (held & BTN_ROLL) ? QR.t - ratio : 0;  // (released: over at once, GUESS)
        if (QR.t <= 0) {
            QR.ph = 2, QR.brake = QR_ROW[QR.lv - 1].stop;
            play(MOT_QRUN_END), S.special = SP_ROLL, QR.swing = S.swing;
            LOG("[qrun] %s after %.0f cm: brake out (motion %d)\n", (held & BTN_ROLL) ? "time up" : "Square released", QR.dist, MOT_QRUN_END);
        }
        return;
    }
    if (QR.ph) return;
    int lv = qr_lv();
    if (lv < 1 || S.act != ACT_IDLE || S.takeoff > 0 || S.air_frames) return;
    if (S.special == SP_ROLL && S.motion == MOT_ROLL && clock_old < QR_HOLD_FRAME && S.clock >= QR_HOLD_FRAME && (held & BTN_ROLL)) {
        qr_start(sinf(S.face), cosf(S.face), lv, "Square held through the roll", p);
        return;
    }
    // no Dodge Roll (not learned, or Wisdom Form: no roll there): Square with the stick = Quick Run at once (KH2's order)
    bool roll_here = !fm_in_form() && roll_learned() && mot_of(MOT_ROLL);
    if (!roll_here && !guard_took && (press & BTN_ROLL) && sspeed > 0 && is_free && S.special != SP_ROLL)
        qr_start(sdir[0], sdir[1], lv, IN_WISDOM ? "press, Wisdom Form" : "press, no Dodge Roll learned", p);
}
// the ACT_IDLE move (sora.c, after the root move): the dash's own velocity, braked once per physics step
static void qr_move(float move[2], float kh, float ratio) {
    if (!QR.ph) return;
    float k = powf(QR.brake, ratio);
    QR.vel[0] *= k, QR.vel[1] *= k;
    move[0] = QR.vel[0] * kh, move[1] = QR.vel[1] * kh;
    S.vel[0] = QR.vel[0], S.vel[1] = QR.vel[1];  // (a walk-off mid-dash carries it)
    QR.dist += hypotf(move[0], move[1]);
}
static bool qr_live_set(const char *key, int v) {
    if (!strcmp(key, "qrun")) { g_qrun = v != 0; LOG("[qrun] qrun %d: %s\n", g_qrun, g_qrun ? "hold Square = Quick Run (Wisdom growth)" : "off (Square = roll / guard only)"); return true; }
    return false;
}
