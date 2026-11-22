// s94 b8-chestev: BotW's "camp cleared -> treasure chest unlocks" camera event. Link is held by the game, Sora walked
// around (user: "i'm also able to move around when link normal cant"). No screen and no Link state word showed it
// (state stays ground), and no log of one exists, so the signal is a GUESS: the demo camera leaves Link.
// recon/S94_HELPER_CHESTEV.md. Included by sora.c after the s85g probe (uses g_probe_on, probe_start, link_actor).
//
// Detached camera = the eye is farther than g_evcam metres from Link, or Link is more than g_evang degrees off the
// camera's view axis (the gameplay orbit camera keeps him near the screen centre). Held g_evon presents = event on;
// g_evoff presents attached again = off. While on (and "set evfreeze 1"): Sora's press / speed are 0, Link has the pad.
// Live keys: set evfreeze 0/1, set evrec 0/1 (log + start the Link-actor word probe at each detach edge),
// set evcam <m>, set evang <deg>, set evon <presents>, set evoff <presents>. Log lines "[sora] chestev: ...".
// (the knob variables are declared at the top of sora.c, next to g_chev)
static bool g_chev_det;      // the camera is detached right now (the raw reading)
// (evstick) user: "there was a moment where the kh2 hud would disappear at a certain angle". Log: 7 "event ON" with no
// chest (3.1..6.1 m, 41..81 deg): the player's own camera (tilted up / swung round) puts Link > g_evang off axis, so Sora
// froze and the HUD hid (g_event). A demo camera moves with no camera input, so with this on a detach that starts within
// EVSTICK_PRESENTS of right-stick input (or sees stick input while it lasts) is the player's: it never turns the event on
// until the camera is back on Link. SAVAGEWIND_SORA_EV_STICK=1 / live "set evstick 0/1"; default 0. GUESS: a gyro / mouse
// camera is not seen here.
// (g_evstick: declared in sora.c, before the env / live parsers)
#define EVSTICK_PRESENTS 60
#define EVSTICK_DZ 6000  // (of 32767)

static bool chev_measure(const float p[3], float *dist, float *ang) {
    float m[12];
    if (!linkpos_camera_view(m)) return false;
    // eye (world) = -R^T t, R = rows 0..2 (right / up / back), t = column 3; view axis = -back = -row 2
    float e[3];
    for (int c = 0; c < 3; c++) e[c] = -(m[0 * 4 + c] * m[0 * 4 + 3] + m[1 * 4 + c] * m[1 * 4 + 3] + m[2 * 4 + c] * m[2 * 4 + 3]);
    float d[3] = {p[0] - e[0], p[1] - e[1], p[2] - e[2]};
    float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (l < 1e-3f) return false;
    float cs = -(d[0] * m[8] + d[1] * m[9] + d[2] * m[10]) / l;
    *dist = l, *ang = acosf(cs > 1 ? 1 : cs < -1 ? -1 : cs) * 180.0f / PI;
    return true;
}

static void chestev_tick(uint64_t present, const float p[3]) {
    static int on_n, off_n;
    static uint64_t at;
    float dist, ang;
    {  // (revfix) a loose camera (cam_alive only, off cam_track) may be a wrong one: its distance / angle must not freeze Sora
        static bool tried;
        static bool (*loose_fn)(void);
        if (!tried) tried = true, loose_fn = (bool (*)(void))(void *)GetProcAddress(GetModuleHandleA(NULL), "linkpos_camera_loose");
        if (loose_fn && loose_fn()) {
            on_n = 0, g_chev_det = false;
            if (g_chev && ++off_n >= g_evoff) g_chev = false, LOG("[sora] chestev: event OFF at present %llu (camera not tracking)\n", (unsigned long long)present);
            return;
        }
    }
    if (!chev_measure(p, &dist, &ang)) return;
    bool det = dist > (float)g_evcam || (ang > (float)g_evang && dist >= (float)g_evmin);  // (s94 review: evmin)
    {  // (evstick) the player's own camera turn is no event
        static uint64_t stick_at;
        static bool mine;
        int32_t rx = g_rx_raw, ry = g_ry_raw;
        bool stick = rx > EVSTICK_DZ || rx < -EVSTICK_DZ || ry > EVSTICK_DZ || ry < -EVSTICK_DZ;
        if (stick) stick_at = present;
        bool recent = stick_at && present - stick_at <= EVSTICK_PRESENTS;
        if (!det) mine = false;
        else if (!mine && g_evstick && recent && !g_chev) {
            mine = true;
            LOG("[sora] chestev: camera off Link (%.1f m, %.1f deg) after right-stick input: the player's camera, no event (evstick)\n", dist, ang);
        }
        if (mine && g_evstick) det = false;
    }
    if (det != g_chev_det && g_evrec)
        LOG("[sora] chestev: camera %s at present %llu: eye %.1f m from Link, Link %.1f deg off axis (Link state %#x, pause %u, event %d)\n",
            det ? "DETACHED" : "back on Link", (unsigned long long)present, dist, ang, linkpos_link_state_word(), GV(uint32_t, PAUSE_STATE, 0),
            g_chev);
    if (det && !g_chev_det && g_evrec && !PR.on) {  // the actor words that flip with the demo = a better signal than the camera
        int keep = g_probe_on;
        g_probe_on = 1, probe_start(present), g_probe_on = keep;
    }
    g_chev_det = det;
    if (g_evrec && det && present - at >= 30) at = present, LOG("[sora] chestev: detached %.1f m %.1f deg, Link %.2f %.2f %.2f\n", dist, ang, p[0], p[1], p[2]);
    on_n = det ? on_n + 1 : 0, off_n = det ? 0 : off_n + 1;
    if (!g_chev && on_n >= g_evon && g_evfreeze) g_chev = true, LOG("[sora] chestev: event ON at present %llu (%.1f m, %.1f deg): Sora frozen, Link has the pad (GUESS signal)\n", (unsigned long long)present, dist, ang);
    else if (g_chev && (off_n >= g_evoff || !g_evfreeze)) g_chev = false, LOG("[sora] chestev: event OFF at present %llu\n", (unsigned long long)present);
}
