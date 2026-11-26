// Multiplayer slice 3: the remote Donald + Goofy (spec D:/SoraBotW/docs/specs/2026-11-06-multiplayer-slice3-design.md, unit A).
// Included once by sora.c right after sora_net_avatar.h (needs AV / av_dot_path / g_av_party_ok from it, PD / pwf_sample from
// sora_party.h). The sender side is sora_net_pcap.h (what party_draw_member / pl_draw drew -> SNAP pm).
//   draw: two draw actors, loaded lazily the first time a member is shown, from the files the local party loads with "/./"
//         before the file name (the exe dedupes actors by path string: the plain spelling would hand back our own Donald).
//         Poses = the 8-frame blend (PD.slot_pose from + current, per-bone lerp, PD.world) when NPM_BLEND, else PD.pose;
//         shadow at feet y + floor_dy; water ghost; weapon on / off; Goofy Bash's flying shield (PD.weapon_at + pwf_sample).
//   (mp4: per peer slot k, see NP[k])
//   hidden: netparty 0, net off, !g_av_local_ok, his Sora not drawn / faded / far (g_av_party_ok), the member absent from
//         the bracketing SNAP (absorbed by a Drive Form, party off, ...), the actor failed to load.
//   loopback: the same side shift as his Sora (g_av_shift), so the party keeps its formation with her.
//   sounds: NONE here on purpose: the local party's sounds go through sora_sfx_play_at (the net_note_sfx macro), i.e. they
//         already reach the peer as sfx events and play at the world pos; a second sound path here would double them (and the
//         macro would send ours back out).
#pragma once

// (mp4, slice 4) per peer slot k: NP[k] = that peer's two actors (path spelling with k + 1 "/./" inserts, so each slot has its own).
#define NP_MAX_BONES 256
#define NP_CHAR_MAX 4  // (s267 review F5) party characters loaded per friend (Donald + Goofy + 2 stand-ins), never freed

// (s267 co-op item 2) a party SLOT w of a friend shows the CHARACTER his SNAP names (NetPartyMember.chr: Donald, Goofy, Riku, ...):
// actors are kept per (peer slot k, character c) in act[k][c] (the exe never frees one; a friend who switches member gets the next
// one loaded once), id[w] / chr[w] = the one slot w draws now (-1 = none yet).
static struct {
    int id[NET_PARTY_N], chr[NET_PARTY_N];  // the actor slot w uses now (0 = none) + its character (-1 = none)
    int act[PCH_N];            // per character: 0 = not tried, > 0 = loaded, -1 = failed (that character is off for the session)
    bool shown[NET_PARTY_N], vis[NET_PARTY_N], sh_on[NET_PARTY_N], init;
    int wp[NET_PARTY_N], slot[NET_PARTY_N];  // weapon shown last set (-1 unset), slot drawn last
    float gh[NET_PARTY_N], feet[NET_PARTY_N][3], face[NET_PARTY_N];
} NP[NET_MAX_PEERS];
static bool np_notready_logged, np_badchr_logged;
static float np_from[NP_MAX_BONES * 12], np_cur[NP_MAX_BONES * 12];  // (scratch: a blend is built and drawn at once)
static void np_init(int k) {  // (the zero state wants wp = 1 (shown), gh = -1, chr = -1)
    if (NP[k].init) return;
    NP[k].init = true;
    for (int w = 0; w < NET_PARTY_N; w++) NP[k].wp[w] = 1, NP[k].gh[w] = -1, NP[k].chr[w] = -1;
}
// the character of a SNAP member: its wire chr (PCH_* + 1; 0 = the slot's own member); -1 = an id this build does not know
static int np_chr(int w, uint8_t chr) {
    if (!chr) return w >= 0 && w < PCH_SLOTS ? PCH_HOME[w] : -1;
    return pchar_valid(chr - 1) ? chr - 1 : -1;
}

