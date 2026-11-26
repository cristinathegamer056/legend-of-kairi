// (s272, co-op item 4b phase 1, Route D) friends on BotW's full (pause) map: BotW's own Travel Medallion marker jumps onto the
// friend under the cursor, BotW's own Travel / Cancel + warp do the rest. Included once by sora.c after sora_warp.h (needs WP,
// warp_bind, warp_call, warp_dest_get, MMP / sora_hud_minimap_pane, g_body, g_loading, TITLE, LOG). Facts:
// recon/s270/S270_HELPER_MAPWARP.md (sec 1 projection, sec 2 the marker, sec 5 the tests). Ghidra 0x71... = runtime 0x70...
//
// Step 1 (this file now): "set fmprobe 1" = a READ-ONLY live look, nothing changes in game. Once a second (and once when the map
// mode changes) while BotW's AppMap screen is up:
//   [fmwarp] tick ...   frames seen since the last line (does the mod frame run while the pause map is open?), pause state,
//                       the AppMap screen's vtable / +0xfe, map mode (the exe's ui_layout_map_mode: 1 full map, 0 minimap)
//   [fmwarp] probe ...  the map manager d = [0x7001e17ef8] and d2 = [[screen+0x3628]+0x8d8] (pc_cursor.c's), mode848, scale
//                       sx sy (+0x8b0), translation (+0x870), zoom now / goal (+0x878 / +0x888), level (+0x898), angle (+0x87c),
//                       cursor (+0x3868, map units) -> world (cx / sx, -cy / sy)
//   [fmwarp] link ...   Link's world x z -> layout (X*sx*zoom + tx, -Z*sy*zoom + ty) | BotW's Pa_MapIconPlayer_00 pane +0x7c / +0x8c
//                       (found by the game's findPane FUN_7100f6f800, as pc_cursor.c finds Pa_Cursor_00)
//   [fmwarp] medal ...  the medallion marker [d+0x480]: type +0x1c, cat +0x20, t3 +0x24, pos +0x28, flags +0x3c, the pillar
//                       handle d+0x488 alive?, the gdt DestPos (game getter, not in a load), the AoC version [[0x7001d6a2a0]+0xd8]
// Pass (recon sec 5): d == d2, sx sy finite (equal = GUESS), angle 0 on the full map, Link's layout = the pane within ~1 px at
// each zoom level and while panning, "cursor -> world" = where the cursor is.
#pragma once

#define FMW_SCREEN_ID 0x2a                 // the AppMap screen slot (screen_mgr +0x30 [0x2a])
#define FMW_APPMAP_VT 0x7001ce8298ull      // its vtable (pc_cursor.c)
#define FMW_FINDPANE 0x7000f6f800ull       // FUN_7100f6f800(screen, name, 0) -> pane (pc_cursor.c)
#define FMW_AOC_MGR 0x7001d6a2a0ull        // [it]+0xd8 = AoC version (recon sec 2, map_update's gate >= 0x200)
#define FMW_PLAYER_PANE "Pa_MapIconPlayer_00"

static bool g_fmw_probe;  // live fmprobe (s272)

static struct {
    uint64_t log_at;
    uint32_t ticks;      // mod frames since the last line
    int last_mode;       // -2 = none yet
    uint64_t pane;       // Pa_MapIconPlayer_00 (looked up again when the screen changes)
    uint64_t pane_screen;
} FMW = {.last_mode = -2};

static bool fmw_rd(uint64_t a, void *out, size_t n) { return WP.api && WP.api->read(a, out, n); }
static uint64_t fmw_u64(uint64_t a) { uint64_t v = 0; fmw_rd(a, &v, 8); return v; }
static uint32_t fmw_u32(uint64_t a) { uint32_t v = 0; fmw_rd(a, &v, 4); return v; }
static float fmw_f32(uint64_t a) { float v = 0; fmw_rd(a, &v, 4); return v; }

// the AppMap screen (0 = none)
static uint64_t fmw_screen(void) {
    uint64_t sm = WP.api->site("screen_mgr");
    uint64_t mgr = sm ? fmw_u64(sm) : 0;
    if (!mgr || fmw_u32(mgr + 0x28) <= FMW_SCREEN_ID) return 0;
    uint64_t arr = fmw_u64(mgr + 0x30);
    return arr ? fmw_u64(arr + 8 * FMW_SCREEN_ID) : 0;
}

