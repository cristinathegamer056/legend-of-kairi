// (s269 warpcore, co-op item 4) the net side of "warp to a friend": NET_WARP (sora_net_proto.h, a NEW type: no NET_VERSION change,
// an older game drops it as a "bad" packet without a log line). Included by sora_net.h after sora_net_kills.h (it uses NET,
// net_send_to, LOG, NetLogRate). The game side (runtime/sora_warp.h, sora.c) sets NETW.hk at its first frame; with no hooks (the
// host tests of sora_net.h) an ASK gets a SPOT with flags 0 (= "not on MainField": the warper refuses) and a NOTE is only counted.
//   warper  net_warp_ask(k): ASK to slot k (re-sent every WA_RESEND_MS over UDP, once in a Melty room) until the SPOT with the same
//           nonce comes back (hk.on_spot) or WA_TIMEOUT_MS passes (hk.on_spot with NULL = no answer). net_warp_note(k, nonce)
//           once the game took the warp call.
//   friend  an ASK addressed to us -> hk.spot_fill (his feet + on-ground flag + gdt PlayerSavePos / angle, read on the game thread:
//           net_recv_all runs inside net_frame, inside the mod frame) -> SPOT back to that slot only; a NOTE -> hk.on_note(k).
// Every packet goes to ONE slot (net_send_to: his address, or his room peer in Melty). "set warplog 1" = one [warp] line per packet.
#pragma once
#include "sora_warp_core.h"

typedef struct {
    bool (*spot_fill)(NetWarpMsg *m);              // friend: flags / live / save of THIS game (false = nothing to say: flags 0)
    void (*on_spot)(int k, const NetWarpMsg *m);   // warper: the answer to our ASK (m NULL = no answer in time)
    void (*on_note)(int k);                        // friend: slot k warped to us
} NetWarpHooks;

static bool g_warp_log;  // live "set warplog 1" (parsed in sora_warp.h)
static bool g_warp_net = true;  // live "set warpnet 0": no answer to an ASK, no note shown (A/B; default on)

static struct {
    uint16_t seq;
    bool seeded;
    NetWarpHooks hk;
    bool asking;
    int k;
    uint8_t player;  // his player id when we asked (a slot that changed hands since = not him)
    uint32_t nonce;
    uint64_t t0, next;
    struct { uint32_t ask_tx, ask_rx, spot_tx, spot_rx, note_tx, note_rx, bad, other, late, off; } st;
    NetLogRate lr;
} NETW;

static uint32_t net_warp_nonce(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    uint64_t x = (uint64_t)t.QuadPart * 0x9E3779B97F4A7C15ull ^ GetTickCount64();
    x ^= x >> 31;
    uint32_t n = (uint32_t)x ^ (uint32_t)(x >> 32);
    return n ? n : 1;
}

static bool net_warp_slot_ok(int k) {
    return NET.on && k >= 0 && k < NET_MAX_PEERS && NET.slot[k].used && NET.slot[k].p.st != PEER_NONE && (NET.slot[k].ip || NET.melty);
}

static bool net_warp_send(int k, const NetWarpMsg *m) {
    if (!net_warp_slot_ok(k)) return false;
    if (!NETW.seeded) NETW.seeded = true, NETW.seq = (uint16_t)(GetTickCount64() * 2654435761u >> 7);
    uint8_t b[NET_WARP_SPOT_LEN];
    int n = net_pack_warp(NET.me, ++NETW.seq, m, b, (int)sizeof b);
    if (n <= 0) return false;
    net_send_to(NET.slot[k].ip, NET.slot[k].port, b, n);
    if (g_warp_log) LOG("[warp] net: %s to slot %d \"%s\" (nonce %08x)\n", m->op == NWO_ASK ? "ASK" : m->op == NWO_SPOT ? "SPOT" : "NOTE", k + 1,
                        NET.slot[k].p.name, (unsigned)m->nonce);
    return true;
}

// warper: start asking slot k where he is. false = no such friend now (net off, empty slot)
static bool net_warp_ask(int k, uint64_t now) {
    if (!net_warp_slot_ok(k)) return false;
    NETW.asking = true, NETW.k = k, NETW.player = NET.slot[k].p.player, NETW.nonce = net_warp_nonce();
    NETW.t0 = now ? now : 1, NETW.next = 0;
    return true;
}
static void net_warp_cancel(void) { NETW.asking = false; }