// (s267) the motion table of a friend's stand-in (only slot / frames / loop: the clock wrap), read once from his <key>_motion.bin
// when this game has not loaded him itself (PCS)
static PMotX NPM[PCH_N][PMOT_MAX];
static int NPM_N[PCH_N];  // -1 = the file did not read
static void np_mot_load(int c) {
    char p[512];
    party_char_path(c, "motion", p, sizeof p);
    FILE *f = fopen(p, "rb");
    uint8_t hd[16], d[24];
    NPM_N[c] = -1;
    if (!f) { LOG("[net] remote %s: no motion bake (%s): his clock is not wrapped\n", PCH_TAB[c].name, p); return; }
    if (fread(hd, 1, 16, f) == 16 && !memcmp(hd, "SKRM", 4)) {
        uint32_t count;
        memcpy(&count, hd + 8, 4);
        NPM_N[c] = 0;
        for (uint32_t k = 0; k < count && NPM_N[c] < PMOT_MAX && fread(d, 1, 24, f) == 24; k++) {
            PMotX *x = &NPM[c][NPM_N[c]];
            memset(x, 0, sizeof *x);
            memcpy(&x->slot, d, 2), memcpy(&x->frames, d + 2, 2), memcpy(&x->loop, d + 4, 2), memcpy(&x->flags, d + 6, 2);
            if (x->frames) NPM_N[c]++;
        }
    }
    fclose(f);
    LOG("[net] remote %s: motion table %d slots (%s)\n", PCH_TAB[c].name, NPM_N[c], p);
    if (!NPM_N[c]) NPM_N[c] = -1;  // (s267 review F6: no rows = tried, not read again every frame)
}
static const PMotX *np_mot(int c, int slot) {
    if (!pchar_valid(c)) return NULL;
    if (PCH_TAB[c].home >= 0) return party_home_mot(PCH_TAB[c].home, slot);  // (Donald / Goofy: as before s267)
    if (PCS[c].ok) {  // (this game loaded him too: his own table)
        for (int k = 0; k < PCS[c].nmot; k++)
            if (PCS[c].mot[k].slot == slot) return &PCS[c].mot[k];
        return NULL;
    }
    if (!NPM_N[c]) np_mot_load(c);
    for (int k = 0; k < NPM_N[c]; k++)
        if (NPM[c][k].slot == slot) return &NPM[c][k];
    return NULL;
}
static float np_shadow_r(int c) { return PCH_TAB[c].home >= 0 ? party_home_def(PCH_TAB[c].home)->shadow_r : PCHAR_DEF[c]->shadow_r; }

static bool net_party_local_pose(int who, float feet[3], float *face) { return pcap_get(who, feet, face); }
static bool net_party_remote_pose(int k, int who, float feet[3], float *face) {
    if (k < 0 || k >= NET_MAX_PEERS || who < 0 || who >= NET_PARTY_N) return false;
    if (NP[k].vis[who]) {
        memcpy(feet, NP[k].feet[who], 3 * sizeof(float)), *face = NP[k].face[who];
        return true;
    }
    // (review I2) not drawn HERE (netparty 0, no actor, his Sora fading, first frame) but in his SNAP: his sampled place,
    // so his spell fx still land where he is; false only when his game did not draw him either
    PartyPose q = peer_party_sample(&NETP(k), who, GetTickCount64());
    if (!q.valid) return false;
    feet[0] = q.feet[0] + (g_net_loop ? g_av_shift[k][0] : 0), feet[1] = q.feet[1], feet[2] = q.feet[2] + (g_net_loop ? g_av_shift[k][1] : 0);
    *face = q.face;
    return true;
}

