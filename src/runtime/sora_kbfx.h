// sora_kbfx.h (s89 kbfx): each Keyblade's own KH2 swing ribbon + hit sparks. Notes: recon/S89_HELPER_KBFX.md.
//
// KH2 gives every Keyblade its own effect set, obj/W_EX010_<xx>.a.us (the model's name: 03system.bin "went" row ->
// 00objentry; PHH mined/keyblades.kh.json): element 1 = the combo hit spark, 2 = the finisher spark, 3 = the guard /
// wall spark (as w_ex010.1 / .2 / .3 for the Kingdom Key, recon/S35_HELPER_X_FX.md), element 10 = the swing ribbon
// (texture + blend + colour; KH2 shapes it from the blade's motion). The fx bake (tools_s89/kbfx/fxbake.py, folded into
// tools/fxbake.py at merge) packs them as "kb<xx>_hit" / "kb<xx>_hit_finish" / "kb<xx>_hit_guard" / "kb<xx>_trail";
// the Kingdom Key keeps "hit" / "hit_finish" / "hit_guard" and adds "kb00_trail".
// Who draws what: blade 0 = KB_MAIN (base Sora, Wisdom, Valor's right hand), blade 1 = KB_VALOR (Valor's left hand).
// An id not in KBFX_TAB (a Keyblade added later) or a set missing from fx.bin = the Kingdom Key's set.
// The ribbon's look needs the s89 exe (sora_fx_trail_ex, GetProcAddress); an older exe = the s35 white ribbon, main
// blade only (the hit sparks work with the old exe: they are plain fx.bin elements).
// Live: "set kbfx 0" = the old single set (Kingdom Key sparks + white ribbon, no left-blade ribbon); "set kbfxid <id>"
// = force that Keyblade's set on both blades (e.g. 0x2b Oblivion; 0 = the equipped ones); "set kbfxflip 1" = the
// ribbon texture's v 0 at the grip (GUESS default: at the tip). (s216) "set kbown 0" = our two Keyblades draw their s214
// placeholder sets again, "set kbse 0" = their hits play the normal KH2 hit (both default 1).
// (s216 kbfx) The mod's own Keyblades (Sealing Blade 0x7e01, Guardian Core 0x7e02) have OWN sets "s1" / "g1" (tools/sorabuild/fx/
// kbownfx.py: shaped from Fenrir's / Photon Debugger's sparks, our colours + shapes; our ribbon textures; Sealing Blade's beam
// "kbs1_beam" / "kbs1_beam_hit") and OWN hit sounds ("kbs1_hit" / "kbg1_hit" in sfx.bin, synthesised by ours/kb/kbsynth.py).
// A row's `alt` = the placeholder set used when fx.bin has not got the own set (an older pack) or with live "set kbown 0"; `se` =
// the sfx.bin name of its own hit sound (missing = the normal KH2 hit, sora_sfx_hit; live "set kbse 0" = the normal one too).
// KH2 fact (s216, the KH2 extract): every Keyblade's W_EX010_<xx>.a.us names its OWN sound bank (se500 = Kingdom Key ... se527) and
// those banks hold different streams at the hit indexes 0..8, so vanilla KH2 gives each Keyblade its own hit sounds; the mod plays
// se500's (the Kingdom Key's) for every KH2 Keyblade (sora_sfx_hit), only these two get their own (recon/s216/S216_HELPER_FX.md).
// (s247 kbfx2) The user's two new Keyblades, Silent Princess 0x7e03 "p1" and Calamity's Gaze 0x7e04 "c1", get OWN sets too
// (kbownfx.py part s247, all procedural: a blooming flower / a Malice splash + the slit eye; our ribbons; sounds "kbp1_hit" /
// "kbc1_hit"), placeholders Fenrir "m0" / Oblivion "20". NEW: while a blade's ribbon is drawn, a set with "kb<set>_shed_<k>"
// elements in fx.bin drops them off the blade (kbfx_shed: petals / embers), at a rate per SECOND (not per frame), capped; only
// these two sets have them. Live "set kbshed 0" = none, "set kbshedrate <n>" = bits per second per blade (default 16, 0..60).
// recon/s247/S247_HELPER_KBFX.md.
// (s247 kbse) Live "set kbhitse 1" = each KH2 Keyblade plays its OWN KH2 hit sounds (its bank se501..se527, as the KH2 exe does:
// kbhitse_id below); default 0 = the Kingdom Key's se500 for all, as before. recon/s247/S247_HELPER_KBSE.md.
// (s249 kbswing) Live "set kbswingse 1" = each hand whooshes with its own Keyblade's KH2 swing sounds, "set kbwallse 1" = the wall
// clank of the blade that hit is its Keyblade's own (kbsw_motion / kbwall_play below; both default 0). recon/s249/S249_HELPER_KBSWING.md.
// Needs (sora.c before the include): KB (sora_levels.h), sora_fx.h, sora_sfx.h, LOG, windows.h, IN_ANTI, IN_MICKEY.
#pragma once