// warper: every frame while asking (sora_warp.h warp_tick)
static void net_warp_ask_tick(uint64_t now) {
    if (!NETW.asking) return;
    if (!net_warp_slot_ok(NETW.k) || NET.slot[NETW.k].p.player != NETW.player) {
        NETW.asking = false;
        if (NETW.hk.on_spot) NETW.hk.on_spot(NETW.k, NULL);
        return;
    }
    int a = wa_step(now, NETW.t0, &NETW.next, NET.melty);
    if (a == WAA_GIVE_UP) {
        NETW.asking = false;
        if (NETW.hk.on_spot) NETW.hk.on_spot(NETW.k, NULL);
    } else if (a == WAA_SEND) {
        NetWarpMsg m = {.op = NWO_ASK, .target = NETW.player, .nonce = NETW.nonce};
        if (net_warp_send(NETW.k, &m)) NETW.st.ask_tx++;
    }
}

// warper: "I warped to you" (once, after the warp call)
static void net_warp_note(int k, uint32_t nonce) {
    if (!net_warp_slot_ok(k)) return;
    NetWarpMsg m = {.op = NWO_NOTE, .target = NET.slot[k].p.player, .nonce = nonce ? nonce : 1};
    if (net_warp_send(k, &m)) NETW.st.note_tx++;
}

// a NET_WARP packet from slot k (header already checked by net_unpack_ex)
static void net_warp_recv(int k, const uint8_t *buf, int n, uint64_t now) {
    NetWarpMsg m;
    uint8_t pl = 0;
    uint16_t sq = 0;
    if (!net_unpack_warp(buf, n, &pl, &sq, &m)) { NETW.st.bad++; return; }
    if (m.target != NET.me) { NETW.st.other++; return; }  // (for another player: never ours to answer)
    bool say = g_warp_log && net_lograte_ok(&NETW.lr, now, NULL);
    if (m.op == NWO_ASK) {
        NETW.st.ask_rx++;
        if (!g_warp_net) { NETW.st.off++; return; }
        NetWarpMsg r = {.op = NWO_SPOT, .target = pl, .nonce = m.nonce};
        if (!(NETW.hk.spot_fill && NETW.hk.spot_fill(&r))) r.flags = 0;
        if (net_warp_send(k, &r)) NETW.st.spot_tx++;
        if (say) LOG("[warp] net: slot %d \"%s\" asked where we are: flags %#x, feet %.2f %.2f %.2f (%.0f deg), save %.2f %.2f %.2f (%.0f deg)\n", k + 1,
                     NET.slot[k].p.name, r.flags, r.live[0], r.live[1], r.live[2], r.live_deg, r.save[0], r.save[1], r.save[2], r.save_deg);
    } else if (m.op == NWO_SPOT) {
        NETW.st.spot_rx++;
        if (!NETW.asking || k != NETW.k || m.nonce != NETW.nonce) { NETW.st.late++; return; }  // (an answer to an older ask, or not asked)
        NETW.asking = false;
        if (say) LOG("[warp] net: slot %d \"%s\" answered in %llu ms: flags %#x\n", k + 1, NET.slot[k].p.name, (unsigned long long)(now - NETW.t0), m.flags);
        if (NETW.hk.on_spot) NETW.hk.on_spot(k, &m);
    } else if (m.op == NWO_NOTE) {
        NETW.st.note_rx++;
        if (!g_warp_net) { NETW.st.off++; return; }
        if (say) LOG("[warp] net: slot %d \"%s\" warped to us\n", k + 1, NET.slot[k].p.name);
        if (NETW.hk.on_note) NETW.hk.on_note(k);
    }
}

static void net_warp_diag(void) {
    LOG("[net] warp: ask tx %u rx %u, spot tx %u rx %u (late %u), note tx %u rx %u, bad %u, for others %u, off %u%s\n", NETW.st.ask_tx, NETW.st.ask_rx,
        NETW.st.spot_tx, NETW.st.spot_rx, NETW.st.late, NETW.st.note_tx, NETW.st.note_rx, NETW.st.bad, NETW.st.other, NETW.st.off,
        NETW.asking ? " (asking now)" : "");
}