// peer slot k's actor of character c (lazy load, cached for the session); 0 = not ready yet (the local party has not loaded the
// draw API), -1 = failed. (s267: by character: a stand-in's files are <key>_model / _pose.bin, k + 1 "/./" inserts as before so
// the exe's path dedupe never hands back our own Donald / Riku)
static int np_actor(int k, int c) {
    np_init(k);
    if (!pchar_valid(c)) return -1;
    if (NP[k].act[c]) return NP[k].act[c];
    if (!PD.load || !PD.pose || !PD.shadow || !PD.shape || !PD.off) {
        if (!np_notready_logged) np_notready_logged = true, LOG("[net] remote party: the party draw API is not loaded (party off / old exe), waiting\n");
        return 0;
    }
    if (!av_init()) { NP[k].act[c] = -1; return -1; }
    int nk = 0;  // (s267 review F5: at most NP_CHAR_MAX characters per friend: the exe never frees an actor)
    for (int j = 0; j < PCH_N; j++) nk += NP[k].act[j] > 0;
    if (nk >= NP_CHAR_MAX) {
        NP[k].act[c] = -1;
        LOG("[net] slot %d remote %s: this friend already showed %d party characters (the most per friend), not drawn (this session)\n", k + 1, PCH_TAB[c].name, nk);
        return -1;
    }
    int cap = AV.cap ? AV.cap() : 0;
    if (cap && cap - av_used() <= AV_RESERVE) {  // (review I1: as the remote forms, AV_RESERVE stay free for local summons / skate / Roxas)
        NP[k].act[c] = -1;
        LOG("[net] slot %d remote %s: %d of %d draw actors in use, not drawn (this session)\n", k + 1, PCH_TAB[c].name, av_used(), cap);
        return -1;
    }
    char mp[512], pp[512], dm[600], dp[600];
    party_char_path(c, "model", mp, sizeof mp), party_char_path(c, "pose", pp, sizeof pp);
    if (!party_file_ok(mp) || !party_file_ok(pp)) {
        NP[k].act[c] = -1;
        LOG("[net] slot %d remote %s: his bake is not installed here (%s), not drawn (this session)\n", k + 1, PCH_TAB[c].name, party_file_ok(mp) ? pp : mp);
        return -1;
    }
    av_dot_pathn(mp, dm, sizeof dm, k + 1), av_dot_pathn(pp, dp, sizeof dp, k + 1);
    uint64_t t = GetTickCount64();
    int id = PD.load(dm, dp);
    NP[k].act[c] = id > 0 ? id : -1;
    if (id > 0) {
        PD.shape(id, np_shadow_r(c), 0, 0, 0);
        if (PD.weapon) PD.weapon(id, true);
        if (PD.weapon_at) PD.weapon_at(id, NULL, 0, NULL, 0, 0);
        LOG("[net] slot %d remote %s loaded (%llu ms): actor %d (%s)\n", k + 1, PCH_TAB[c].name, (unsigned long long)(GetTickCount64() - t), id, dm);
    } else LOG("[net] slot %d remote %s: load failed (%s), not drawn (this session)\n", k + 1, PCH_TAB[c].name, dm);
    return NP[k].act[c];
}

static void np_hide(int k, int who) {
    int o = 1 - who;  // (s267: a character who moved to the other slot this frame keeps his actor shown there)
    bool other = o >= 0 && o < NET_PARTY_N && NP[k].shown[o] && NP[k].id[o] == NP[k].id[who];
    if (NP[k].shown[who] && PD.off && NP[k].id[who] > 0 && !other) PD.off(NP[k].id[who]), NP[k].chr[who] = -1;  // (review F4: shown again = a clean start)
    NP[k].shown[who] = NP[k].vis[who] = false;
}
static void net_party_slot_off(int k) { for (int w = 0; w < NET_PARTY_N; w++) np_hide(k, w); }
static void net_party_off(void) { for (int k = 0; k < NET_MAX_PEERS; k++) net_party_slot_off(k); }