static uint64_t fmw_find_pane(uint64_t screen, const char *name) {
    char nm[32];
    snprintf(nm, sizeof nm, "%s", name);
    SwArgs a = {.x = {screen, 0, 0}, .nx = 3, .x_scratch = 2, .scratch = 32, .init_len = (uint16_t)(strlen(nm) + 1), .init = nm};
    SwRet r = {0};
    if (!WP.api->call(NULL, FMW_FINDPANE, &a, &r)) return 0;
    return WP.api->in_guest(r.x0, 0x90) ? r.x0 : 0;
}

static void fmw_probe_tick(uint64_t now) {
    if (!g_fmw_probe) return;
    FMW.ticks++;
    if (!warp_bind() || !WP.map_ok || g_loading || TITLE.on) return;
    uint64_t screen = fmw_screen();
    if (!screen || !WP.api->in_guest(screen, 0x3648)) return;
    int mode = -1;
    if (!MMP.tried) MMP.tried = true, MMP.fn = (MmPaneFn)(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_minimap_pane");
    if (MMP.fn) {
        MMP.p.size = sizeof MMP.p;
        if (MMP.fn(&MMP.p) >= 0) mode = MMP.p.mode;
    }
    bool chg = mode != FMW.last_mode;
    if (!chg && now < FMW.log_at) return;
    FMW.last_mode = mode, FMW.log_at = now + 1000;
    uint64_t vt = fmw_u64(screen);
    uint8_t fe = 0;
    fmw_rd(screen + 0xfe, &fe, 1);
    uint64_t ps = WP.api->site("pause_state");
    LOG("[fmwarp] tick%s: %u mod frames since the last line, pause state %d, AppMap %llx vt %s fe %u, map mode %d\n", chg ? " CHANGED" : "",
        FMW.ticks, ps ? (int)fmw_u32(ps) : -1, (unsigned long long)screen, vt == FMW_APPMAP_VT ? "ok" : "OTHER", fe, mode);
    FMW.ticks = 0;
    if (mode != 1) return;  // (the rest only on the full map)
    uint64_t d = fmw_u64(WP.fn[WS_MAP_MGR]);
    uint64_t m3628 = fmw_u64(screen + 0x3628), d2 = m3628 ? fmw_u64(m3628 + 0x8d8) : 0;
    if (!d || !WP.api->in_guest(d, 0x3878)) { LOG("[fmwarp] probe: no map manager (d %llx, d2 %llx)\n", (unsigned long long)d, (unsigned long long)d2); return; }
    float sx = fmw_f32(d + 0x8b0), sy = fmw_f32(d + 0x8b4), tx = fmw_f32(d + 0x870), ty = fmw_f32(d + 0x874), zoom = fmw_f32(d + 0x878),
          goal = fmw_f32(d + 0x888), rot = fmw_f32(d + 0x87c), cx = fmw_f32(d + 0x3868), cy = fmw_f32(d + 0x386c), cen_x = fmw_f32(d + 0x880),
          cen_y = fmw_f32(d + 0x884);
    uint32_t lvl = fmw_u32(d + 0x898), m848 = fmw_u32(d + 0x848);
    float wx = sx != 0 ? cx / sx : 0, wz = sy != 0 ? -cy / sy : 0;
    LOG("[fmwarp] probe d=%llx d2=%llx (%s) mode848=%#x sx=%f sy=%f t=%.2f,%.2f zoom=%.4f goal=%.4f lvl=%u rot=%f centre=%.2f,%.2f "
        "cursor=%.2f,%.2f -> world %.2f,%.2f\n",
        (unsigned long long)d, (unsigned long long)d2, d == d2 ? "same" : "DIFFER", m848, sx, sy, tx, ty, zoom, goal, lvl, rot, cen_x, cen_y, cx, cy, wx, wz);
    float lx = g_body[0] * sx * zoom + tx, ly = -g_body[2] * sy * zoom + ty;
    if (FMW.pane_screen != screen || !FMW.pane) FMW.pane = vt == FMW_APPMAP_VT && fe == 2 ? fmw_find_pane(screen, FMW_PLAYER_PANE) : 0, FMW.pane_screen = screen;
    float px = FMW.pane ? fmw_f32(FMW.pane + 0x7c) : 0, py = FMW.pane ? fmw_f32(FMW.pane + 0x8c) : 0;
    if (FMW.pane)
        LOG("[fmwarp] link world %.2f,%.2f -> layout %.2f,%.2f | " FMW_PLAYER_PANE " at %.2f,%.2f (off %.2f,%.2f)\n", g_body[0], g_body[2], lx, ly, px, py, lx - px, ly - py);
    else LOG("[fmwarp] link world %.2f,%.2f -> layout %.2f,%.2f | " FMW_PLAYER_PANE " NOT FOUND (vt %s, fe %u)\n", g_body[0], g_body[2], lx, ly, vt == FMW_APPMAP_VT ? "ok" : "other", fe);
    uint64_t obj = fmw_u64(d + WM_OBJ);
    float ip[3] = {0}, dp[3] = {0}, dd = 0;
    int fx = -1;
    warp_map_read(d, ip, &fx);
    bool dok = warp_dest_get(dp, &dd);
    uint64_t aoc = fmw_u64(FMW_AOC_MGR);
    LOG("[fmwarp] medal obj=%llx type=%x cat=%x t3=%x pos=%.2f,%.2f,%.2f flags=%x | DestPos=%s%.2f,%.2f,%.2f (%.1f deg) aoc=%x | effect live=%d\n",
        (unsigned long long)obj, obj ? fmw_u32(obj + 0x1c) : 0, obj ? fmw_u32(obj + 0x20) : 0, obj ? fmw_u32(obj + 0x24) : 0, ip[0], ip[1], ip[2],
        obj ? fmw_u32(obj + 0x3c) : 0, dok ? "" : "(not read) ", dp[0], dp[1], dp[2], dd, aoc ? fmw_u32(aoc + 0xd8) : 0, fx);
}

// ---- step 2: "set fmborrow 1" (default OFF). While the full map is open, BotW's medallion marker [d+0x480] jumps onto the friend
// nearest the map cursor (within fmr layout units, default 40; kept until 2 x fmr) with the game's own FUN_7100d88670(d, 0x2a,
// &pos) (no gdt write). BotW's own hover / "Travel" / Travel-Cancel dialog run unchanged. Leaving the friend or closing the map
// puts the marker back: a real medallion slot = d88670(d, 0x2a, &DestPos) (vanilla's own path); an empty slot (0,0,0) = its parked
// spot written back as memory (pos -100000,0,0 as at init, flag 0x200 "warp-able" cleared, 0x4000 dirty set; the show / hide call
// stays a GUESS so it is not used). The Yes: BotW writes "MainField/WarpIcon" into the UI mgr [0x7001e328d8]+0x650a8; seen while a
// borrow is up -> sora_warp.h warp_borrow(spot) at once (the slot is read only at the stage change, later), BotW closes the menu
// and calls its warp itself; the run restores the slot after the arrival (warpicon 1 = the marker + pillar too).
// Spot: a real friend = his feet now (GUESS: his live sample is on the ground; the two-copy run checks it); a PRETEND friend (mmfake)
// = YOUR OWN spot (a safe full-chain test: the blue dissolve, the load, the arrival where you stand). "set fmlog 1" = more lines.
#define FMW_UI_MGR 0x7001e328d8ull   // [it] = the UI manager; +0x650a8 / +0x650b0 = the map warp's strings after BotW's Yes
#define FMW_UI_WARP 0x650a8
#define FMW_PEND_MS 3000
#define FMW_DIALOG_ID 0x3e           // BotW's Travel / Cancel dialog screen (screen_mgr slot)
static bool g_fmw_borrow = true, g_fmw_log;  // (s273: default ON, his two-copy run "it worked good")
static int g_fmw_r = 40;

static struct {
    int k;               // borrowed friend: 0..NET_MAX_PEERS-1 real, 100+i pretend i, -1 none
    float pos[3];        // where the marker is now
    float saved_pos[3];  // the marker's parked pos before the first move (restore of an empty slot)
    bool saved, yes_done, frozen;
    uint64_t pend_until;  // (s272 live: the map closed with the Travel dialog up: wait for the Yes / the warp-out before restoring)
    uint64_t moved_at;
} FB = {.k = -1};

static uint64_t fmw_screen_at(int id) {
    uint64_t sm = WP.api->site("screen_mgr");
    uint64_t mgr = sm ? fmw_u64(sm) : 0;
    if (!mgr || fmw_u32(mgr + 0x28) <= (uint32_t)id) return 0;
    uint64_t arr = fmw_u64(mgr + 0x30);
    return arr ? fmw_u64(arr + 8 * id) : 0;
}
static bool fmw_icon_set(uint64_t d, const float p[3]) {
    float v[4] = {p[0], p[1], p[2], 0};
    SwArgs b = {.x = {d, 0x2a, 0}, .nx = 3, .x_scratch = 4, .scratch = 16, .init_len = 12, .init = v};
    return warp_call(WS_MAP_ICON, &b, NULL);
}
static void fmw_restore(uint64_t d, const char *why) {
    if (FB.k < 0) return;
    float dp[3] = {0}, dd = 0;
    bool real = warp_dest_get(dp, &dd) && (dp[0] != 0 || dp[1] != 0 || dp[2] != 0);
    bool ok = false;
    uint64_t obj = d ? fmw_u64(d + WM_OBJ) : 0;
    if (real && d) ok = fmw_icon_set(d, dp);
    else if (obj && FB.saved) {
        uint32_t fl = fmw_u32(obj + 0x3c);
        fl = (fl & ~0x200u) | 0x4000u;
        ok = WP.api->write(obj + 0x28, FB.saved_pos, 12) && WP.api->write(obj + 0x3c, &fl, 4);
    }
    LOG("[fmwarp] restore (%s): marker %s %s\n", why, ok ? "back on" : "NOT put back on", real ? "your medallion" : "its parked spot (no medallion placed)");
    FB.k = -1, FB.frozen = false;
}

// the friends that can be picked: world spots (y = feet; pretend ones = your height)
static int fmw_friends(float out[][3], int *ids, int max, uint64_t now) {
    int n = 0;
    if (NET.on)
        for (int k = 0; k < NET_MAX_PEERS && n < max; k++) {
            NetPeerSlot *sl = &NET.slot[k];
            if (!sl->used || sl->hidden) continue;
            PeerPose q = peer_sample(&sl->p, now);
            if (!q.valid || (q.flags & NF_LOADING)) continue;
            memcpy(out[n], q.feet, 12), ids[n++] = k;
        }
    if (g_mm_fake) {  // (the same three as the minimap's: sora_net_minimap.h mm_tick)
        float t = (float)(now % 600000) * 0.001f;
        const float off[3][2] = {{100.0f * cosf(t * 0.3f), 100.0f * sinf(t * 0.3f)}, {0, 40.0f}, {283.0f, -283.0f}};
        for (int i = 0; i < 3 && n < max; i++) out[n][0] = g_body[0] + off[i][0], out[n][1] = g_body[1], out[n][2] = g_body[2] + off[i][1], ids[n++] = 100 + i;
    }
    return n;
}

static void fmw_borrow_tick(uint64_t now) {
    if (!g_fmw_borrow && FB.k < 0) return;
    if (!warp_bind() || !WP.map_ok || g_loading || TITLE.on) { FB.k = -1; return; }
    uint64_t d = fmw_u64(WP.fn[WS_MAP_MGR]);
    if (!d || !WP.api->in_guest(d, 0x3878)) { FB.k = -1; return; }
    uint64_t screen = fmw_screen_at(FMW_SCREEN_ID);
    int mode = -1;
    if (!MMP.tried) MMP.tried = true, MMP.fn = (MmPaneFn)(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_minimap_pane");
    if (MMP.fn) {
        MMP.p.size = sizeof MMP.p;
        if (MMP.fn(&MMP.p) >= 0) mode = MMP.p.mode;
    }
    uint8_t fe = 0;
    if (screen) fmw_rd(screen + 0xfe, &fe, 1);
    uint64_t ps = WP.api->site("pause_state");
    bool on_map = screen && fmw_u64(screen) == FMW_APPMAP_VT && fe == 2 && mode == 1 && ps && fmw_u32(ps) == 2;
    // BotW's Yes. s272 live: the pointer test alone never fired (the map closed first, the restore ran, BotW warped to the empty
    // slot). Now: the UI mgr's +0x650a8 word as a pointer OR the text it points at; and after the map closed with the Travel dialog
    // up, FMW_PEND_MS of waiting for that Yes or the warp-out itself (Link state 0x1 / the UI event bit) before restoring.
    uint64_t ui = fmw_u64(FMW_UI_MGR), w0 = 0, w1 = 0;
    if (ui && WP.api->in_guest(ui + FMW_UI_WARP, 16)) w0 = fmw_u64(ui + FMW_UI_WARP), w1 = fmw_u64(ui + FMW_UI_WARP + 8);
    char txt[20] = {0};
    if (w0 && WP.api->in_guest(w0, 19)) fmw_rd(w0, txt, 19);
    bool yes = w0 && (w0 == WP.fn[WS_STR_MAP] || !strcmp(txt, "MainField/WarpIcon"));
    if (FB.k >= 0 && !on_map && FB.frozen && !FB.pend_until) {
        FB.pend_until = now + FMW_PEND_MS;
        LOG("[fmwarp] the map closed with the Travel dialog up: waiting %d ms for BotW's Yes (UI mgr %llx +0x650a8 %llx \"%.18s\" +0x650b0 %llx)\n", FMW_PEND_MS,
            (unsigned long long)ui, (unsigned long long)w0, txt, (unsigned long long)w1);
    }
    bool out = false;
    if (FB.pend_until && !yes) {
        bool ev = false;
        out = linkpos_link_state_word() == 0x1 || (warp_bool(WS_UI, &ev) && ev && warp_bool(WS_UI_EVENT, &ev) && ev);
    }
    if (FB.k >= 0 && !FB.yes_done && (yes || out)) {
        FB.yes_done = true, FB.pend_until = 0;
        float sp[3], deg = 0;
        const char *what;
        if (FB.k >= 100) sp[0] = g_body[0], sp[1] = g_body[1], sp[2] = g_body[2], deg = wsp_deg_norm(S.face * 57.2957795f), what = "fmborrow (pretend friend: your own spot)";
        else memcpy(sp, FB.pos, 12), what = "fmborrow (friend's feet)";
        LOG("[fmwarp] %s on %s %d: slot <- %.2f %.2f %.2f (%.1f deg) | UI mgr +0x650a8 %llx \"%.18s\"\n", yes ? "Yes seen" : "warp-out seen (no Yes string)",
            FB.k >= 100 ? "pretend friend" : "friend", FB.k >= 100 ? FB.k - 99 : FB.k + 1, sp[0], sp[1], sp[2], deg, (unsigned long long)w0, txt);
        if (!warp_borrow(sp, deg, what, 15000, now)) LOG("[fmwarp] the slot was NOT borrowed: BotW warps to the medallion slot as it is\n");
        else {
            // (s273 two-copy run: the friend got no "<name> warped to you" on a map warp) the run sends the NOTE once the game took it
            WP.k = FB.k < 100 ? FB.k : -1, WP.nonce = 0, WP.note_pend = WP.k >= 0;
            warp_check_start(sp, deg, false, "fmborrow");
        }
        FB.k = -1, FB.frozen = false;  // (the marker stays where it is; the run + warpicon handle it after the arrival)
        return;
    }
    if (!w0) FB.yes_done = false;
    if (FB.pend_until) {
        if (on_map) FB.pend_until = 0;  // (back on the map: Cancel after all)
        else if (now < FB.pend_until) return;
        else { fmw_restore(d, "no Yes / warp-out within the wait"); return; }
    }
    if (!on_map) { if (FB.k >= 0) fmw_restore(d, "the full map closed"); return; }
    if (!g_fmw_borrow) { fmw_restore(d, "fmborrow off"); return; }
    if (WP.run.st != WR_IDLE) return;  // (a warp holds the slot)
    // the Travel dialog up: keep the marker where it is (GUESS: its screen's +0xfe == 2 while shown, as the map's)
    uint64_t dlg = fmw_screen_at(FMW_DIALOG_ID);
    uint8_t dfe = 0;
    if (dlg) fmw_rd(dlg + 0xfe, &dfe, 1);
    bool frozen = dfe == 2;
    if (frozen != FB.frozen) LOG("[fmwarp] dialog %s (screen 0x3e %llx fe %u)\n", frozen ? "UP: marker held" : "gone", (unsigned long long)dlg, dfe);
    FB.frozen = frozen;
    if (frozen) return;
    float sx = fmw_f32(d + 0x8b0), sy = fmw_f32(d + 0x8b4), zoom = fmw_f32(d + 0x878), cx = fmw_f32(d + 0x3868), cy = fmw_f32(d + 0x386c);
    float fr[NET_MAX_PEERS + 3][3];
    int ids[NET_MAX_PEERS + 3];
    int n = fmw_friends(fr, ids, NET_MAX_PEERS + 3, now), best = -1;
    float bd = 1e30f, cur_d = 1e30f;
    for (int i = 0; i < n; i++) {
        float dx = (fr[i][0] * sx - cx) * zoom, dy = (-fr[i][2] * sy - cy) * zoom, dist = sqrtf(dx * dx + dy * dy);
        if (dist < bd) bd = dist, best = i;
        if (ids[i] == FB.k) cur_d = dist;
    }
    int pick = -1;
    float pd = 0;
    if (FB.k >= 0 && cur_d <= 2.0f * g_fmw_r) {  // (hold the one we are on)
        for (int i = 0; i < n; i++)
            if (ids[i] == FB.k) pick = i;
        pd = cur_d;
    }
    if (pick < 0 && best >= 0 && bd <= g_fmw_r) pick = best, pd = bd;
    if (pick < 0) {
        if (FB.k >= 0) fmw_restore(d, "the cursor left the friend");
        return;
    }
    uint64_t obj = fmw_u64(d + WM_OBJ);
    if (!obj) return;
    if (FB.k < 0 && !FB.saved) {
        fmw_rd(obj + 0x28, FB.saved_pos, 12), FB.saved = true;
        if (FB.saved_pos[0] > -50000) FB.saved_pos[0] = -100000, FB.saved_pos[1] = 0, FB.saved_pos[2] = 0;  // (a shown medallion: its restore = d88670 anyway)
    }
    float mv = fabsf(fr[pick][0] - FB.pos[0]) + fabsf(fr[pick][2] - FB.pos[2]);
    if (ids[pick] != FB.k || (mv > 1.0f && now >= FB.moved_at + 100)) {
        bool ok = fmw_icon_set(d, fr[pick]);
        if (ids[pick] != FB.k || g_fmw_log)
            LOG("[fmwarp] borrow %s %d at %.1f,%.1f,%.1f dist %.1f layout units (zoom %.3f): %s\n", ids[pick] >= 100 ? "pretend" : "friend",
                ids[pick] >= 100 ? ids[pick] - 99 : ids[pick] + 1, fr[pick][0], fr[pick][1], fr[pick][2], pd, zoom, ok ? "marker moved (FUN_7100d88670)" : "REFUSED");
        if (!ok) return;
        FB.k = ids[pick], memcpy(FB.pos, fr[pick], 12), FB.moved_at = now;
    }
}

static bool fmw_live_key(const char *key, int v) {
    if (!strcmp(key, "fmprobe")) g_fmw_probe = v != 0, FMW.last_mode = -2, FMW.pane = 0, LOG("[fmwarp] full map probe %s\n", g_fmw_probe ? "on (read only)" : "off");
    else if (!strcmp(key, "fmborrow")) g_fmw_borrow = v != 0, LOG("[fmwarp] fmborrow %s\n", g_fmw_borrow ? "on: the medallion marker jumps onto the friend under the cursor on the full map" : "off");
    else if (!strcmp(key, "fmlog")) g_fmw_log = v != 0, LOG("[fmwarp] fmlog %d\n", g_fmw_log);
    else if (!strcmp(key, "fmr")) g_fmw_r = v < 5 ? 5 : v > 300 ? 300 : v, LOG("[fmwarp] pick radius %d layout units\n", g_fmw_r);
    else return false;
    return true;
}

// once a mod frame (sora_mod_frame, after warp_tick; also with Sora mode off)
static void fmw_tick(uint64_t now) {
    fmw_probe_tick(now);
    fmw_borrow_tick(now);
}