typedef struct {
    uint16_t id;       // KH2 item id (03system.bin)
    char set[3];       // W_EX010_<set>.a.us; "" = W_EX010 (the Kingdom Key)
    const char *name;  // log name (sora_kb.h KB_TAB may not have the row yet)
    char alt[3];       // (s216) the placeholder set when fx.bin lacks `set` or with "set kbown 0" ("" = none: KH2's rows)
    const char *se;    // (s216) sfx.bin name of the Keyblade's own hit sound (NULL = the normal KH2 hit)
} KbfxRow;
static const KbfxRow KBFX_TAB[] = {
    {0x029, "", "Kingdom Key"},     {0x051, "", "Kingdom Key (item 81)"}, {0x02a, "10", "Oathkeeper"},
    {0x02b, "20", "Oblivion"},      {0x1e0, "30", "Star Seeker"},       {0x1e1, "40", "Hidden Dragon"},
    {0x1e4, "50", "Hero's Crest"},  {0x1e5, "60", "Monochrome"},        {0x1e6, "70", "Follow the Wind"},
    {0x1e7, "80", "Circle of Life"}, {0x1e8, "90", "Photon Debugger"},  {0x1e9, "a0", "Gull Wing"},
    {0x1ea, "b0", "Rumbling Rose"}, {0x1eb, "c0", "Guardian Soul"},     {0x1ec, "d0", "Wishing Lamp"},
    {0x1ed, "e0", "Decisive Pumpkin"}, {0x1ee, "f0", "Sleeping Lion"},  {0x1ef, "g0", "Sweet Memories"},
    {0x1f0, "h0", "Mysterious Abyss"}, {0x1f1, "j0", "Fatal Crest"},    {0x1f2, "k0", "Bond of Flame"},
    {0x1f3, "m0", "Fenrir"},        {0x1f4, "n0", "Ultima Weapon"},     {0x21f, "p0", "Two Become One"},
    {0x220, "r0", "Winner's Proof"}, {0x180, "w0", "Struggle Sword"},   {0x1f5, "v0", "Struggle Wand"},
    {0x1f6, "u0", "Struggle Hammer"}, {0x02c, "y0", "Detection Saber"}, {0x02d, "z0", "Frontier of Ultima"},
    // (s216 kbfx) our own Keyblades: their own sets (kbownfx.py) + own hit sounds; without them in the packs = the s214
    // placeholders: Sealing Blade = Fenrir's set "m0", Guardian Core = Photon Debugger's "90" (recon/S89_HELPER_KBFX.md)
    {0x7e01, "s1", "Sealing Blade", "m0", "kbs1_hit"}, {0x7e02, "g1", "Guardian Core", "90", "kbg1_hit"},
    // (s247 kbfx2) the user's two new Keyblades; placeholders: Silent Princess = Fenrir "m0" (pale blue-white), Calamity's Gaze =
    // Oblivion "20" (purple / black)
    {0x7e03, "p1", "Silent Princess", "m0", "kbp1_hit"}, {0x7e04, "c1", "Calamity's Gaze", "20", "kbc1_hit"},
};
#define KBFX_N ((int)(sizeof KBFX_TAB / sizeof KBFX_TAB[0]))

static int g_kbfx = 1;     // live "set kbfx": 0 = the old single set
static int g_kbfx_id;      // live "set kbfxid": force this item id's set on both blades (0 = the equipped Keyblades)
static int g_kbfx_vflip;   // live "set kbfxflip": the ribbon texture's v 0 at the grip
static int g_kbfx_blade;   // the blade whose hit is being drawn (blade_hits: 0 main, 1 Valor's left)
static int g_kbown = 1;    // (s216) live "set kbown": 0 = our Keyblades draw their placeholder sets (`alt`)
static int g_kbse = 1;     // (s216) live "set kbse": 0 = our Keyblades' hits play the normal KH2 hit sound
static int g_kbsecg;       // (s250) live "set kbsecg": 1 = Calamity's Gaze hits play its own sound "kbc1_hit" again; default 0 = the normal
                           // KH2 hit (the user: its own hit sound "could get pretty annoying"; a new sound for it is open)
static int g_kbshed = 1;   // (s247) live "set kbshed": 0 = no petals / embers off the blade
static int g_kbshed_rate = 16;  // (s247) live "set kbshedrate": shed bits per second per blade (0..60)
static int g_kbhitse = 1;  // (s247 kbse) live "set kbhitse": 1 = each KH2 Keyblade plays its own KH2 hit sounds (kbhitse_id; s249: default ON, the user verified it; 0 = the Kingdom Key sounds)
static int g_kbswingse = 1; // (s249 kbswing) live "set kbswingse": 1 = each hand whooshes with its own Keyblade's KH2 bank (kbsw_motion; default 1 since s250, user verified)
static int g_kbwallse = 1; // (s249 kbswing) live "set kbwallse": 1 = the wall clank from the blade's own Keyblade's bank (kbwall_play; default 1 since s250, user verified)

static int kbfx_row(int id) {
    for (int i = 0; i < KBFX_N; i++)
        if (KBFX_TAB[i].id == id) return i;
    return -1;
}
static int kbfx_item(int blade) { return g_kbfx_id ? g_kbfx_id : blade ? kb_sub_id(KB.kb, g_fmots_on ? g_fform : 0) : KB.kb[KB_MAIN]; }  // (s91 mfkb: blade 1 = the form's own slot)