static void np_slot_tick(int k, uint64_t now) {
    np_init(k);
    bool on = !g_net_noparty && NET.on && g_av_local_ok && g_av_party_ok[k] && NET.slot[k].used;
    float to_u = 0.01f * g_k;
    for (int w = 0; w < NET_PARTY_N; w++) {
        PartyPose q;
        int id = 0;
        memset(&q, 0, sizeof q);
        if (on) q = peer_party_sample(&NETP(k), w, now);
        int c = q.valid ? np_chr(w, q.chr) : -1;
        if (on && q.valid && c < 0 && !np_badchr_logged)
            np_badchr_logged = true, LOG("[net] slot %d party slot %d: character %d is not known to this build (a newer copy?): not drawn\n", k + 1, w, q.chr - 1);
        if (!on || !q.valid || c < 0 || (id = np_actor(k, c)) <= 0) { np_hide(k, w); continue; }
        if (NP[k].chr[w] != c) {  // (s267: he switched member: the old body goes, the new one starts clean)
            if (NP[k].chr[w] >= 0) LOG("[net] slot %d party slot %d: %s -> %s\n", k + 1, w, PCH_TAB[NP[k].chr[w]].name, PCH_TAB[c].name);
            np_hide(k, w);
            NP[k].chr[w] = c, NP[k].id[w] = id, NP[k].wp[w] = 1, NP[k].gh[w] = -1, NP[k].sh_on[w] = false;
            if (PD.weapon) PD.weapon(id, true);
            if (PD.ghost) PD.ghost(id, 0.0f), NP[k].gh[w] = 0;
        }
        float feet[3] = {q.feet[0] + (g_net_loop ? g_av_shift[k][0] : 0), q.feet[1], q.feet[2] + (g_net_loop ? g_av_shift[k][1] : 0)};
        const PMotX *mo = np_mot(c, q.slot);  // (s267: the motion of the character he sent)  // (the clock past its motion's end, as the local party's)
        float clock = mo ? peer_clock_wrap(q.clock, mo->frames, mo->loop, mo->loop < mo->frames) : q.clock;
        PD.shadow(id, feet[1] + q.floor_dy, blob_str(0.55f));
        if (PD.ghost) {
            float gh = (q.flags & NPM_GHOST) ? g_swim_ghost / 100.0f : 0.0f;
            if (gh != NP[k].gh[w]) PD.ghost(id, gh), NP[k].gh[w] = gh;
        }
        int wp = (q.flags & NPM_WEAPON) != 0;
        if (PD.weapon && wp != NP[k].wp[w]) PD.weapon(id, wp != 0), NP[k].wp[w] = wp;
        if (PD.weapon_at) {  // (Goofy Bash's shield in flight; the older exe has no weapon_at: not drawn, as local)
            // (s267: only Goofy throws it; the fly bones = Goofy's g_pwf[1], loaded once at party_load, never swapped)
            if ((q.flags & NPM_SHIELD) && c == PCH_GOOFY) {
                const float *wb = pwf_sample(1, q.shield_t);
                float sp[3] = {q.shield[0] + (feet[0] - q.feet[0]), q.shield[1], q.shield[2] + (feet[2] - q.feet[2])};
                PD.weapon_at(id, wb, wb ? g_pwf[1].nb : 0, sp, q.shield_yaw, to_u), NP[k].sh_on[w] = true;
            } else if (NP[k].sh_on[w]) PD.weapon_at(id, NULL, 0, NULL, 0, 0), NP[k].sh_on[w] = false;
        }
        int nb = PD.bones ? PD.bones(id) : 0;
        bool drawn = false;
        if ((q.flags & NPM_BLEND) && nb > 0 && nb <= NP_MAX_BONES && PD.slot_pose && PD.world &&
            PD.slot_pose(id, q.from_slot, q.from_clock, np_from) && PD.slot_pose(id, q.slot, clock, np_cur)) {
            float wgt = q.blend;  // (party_draw_member's per-bone lerp; all bones: the wire has no catch-phase flag, GUESS)
            for (int bi = 0; bi < nb; bi++) {
                float *d = np_cur + bi * 12, *a = np_from + bi * 12, len[3];
                for (int r = 0; r < 3; r++) {
                    float la = sqrtf(v3dot(a + 3 * r, a + 3 * r)), lb = sqrtf(v3dot(d + 3 * r, d + 3 * r));
                    len[r] = la + (lb - la) * wgt;
                }
                for (int j = 0; j < 12; j++) d[j] = a[j] + (d[j] - a[j]) * wgt;
                ortho_rows(d, len);
            }
            PD.world(id, np_cur, feet, q.face, to_u);
            drawn = true;
        }
        if (!drawn) PD.pose(id, q.slot, clock, feet, q.face, to_u);
        NP[k].slot[w] = q.slot, NP[k].shown[w] = NP[k].vis[w] = true;
        memcpy(NP[k].feet[w], feet, sizeof feet), NP[k].face[w] = q.face;
    }
}

static void net_party_tick(uint64_t now) { for (int k = 0; k < NET_MAX_PEERS; k++) np_slot_tick(k, now); }

static void net_party_diag(void) {
    LOG("[net] remote party: %s\n", g_net_noparty ? "OFF (set netparty 0)" : "on");
    for (int k = 0; k < NET_MAX_PEERS; k++)
        LOG("[net]   slot %d%s: Donald's place %s %s actor %d slot %d, Goofy's place %s %s actor %d slot %d\n", k + 1,
            g_av_party_ok[k] ? "" : " (his Sora not drawn: party hidden)", NP[k].chr[0] >= 0 ? PCH_TAB[NP[k].chr[0]].name : "-",
            NP[k].shown[0] ? "shown" : "hidden", NP[k].id[0], NP[k].slot[0], NP[k].chr[1] >= 0 ? PCH_TAB[NP[k].chr[1]].name : "-",
            NP[k].shown[1] ? "shown" : "hidden", NP[k].id[1], NP[k].slot[1]);
}