// the fx.bin element ids of one row (resolved once; -1 = not in fx.bin -> the Kingdom Key's)
#define KBSHED_VAR 3  // (s247) "kb<set>_shed_0..2": the variants a set may have
#define KBSHED_CAP 12  // (s247 kbfx_shed) shed bits alive at most (all blades)
#define KBSHED_FREE 8  // never shed while fewer of sora_fx.c's 32 element slots are free
#define KBSHED_LIFE_MS 1000  // the longest shed element is 48 KH frames (0.8 s) + a margin; a slot reused later is not counted
typedef struct { int tried, hit, fin, guard, trail, nshed, shed[KBSHED_VAR]; } KbfxIds;
static KbfxIds g_kbfx_ids[KBFX_N], g_kbfx_ids_alt[KBFX_N];  // (s216) the row's set / its placeholder set
static int g_kbfx_pack = -1;  // fx.bin has the per-Keyblade sets (its "kb00_trail"): 1 yes, 0 no, -1 not asked
static const KbfxIds *kbfx_ids_set(int row, bool alt) {
    if (row < 0) row = 0;
    if (g_kbfx_pack < 0) {
        if (!sora_fx_ready()) return NULL;  // (fx.bin not loaded yet: ask again later)
        g_kbfx_pack = sora_fx_id("kb00_trail") >= 0;
        if (!g_kbfx_pack) LOG("[sora] kbfx: fx.bin has no per-Keyblade sets (bake tools_s89/kbfx/fxbake.py -> fx_s89_kbfx.bin, "
                              "or the combined rebake): every Keyblade draws the Kingdom Key's sparks + the white ribbon\n");
    }
    KbfxIds *d = alt ? &g_kbfx_ids_alt[row] : &g_kbfx_ids[row];
    if (!d->tried) {
        d->tried = 1, d->hit = d->fin = d->guard = d->trail = -1, d->nshed = 0;
        if (g_kbfx_pack) {
            const char *s = alt ? KBFX_TAB[row].alt : KBFX_TAB[row].set;
            char b[32];
            if (s[0]) {
                snprintf(b, sizeof b, "kb%s_hit", s), d->hit = sora_fx_id(b);
                snprintf(b, sizeof b, "kb%s_hit_finish", s), d->fin = sora_fx_id(b);
                snprintf(b, sizeof b, "kb%s_hit_guard", s), d->guard = sora_fx_id(b);
                if (s[1] != '0')  // (s247: only our sets can have shed bits; KH2's are never asked: sora_fx_id logs unknown names)
                    for (int k = 0; k < KBSHED_VAR; k++) {
                        snprintf(b, sizeof b, "kb%s_shed_%d", s, k);
                        int id = sora_fx_id(b);
                        if (id >= 0) d->shed[d->nshed++] = id;
                    }
            }
            snprintf(b, sizeof b, "kb%s_trail", s[0] ? s : "00"), d->trail = sora_fx_id(b);
        }
    }
    return d;
}
// (s216) the set a row draws: its own, or its placeholder (`alt`) when fx.bin lacks the own set or with "set kbown 0".
// *set (may be NULL) = that set's name ("" = the Kingdom Key's W_EX010).
static const KbfxIds *kbfx_use(int row, const char **set) {
    if (row < 0) row = 0;
    const KbfxIds *d = kbfx_ids_set(row, false);
    const char *s = KBFX_TAB[row].set;
    if (KBFX_TAB[row].alt[0] && (!g_kbown || !d || d->hit < 0)) d = kbfx_ids_set(row, true), s = KBFX_TAB[row].alt;
    if (set) *set = s;
    return d;
}

// The element name of a hit spark for the blade being drawn (g_kbfx_blade): kind = "hit" / "hit_finish" / "hit_guard"
// -> "kb<xx>_<kind>" when fx.bin has it, else kind itself (the Kingdom Key's).
static const char *kbfx_name(const char *kind) {
    { const char *mk = mk_kbfx_name(kind); if (mk) return mk; }  // (s110m mickeyfx: W_EX200's own sparks)
    if (!g_kbfx || IN_ANTI) return kind;  // (s89 trial: Anti has no Keyblade - its claws keep the plain sparks)
    int row = kbfx_row(kbfx_item(g_kbfx_blade));
    if (row <= 0 || !KBFX_TAB[row].set[0]) return kind;
    const char *s = "";
    const KbfxIds *d = kbfx_use(row, &s);
    int id = !d ? -1 : !strcmp(kind, "hit") ? d->hit : !strcmp(kind, "hit_finish") ? d->fin : !strcmp(kind, "hit_guard") ? d->guard : -1;
    if (id < 0) return kind;
    static char b[4][32];
    static int k;
    k = (k + 1) & 3;
    snprintf(b[k], sizeof b[k], "kb%s_%s", s, kind);
    return b[k];
}

// The swing ribbon of blade `blade` (blade_hits_one, every frame of an attack): the Keyblade's own look on the s89 exe.
static void kbfx_trail(int blade, const float grip[3], const float tip[3], bool on) {
    typedef void (*TrailEx)(int, const float *, const float *, bool, int, int);
    static TrailEx ex;
    static int tried;
    if (!tried) {
        tried = 1, ex = (TrailEx)(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_fx_trail_ex");
        if (!ex) LOG("[sora] kbfx: exe has no sora_fx_trail_ex (before s89): the white ribbon on the main blade only\n");
    }
    if (!ex || !g_kbfx) {
        if (!blade) sora_fx_trail(grip, tip, on);
        return;
    }
    const KbfxIds *d = kbfx_use(kbfx_row(kbfx_item(blade)), NULL);
    ex(blade, grip, tip, on, d ? d->trail : -1, g_kbfx_vflip ? 1 : 0);
}

static void kbfx_log_id(const char *who, int id) {
    int row = kbfx_row(id);
    const char *s = "";
    const KbfxIds *d = kbfx_use(row, &s);
    if (row < 0) s = "";
    bool own = row > 0 && s[0] && d && d->hit >= 0;
    bool ours = s[0] && s[1] != '0';  // (s216) our sets ("s1", "g1"); KH2's all end in '0'
    bool placeholder = row > 0 && KBFX_TAB[row].alt[0] && strcmp(s, KBFX_TAB[row].set);
    LOG("[sora] kbfx: %s = %s (0x%x)%s -> %s%s%s set%s: sparks %s, ribbon %s%s\n", who,
        row < 0 ? kb_name(id) : KBFX_TAB[row].name, id, row < 0 ? " (no kbfx row: the Kingdom Key's)" : "", ours ? "our own " : "W_EX010",
        ours || !s[0] ? "" : "_", s, placeholder ? (g_kbown ? " (PLACEHOLDER: fx.bin has no own set)" : " (PLACEHOLDER: set kbown 0)") : "",
        own ? "its own" : "the Kingdom Key's", d && d->trail >= 0 ? "its own" : "the white gradient",
        d && d->nshed > 0 ? (g_kbshed && g_kbshed_rate > 0 ? ", sheds bits off the blade" : ", shed bits OFF") : "");
}

// once per frame (sora_mod_frame_body, after kb_send): logs the active sets when the equipped Keyblades / live keys change
static void kbfx_tick(void) {
    static int last[14] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    int now[14] = {KB.kb[KB_MAIN], KB.kb[KB_VALOR], g_kbfx, g_kbfx_id, g_kbfx_vflip, KB.kb[KB_MASTER] * 4 + g_mfkb, KB.kb[KB_FINAL],
                   g_kbown, g_kbse, g_kbshed, g_kbshed_rate, g_kbhitse, g_kbswingse, g_kbwallse};  // (s91 mfkb: + Master / Final; s216: + kbown / kbse; s247: + kbshed, kbhitse; s249: + kbswingse, kbwallse)
    if (!memcmp(now, last, sizeof now)) return;
    if (g_kbfx_pack < 0 && !sora_fx_ready()) return;
    memcpy(last, now, sizeof now);
    if (!g_kbfx) {
        LOG("[sora] kbfx: off (set kbfx 0): Kingdom Key sparks + the white ribbon for every Keyblade\n");
        return;
    }
    if (g_kbfx_id) LOG("[sora] kbfx: forced to item 0x%x on both blades (set kbfxid 0 = the equipped ones)\n", g_kbfx_id);
    kbfx_log_id("main", g_kbfx_id ? g_kbfx_id : KB.kb[KB_MAIN]);
    kbfx_log_id("valor left", g_kbfx_id ? g_kbfx_id : KB.kb[KB_VALOR]);
    kbfx_log_id("master 2nd", g_kbfx_id ? g_kbfx_id : kb_sub_id(KB.kb, 3));  // (s91 mfkb)
    kbfx_log_id("final 2nd", g_kbfx_id ? g_kbfx_id : kb_sub_id(KB.kb, 4));
    if (g_kbfx_vflip) LOG("[sora] kbfx: ribbon texture flipped (v 0 at the grip)\n");
    if (!g_kbse) LOG("[sora] kbfx: own hit sounds off (set kbse 0): every Keyblade plays the normal KH2 hit\n");
    LOG(g_kbhitse ? "[sora] kbfx: KH2 Keyblades play their OWN KH2 hit sounds (set kbhitse 1: each blade its Keyblade's bank)\n"
                  : "[sora] kbfx: KH2 Keyblades play the Kingdom Key's hit sounds (set kbhitse 0, the default; 1 = each its own)\n");
    LOG(g_kbswingse ? "[sora] kbfx: KH2 Keyblades whoosh with their OWN KH2 swing sounds (set kbswingse 1: each hand its Keyblade's bank)\n"
                    : "[sora] kbfx: every Keyblade whooshes as the Kingdom Key (set kbswingse 0, the default; 1 = each its own)\n");
    LOG(g_kbwallse ? "[sora] kbfx: KH2 Keyblades clank off walls with their OWN KH2 sound (set kbwallse 1: the blade that hit, its bank's cue 10)\n"
                   : "[sora] kbfx: every Keyblade clanks off walls as the Kingdom Key (set kbwallse 0, the default; 1 = each its own)\n");
    if (g_kbshed && g_kbshed_rate > 0)
        LOG("[sora] kbfx: shed bits on (Silent Princess petals / Calamity's Gaze embers): %d per second per blade, at most %d alive\n",
            g_kbshed_rate > 60 ? 60 : g_kbshed_rate, KBSHED_CAP);
    else LOG("[sora] kbfx: shed bits OFF (set kbshed 0 / kbshedrate 0)\n");
}

// (s216) The hit sound of the blade being drawn (g_kbfx_blade, like kbfx_name): a Keyblade with its own sound in sfx.bin
// (KBFX_TAB.se) plays that at pos; anything else (KH2's Keyblades, Mickey, Anti, kbfx / kbse off, an sfx.bin without it)
// = sora_sfx_hit(atkp), the KH2 hit of that attack row as before.
static int g_kbse_sfx[KBFX_N];  // 0 = not asked yet, -1 = not in sfx.bin, else sound id + 1
static int kbse_id(void) {
    if (!g_kbse || !g_kbfx || IN_ANTI || IN_MICKEY) return -1;
    int row = kbfx_row(kbfx_item(g_kbfx_blade));
    if (row < 0 || !KBFX_TAB[row].se) return -1;
    if (KBFX_TAB[row].id == 0x7e04 && !g_kbsecg) return -1;  // (s250) Calamity's Gaze: the normal KH2 hit for now
    if (!g_kbse_sfx[row]) {
        if (!sora_sfx_ready()) return -1;  // (not loaded yet: sora_sfx_hit loads it; ask again next hit)
        int id = sora_sfx_id(KBFX_TAB[row].se);
        g_kbse_sfx[row] = id >= 0 ? id + 1 : -1;
        if (id < 0) LOG("[sora] kbfx: sfx.bin has no \"%s\" (an older pack): %s hits play the normal KH2 hit\n", KBFX_TAB[row].se, KBFX_TAB[row].name);
        else LOG("[sora] kbfx: %s hits play its own sound \"%s\" (sfx #%d)\n", KBFX_TAB[row].name, KBFX_TAB[row].se, id);
    }
    return g_kbse_sfx[row] > 0 ? g_kbse_sfx[row] - 1 : -1;
}
// (s247 kbse) Each KH2 Keyblade's OWN KH2 hit sounds (live "set kbhitse 1"; default 0 = the Kingdom Key's se500 for all, as before).
// KH2 FM exe (recon/s247/S247_HELPER_KBSE.md): the actor-hit path FUN_1403d12c0 plays the atkp row's hit SE through
// FUN_1403dadf0(the attack's SOURCE object, se, ...): a bank-500 id (500000..500999) is re-banked to that object's own sound bank
// (a weapon object, +0x6C8 bit 6: its own; Sora's body, bit 2: his slot-0 weapon, +0xD60). A weapon's attack windows run on the
// weapon object (FUN_1403dafe0 -> weapon vtable +0x98 = FUN_1403ef6b0 passes the weapon as the source), one weapon object per hand
// (+0xD60 + 8 * slot, made from the held item by FUN_1403ef750). So: the blade that hit plays ITS Keyblade's bank, the second blade
// (Valor's left / Master's / Final's 2nd) its own. The bank = the one obj/W_EX010_<xx>.a.us loads (type-31 entry "se/obj/se5NN").
// sfx.bin (tools/sorabuild/fx/kbhitse.py) holds "kbh<set>_<atkp>" = that set's sound for the atkp row's se500 cue, + marker
// "kbhitse_v1". Anything else = today's behaviour exactly: the Kingdom Key ("" set: se500 is its own), our Keyblades (their `se`
// first), Mickey, Anti, kbfx 0, an sfx.bin without the marker, an atkp row the pack has no name for (not a se500 hit row).
#define KBHITSE_MARK "kbhitse_v1"
#define KBHITSE_CACHE 256  // (row, atkp) pairs remembered (a full cache only means asking sfx.bin again)
static int g_kbhitse_pack = -1;  // the pack has the per-Keyblade hit sounds: 1 yes, 0 no, -1 not asked
static struct { uint16_t row, atkp; int16_t id; } g_kbhitse_c[KBHITSE_CACHE];
static int g_kbhitse_n;
static uint8_t g_kbhitse_used[KBFX_N];  // its first own-bank hit was logged
static int kbhitse_id(int atkp_row) {
    if (!g_kbhitse || !g_kbfx || IN_ANTI || IN_MICKEY || atkp_row < 0 || atkp_row > 0xffff) return -1;
    int row = kbfx_row(kbfx_item(g_kbfx_blade));
    if (row <= 0) return -1;
    const char *s = KBFX_TAB[row].set;
    if (!s[0] || s[1] != '0' || KBFX_TAB[row].se) return -1;  // (the Kingdom Key: se500 is its own bank; ours: their own sound)
    if (g_kbhitse_pack < 0) {
        if (!sora_sfx_ready()) return -1;  // (not loaded yet: ask again next hit)
        g_kbhitse_pack = sora_sfx_id(KBHITSE_MARK) >= 0;
        if (!g_kbhitse_pack)
            LOG("[sora] kbfx: sfx.bin has no per-Keyblade KH2 hit sounds (no \"%s\": an older pack): every KH2 Keyblade plays the "
                "Kingdom Key's hits\n", KBHITSE_MARK);
    }
    if (!g_kbhitse_pack) return -1;
    for (int i = 0; i < g_kbhitse_n; i++)
        if (g_kbhitse_c[i].row == row && g_kbhitse_c[i].atkp == atkp_row) return g_kbhitse_c[i].id;
    char b[32];
    snprintf(b, sizeof b, "kbh%s_%d", s, atkp_row);
    int id = sora_sfx_id(b);
    if (id < 0) id = -1;
    if (g_kbhitse_n < KBHITSE_CACHE)
        g_kbhitse_c[g_kbhitse_n].row = (uint16_t)row, g_kbhitse_c[g_kbhitse_n].atkp = (uint16_t)atkp_row,
        g_kbhitse_c[g_kbhitse_n++].id = (int16_t)id;
    if (id >= 0 && !g_kbhitse_used[row]) {
        g_kbhitse_used[row] = 1;
        LOG("[sora] kbfx: %s hits play its own KH2 hit sounds (W_EX010_%s's bank; atkp %d -> \"%s\" = sfx #%d)\n",
            KBFX_TAB[row].name, s, atkp_row, b, id);
    }
    return id;
}
static int kbse_hit(int atkp_row, float gain, const float pos[3]) {
    int id = kbse_id();
    if (id < 0) id = kbhitse_id(atkp_row);  // (s247 kbse)
    return id >= 0 ? sora_sfx_play_at(id, gain, pos) : sora_sfx_hit(atkp_row, gain, pos);
}

// (s249 kbswing) Each KH2 Keyblade's OWN swing whooshes (live "set kbswingse 1") and OWN wall clank ("set kbwallse 1"); both
// default 0 = the Kingdom Key's se500 sounds for every Keyblade, as before. KH2 FM exe (recon/s249/S249_HELPER_KBSWING.md):
//   whoosh  an mset frame trigger 8 (play_sound bank, cue) = FUN_14040dd70 case 8 -> FUN_1403c8280(the motion's object,
//           bank * 1000 + cue) -> FUN_1403dadf0: bank 500 -> the object's own bank. All of Sora's bank-500 play_sounds sit in
//           the WEAPON msets (W_EX010 / W_EX010_<form>_R / _L; none in P_EX100*), so each hand whooshes with its Keyblade's bank.
//   wall    FUN_1403d1960 (attack vs BG): se 500010 through FUN_1403dadf0(the attack's source = the weapon) = its bank, cue 10.
// sfx.bin (tools/sorabuild/fx/kbswingse.py): the trigger rows of every Sora / form slot s < 0x10000 again at KBSW_ALT + s (all
// but the whooshes) and at KBSW_ALT + ((1 + 2k + hand) << 16) + s (only that hand's whooshes, Keyblade set k's bank; k = the
// set's first character in base 36, the Kingdom Key 0 = its own se500 rows); names "kbswingse_v1", "kbsw<set>" (that set is
// packed), "kbwallse_v1", "kbwl<set>" (its clank). kbsw_motion plays those three slots instead of s, which together are the old
// slot's rows with each hand's whooshes swapped. Hand 0 = the main blade (base Sora, Wisdom, Limit, Valor's right, Master's /
// Final's first), hand 1 = the second blade (Valor's left, Master's / Final's 2nd), as kbfx_item. Today's sounds exactly for:
// keys off, kbfx 0, Anti, Mickey / Roxas / the summons (their slots are >= 0x10000), the Kingdom Key, our own Keyblades (no KH2
// bank: Sealing Blade, Guardian Core, Silent Princess, Calamity's Gaze), a BotW weapon in Sora's hand, a Keyblade without a
// kbfx row, an sfx.bin without the markers, a set the pack lacks (that hand plays the Kingdom Key's rows).
// Live "set kbswlog 1" = one log line per own whoosh / clank played.
#define KBSW_ALT 0x1000000
#define KBSW_MARK "kbswingse_v1"
#define KBWL_MARK "kbwallse_v1"
static int g_kbswlog;     // (s249 kbswing) live "set kbswlog": 1 = one log line per own whoosh / clank played
static int g_kbsw_pack = -1, g_kbwl_pack = -1;  // the pack has the rows / names: 1 yes, 0 no, -1 not asked
static int8_t g_kbsw_set[KBFX_N];  // "kbsw<set>" in the pack: 0 not asked, 1 yes, -1 no
static int g_kbwl_sfx[KBFX_N];     // "kbwl<set>": 0 not asked, -1 not in the pack, else sound id + 1
static int g_kbwl_base = -2;       // "wall_hit" (the Kingdom Key's, today's), -2 = not asked
static uint8_t g_kbsw_used[KBFX_N], g_kbwl_used[KBFX_N];  // its first own whoosh / clank was logged
static int g_kbsw_last[2] = {-1, -1};  // (host tests) the hand slots the last kbsw_motion played: -1 = the plain slot

// the KH2 set row of blade `blade`'s Keyblade, or -1 (the Kingdom Key, ours, no row, a BotW weapon in hand 0)
static int kbsw_row(int blade, bool botw) {
    if (botw && blade == 0) return -1;
    int row = kbfx_row(kbfx_item(blade));
    if (row <= 0) return -1;
    const char *s = KBFX_TAB[row].set;
    return s[0] && s[1] == '0' && !KBFX_TAB[row].se ? row : -1;
}
static int kbsw_k(int row) {  // the set's slot index (base 36 of its first character)
    char c = KBFX_TAB[row].set[0];
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'z' ? c - 'a' + 10 : 0;
}
// the hand slot of blade `blade` for slot `slot` (its Keyblade's, else the Kingdom Key's k 0)
static int kbsw_hand_slot(int blade, int slot, bool botw, int *row_out) {
    int row = kbsw_row(blade, botw), k = 0;
    if (row > 0) {
        if (!g_kbsw_set[row]) {
            char b[16];
            snprintf(b, sizeof b, "kbsw%s", KBFX_TAB[row].set);
            g_kbsw_set[row] = sora_sfx_id(b) >= 0 ? 1 : -1;
            if (g_kbsw_set[row] < 0)
                LOG("[sora] kbfx: sfx.bin has no \"%s\": %s whooshes with the Kingdom Key's sounds\n", b, KBFX_TAB[row].name);
        }
        if (g_kbsw_set[row] > 0) k = kbsw_k(row);
        else row = -1;
    }
    if (row_out) *row_out = row;
    return KBSW_ALT + ((1 + 2 * k + blade) << 16) + slot;
}
// = sora_sfx_motion(slot, ...) for Sora's own motions (sora.c set_motion / play / the motion clock); botw = Sora holds a BotW
// weapon in the main hand (s128 wpn: its whooshes stay the Kingdom Key's)
static int kbsw_motion(int slot, float prev, float clock, const float pos[3], bool botw) {
    g_kbsw_last[0] = g_kbsw_last[1] = -1;
    if (!g_kbswingse || !g_kbfx || IN_ANTI || IN_MICKEY || slot < 0 || slot >= 0x10000) return sora_sfx_motion(slot, prev, clock, pos);
    if (g_kbsw_pack < 0) {
        if (!sora_sfx_ready()) return sora_sfx_motion(slot, prev, clock, pos);
        g_kbsw_pack = sora_sfx_id(KBSW_MARK) >= 0;
        if (!g_kbsw_pack)
            LOG("[sora] kbfx: sfx.bin has no per-Keyblade KH2 whooshes (no \"%s\": an older pack): every Keyblade whooshes as the "
                "Kingdom Key\n", KBSW_MARK);
    }
    if (!g_kbsw_pack) return sora_sfx_motion(slot, prev, clock, pos);
    int row[2], hs[2] = {kbsw_hand_slot(0, slot, botw, &row[0]), kbsw_hand_slot(1, slot, botw, &row[1])};
    int n = sora_sfx_motion(KBSW_ALT + slot, prev, clock, pos);  // (the slot's footsteps / voices / body SEs)
    for (int b = 0; b < 2; b++) {
        int w = sora_sfx_motion(hs[b], prev, clock, pos);
        g_kbsw_last[b] = hs[b];
        n += w;
        if (w > 0 && row[b] > 0 && !g_kbsw_used[row[b]]) {
            g_kbsw_used[row[b]] = 1;
            LOG("[sora] kbfx: %s (%s blade) whooshes with its own KH2 sounds (W_EX010_%s's bank; slot %d -> trigger slot 0x%x)\n",
                KBFX_TAB[row[b]].name, b ? "second" : "main", KBFX_TAB[row[b]].set, slot, hs[b]);
        }
        if (w > 0 && g_kbswlog)
            LOG("[sora] kbswlog: slot %d %s blade %s -> trigger slot 0x%x: %d whoosh(es) at frame %.1f\n", slot, b ? "second" : "main",
                row[b] > 0 ? KBFX_TAB[row[b]].name : "(the Kingdom Key's)", hs[b], w, clock);
    }
    return n;
}

// The wall clank (sora.c repel_try, KH2's se 500010) of the blade that met the wall (g_kbfx_blade): its own bank's cue 10 with
// "set kbwallse 1", else "wall_hit" (the Kingdom Key's, today's); an sfx.bin before s52 without "wall_hit" = the atkp hit as before.
static int kbwall_id(bool botw) {
    if (!g_kbwallse || !g_kbfx || IN_ANTI || IN_MICKEY) return -1;
    int row = kbsw_row(g_kbfx_blade, botw);
    if (row <= 0) return -1;
    if (g_kbwl_pack < 0) {
        if (!sora_sfx_ready()) return -1;
        g_kbwl_pack = sora_sfx_id(KBWL_MARK) >= 0;
        if (!g_kbwl_pack)
            LOG("[sora] kbfx: sfx.bin has no per-Keyblade KH2 wall clank (no \"%s\": an older pack): the Kingdom Key's for all\n",
                KBWL_MARK);
    }
    if (!g_kbwl_pack) return -1;
    if (!g_kbwl_sfx[row]) {
        char b[16];
        snprintf(b, sizeof b, "kbwl%s", KBFX_TAB[row].set);
        int id = sora_sfx_id(b);
        g_kbwl_sfx[row] = id >= 0 ? id + 1 : -1;
        if (id < 0) LOG("[sora] kbfx: sfx.bin has no \"%s\": %s clanks as the Kingdom Key\n", b, KBFX_TAB[row].name);
    }
    return g_kbwl_sfx[row] > 0 ? g_kbwl_sfx[row] - 1 : -1;
}
static int kbwall_play(int atkp_row, const float pos[3], bool botw) {
    int id = kbwall_id(botw);
    if (id >= 0) {
        int row = kbsw_row(g_kbfx_blade, botw);
        if (row > 0 && !g_kbwl_used[row]) {
            g_kbwl_used[row] = 1;
            LOG("[sora] kbfx: %s clanks off walls with its own KH2 sound (W_EX010_%s's bank cue 10 = sfx #%d)\n", KBFX_TAB[row].name,
                KBFX_TAB[row].set, id);
        }
        if (g_kbswlog) LOG("[sora] kbswlog: wall clank of the %s blade = sfx #%d\n", g_kbfx_blade ? "second" : "main", id);
        return sora_sfx_play_at(id, 1.0f, pos);
    }
    if (g_kbwl_base == -2) g_kbwl_base = sora_sfx_id("wall_hit");
    return g_kbwl_base >= 0 ? sora_sfx_play_at(g_kbwl_base, 1.0f, pos) : sora_sfx_hit(atkp_row, 1.0f, pos);
}

// (s247) Bits shed off the blade while its ribbon is drawn (blade_hits_one, right after kbfx_trail, every frame of an attack):
// one "kb<set>_shed_<k>" element (a few petals / embers that drift and fade by themselves, 0.6 - 0.8 s; unbound, so they stay
// where they were dropped) at a random point of the outer 2/3 of the blade, a random variant and yaw. The rate is per second of
// game time (kh = this frame's KH frames, 60 per second), so 86 or 144 fps drop the same number. Never more than KBSHED_CAP
// alive, and never while fewer than KBSHED_FREE of sora_fx.c's 32 element slots are free (the hit sparks come first: a full
// table evicts the oldest one-shot). Only a set with shed elements in fx.bin drops anything (ours: p1 / c1; not placeholders).
#ifndef KBSHED_NOW_MS
#define KBSHED_NOW_MS() ((uint64_t)GetTickCount64())
#endif
static struct { int h; uint64_t t; } g_kbshed_live[KBSHED_CAP];
static float g_kbshed_acc[2];
static uint32_t g_kbshed_seed = 0x2471u;
static float kbshed_rnd(void) {  // 0..1 (xorshift32: the look only)
    g_kbshed_seed ^= g_kbshed_seed << 13, g_kbshed_seed ^= g_kbshed_seed >> 17, g_kbshed_seed ^= g_kbshed_seed << 5;
    return (float)(g_kbshed_seed >> 8) / 16777216.0f;
}
static int kbshed_count(uint64_t now) {  // our bits still alive (by their slot AND their age; stale entries are freed)
    int n = 0;
    for (int i = 0; i < KBSHED_CAP; i++)
        if (g_kbshed_live[i].t && now - g_kbshed_live[i].t < KBSHED_LIFE_MS && sora_fx_alive(g_kbshed_live[i].h)) n++;
        else g_kbshed_live[i].t = 0;
    return n;
}
// -> how many bits it dropped this frame (for the host tests)
static int kbfx_shed(int blade, const float grip[3], const float tip[3], bool on, float to_u, float kh) {
    if (blade < 0 || blade > 1) return 0;
    if (!on || !g_kbshed || g_kbshed_rate <= 0 || !g_kbfx || IN_ANTI || IN_MICKEY) {
        g_kbshed_acc[blade] = 0.75f;  // (the next window's first bit comes after a quarter of the interval)
        return 0;
    }
    int row = kbfx_row(kbfx_item(blade));
    if (row <= 0) return 0;
    const KbfxIds *d = kbfx_use(row, NULL);
    if (!d || d->nshed <= 0 || !(kh > 0)) return 0;
    if (kh > 8) kh = 8;  // (a hitch: no burst)
    int rate = g_kbshed_rate > 60 ? 60 : g_kbshed_rate;
    g_kbshed_acc[blade] += kh * (float)rate / 60.0f;
    int done = 0;
    while (g_kbshed_acc[blade] >= 1.0f && done < 2) {
        g_kbshed_acc[blade] -= 1.0f;
        uint64_t now = KBSHED_NOW_MS();
        if (kbshed_count(now) >= KBSHED_CAP || sora_fx_count() > 32 - KBSHED_FREE) continue;  // (dropped, not delayed)
        float k = 0.35f + 0.65f * kbshed_rnd(), pos[3];
        for (int c = 0; c < 3; c++) pos[c] = grip[c] + (tip[c] - grip[c]) * k;
        int v = d->shed[(int)(kbshed_rnd() * (float)d->nshed) % d->nshed];
        int h = sora_fx_spawn(v, pos, kbshed_rnd() * 6.2831853f, to_u);
        if (h < 0) continue;
        for (int i = 0; i < KBSHED_CAP; i++)
            if (!g_kbshed_live[i].t) { g_kbshed_live[i].h = h, g_kbshed_live[i].t = now ? now : 1; break; }
        done++;
    }
    if (g_kbshed_acc[blade] > 1.0f) g_kbshed_acc[blade] = 1.0f;
    return done;
}

// live "set kbfxid <item id>" (0 = back to the equipped Keyblades)
static void kbfx_live_id(int id) {
    if (id && kbfx_row(id) < 0) LOG("[sora] live: kbfxid 0x%x is not a Keyblade in KBFX_TAB: the Kingdom Key's set\n", id);
    g_kbfx_id = id;
}
