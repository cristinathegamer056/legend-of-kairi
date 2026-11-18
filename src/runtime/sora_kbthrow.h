// sora_kbthrow.h (s261 kbthrow, live "set kbthrow 0 / 1", default 1: verified by the user in s261): the aimed Keyblade throw in BASE form.
// User s260 / s261: "with the Keyblade the same hold [Triangle + R1] = an aimed Keyblade throw like Strike Raid ... in base form
// using Limit Form's Strike Raid motion + flight". NOT vanilla KH2 (there Strike Raid is Limit Form's Limit, sora_limit.h): the
// mod's own move, his call. DLL only, no re-bake:
//   * the motions = Limit's bake (data/forms/limit_motion.bin slots 924 K420 start, 928 K421 "Raid", 936 K423 the catch), given to
//     base Sora by mot_var while kbthrow is on; the POSES = the Limit draw actor's pose bake through sora_draw_actor_slot_pose
//     (sora.c kt_slot_pose / kt_draw_pose: both bodies have the same 240 bones, limit_model.bin = sora_model.bin's size + header);
//   * the flight = sora_limit.h's LmShot (speed 30 cm / frame, homing 0.087266 rad / frame, radius 160, atkp 1866 Power 200, back at
//     its target or after `kbthrowout` frames, caught at his chest), drawn with the body's own Keyblade (weapon_at on FM.body()).
// The move: Triangle + R1 held on the ground with the Keyblade in hand (no BotW weapon, no Drive Form, not Roxas / Mickey) = motion
// 231, held on its last frame while R1 is held, Sora faces the lock-on target, else the camera's forward (the aim). R1 let go =
// motion 232 (the Keyblade flies at its frame trigger 9), then 234 (the catch). B while aiming = put away, nothing paid.
// MP (user s261: "10 MP a throw"): paid at the throw; refused in MP Charge / at 0 MP (the spells' rule). GUESSES: motion 231 as the
// aim pose, the snap to the camera, kbthrowdmg 50 (% of Power 200; the blade can hit on the way out AND back), the target search.
// Live: kbthrow 0/1 | kbthrowmp <MP> | kbthrowdmg <pct> | kbthrowout <KH frames out, 24 = Strike Raid's> | kbthrowlog 0/1 |
// (s262) kbthrowaim 0/1 | kbthrowcone <deg>.
#pragma once

enum { KT_OFF, KT_AIM, KT_THROW, KT_CATCH };
static struct {
    int ph;
    bool rel, spawned, armed;
    uint64_t target;
    float dir[3];  // (s262) the aim, BotW world axes (y up), unit
    bool locked;   // (s262) the aim is a lock-on target (the throw starts level and homes, as in s261)
} KT = {.armed = true};
static int g_kt_mp = 10, g_kt_pct = 50, g_kt_out = 24, g_kt_log = 1;
// (s262 user: "let me aim it in every direction (up and down too) like BotW's weapon throws, following the camera") the blade
// leaves along the CAMERA's forward, up / down included (s261: its level part only). An auto target is kept (and homed on)
// only inside `kbthrowcone` degrees of that line (GUESS 30), so an aim at the sky is not bent down to an enemy on the ground;
// a lock-on target is always kept. Sora's own pose does not lean with the aim (motion 231 as it is). Live: kbthrowaim 0 = the
// s261 level throw, kbthrowcone <deg> (0 = any auto target, as s261).
static int g_kt_aim3d = 1, g_kt_cone = 30;
#define KT_HOLD_EPS 0.01f  // the aim holds 231 this far before its end (no frame trigger of its last frame fires again)

static void kt_end(const char *why) {
    if (!KT.ph) return;
    LOG("[kbthrow] ended (%s) in phase %s, motion %d clock %.1f\n", why, KT.ph == KT_AIM ? "aim" : KT.ph == KT_THROW ? "throw" : "catch", S.motion, S.clock);
    KT.ph = KT_OFF;
}

// sora.c, right after lm_tick_press: the start, the aim, the presses while it runs
static void kt_tick_press(uint64_t *press, uint64_t held, const Target *tg, int ntg, uint64_t present) {
    g_kt_mots = g_kbthrow != 0;
    g_kt_pose_actor = g_kbthrow ? FM.actor[FORM_LIMIT] : -1;
    if (!g_kt_slot_pose_fn && g_kbthrow) g_kt_slot_pose_fn = (bool (*)(int, int, float, float *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_draw_actor_slot_pose");
    if (!(held & BTN_THROW)) KT.armed = true;  // (one throw per R1 press)
    if (!KT.ph) {
        if (!g_kbthrow || !KT.armed || !(held & BTN_THROW) || !(held & BTN_CLIMB)) return;
        bool ground = S.act == ACT_IDLE && S.takeoff <= 0 && !S.air_frames;
        if (g_fmots_on || IN_GUEST || g_wpn_cur.def || !ground || S.special || LM.on || rc_eat_x() || pl_eat_x() || skt_eat_x() || sm_fps_eat_x()) return;
        KT.armed = false;
        if (!mot_of(231) || !mot_of(232) || g_kt_pose_actor <= 0 || !g_kt_slot_pose_fn) {
            LOG("[kbthrow] refused: Limit's motions / draw actor missing (231 %s, 232 %s, 234 %s, Limit actor %d, slot pose %s)\n", mot_of(231) ? "ok" : "MISSING",
                mot_of(232) ? "ok" : "MISSING", mot_of(234) ? "ok" : "MISSING", g_kt_pose_actor, g_kt_slot_pose_fn ? "ok" : "not in this exe");
            return;
        }
        if (g_kt_mp > 0 && (mp_charging() || MP.mp <= 0)) {
            sora_sfx_event("error", 1);
            LOG("[kbthrow] refused: no MP (MP %.0f%s)\n", MP.mp, mp_charging() ? ", MP Charge" : "");
            return;
        }
        KT.ph = KT_AIM, KT.rel = false, KT.spawned = false, KT.target = 0;
        play(231), S.special = SP_LIMIT, S.next = 0;
        if (present < g_climb_until) g_climb_until = present;  // (a pick-up window the Triangle just opened)
        LOG("[kbthrow] aim: Triangle + R1 held, motion 231 slot %d (%d frames), MP %.0f\n", S.mot ? S.mot->slot : -1, S.mot ? S.mot->frames : 0, MP.mp);
    }
    if (KT.ph == KT_AIM && (*press & BTN_JUMP)) {  // B = put away (nothing paid)
        S.special = SP_NONE, S.motion = -1;
        kt_end("B: put away");
        *press &= ~(uint64_t)BTN_JUMP;
        return;
    }
    *press &= ~(uint64_t)(BTN_ATTACK | BTN_JUMP | BTN_ROLL | BTN_CLIMB);
    if (KT.ph != KT_AIM) return;
    if (!(held & BTN_THROW) && !KT.rel) {
        KT.rel = true;
        if (g_kt_log) LOG("[kbthrow] R1 let go at clock %.1f\n", S.clock);
    }
    const Target *t = g_lock.actor ? target_of(tg, ntg, g_lock.actor) : NULL;
    float cf[2], cv[12];
    KT.locked = t && t->life > 0 && t->hd > 1;
    if (KT.locked) S.face = atan2f(t->d[0], t->d[1]);
    else {
        t = NULL;
        if (linkpos_camera_fwd(cf)) S.face = atan2f(cf[0], cf[1]);
        t = target_auto(tg, ntg, S.face, (float)g_kt_out * 30.0f + 160.0f);  // (what the blade can reach on its way out)
    }
    KT.dir[0] = sinf(S.face), KT.dir[1] = 0, KT.dir[2] = cosf(S.face);
    if (!KT.locked && g_kt_aim3d && linkpos_camera_view(cv)) {  // (view rows right / up / back: forward = -back)
        float l = sqrtf(cv[8] * cv[8] + cv[9] * cv[9] + cv[10] * cv[10]);
        if (l > 1e-4f) {
            KT.dir[0] = -cv[8] / l, KT.dir[1] = -cv[9] / l, KT.dir[2] = -cv[10] / l;
            if (t && g_kt_cone > 0) {  // the auto target from Sora's chest (dy is KH: down = +), against the aim line
                float v[3] = {t->d[0], -t->dy - 100.0f, t->d[1]}, vl = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                if (vl > 1 && (v[0] * KT.dir[0] + v[1] * KT.dir[1] + v[2] * KT.dir[2]) / vl < cosf((float)g_kt_cone * 0.0174533f)) t = NULL;
            }
        }
    }
    KT.target = t && t->life > 0 ? t->actor : 0;
}

// sora.c step 2, before lm_chain and the motion-end check
static void kt_chain(void) {
    if (!KT.ph) return;
    if (S.special != SP_LIMIT || !S.mot || S.motion < 231 || S.motion > 234) { kt_end("interrupted (hit / fall / other)"); return; }
    if (KT.ph == KT_AIM) {
        if (S.clock < (float)S.mot->frames - KT_HOLD_EPS) return;
        if (!KT.rel) { S.clock = (float)S.mot->frames - KT_HOLD_EPS; return; }  // (held: the aim pose)
        float mp0 = MP.mp;
        if (g_kt_mp > 0) mp_change(-g_kt_mp);
        KT.ph = KT_THROW, KT.spawned = false;
        play(232), S.special = SP_LIMIT, S.next = 0;
        LOG("[kbthrow] throw: motion 232 slot %d, MP %.0f -> %.0f%s, target %llx\n", S.mot ? S.mot->slot : -1, mp0, MP.mp, mp_charging() ? " (MP Charge)" : "",
            (unsigned long long)KT.target);
    } else if (S.clock >= S.mot->frames) {
        if (KT.ph == KT_THROW && mot_of(234)) {
            KT.ph = KT_CATCH;
            play(234), S.special = SP_LIMIT, S.next = 0;
            if (g_kt_log) LOG("[kbthrow] catch: motion 234 slot %d\n", S.mot ? S.mot->slot : -1);
        } else kt_end("done");  // (the end check after this takes him back to idle)
    }
}

// sora.c, right before lm_frame (which flies, hits, draws and catches the blade): the release at 232's frame trigger 9
static void kt_spawn(const float feet[3], float to_u) {
    if (KT.ph != KT_THROW || KT.spawned || !S.mot || S.clock < mot_frame_trigger(S.mot, 9, 8)) return;
    KT.spawned = true;
    for (int i = 0; i < LM_SHOTS; i++)
        if (g_lms[i].on && (g_lms[i].kind == LMA_RAID || g_lms[i].kind == LMA_JUDGMENT)) g_lms[i].on = false;  // (one blade)
    LmShot *o = lm_shot_new();
    if (!o) return;
    float tip[3];
    cast_tip(feet, to_u, tip);
    o->on = true, o->kind = LMA_RAID, o->base = true, o->out = g_kt_out, o->atkp = 1866, o->power = 200, o->pct = g_kt_pct;
    o->speed = 30, o->turn = 0.087266f, o->r = 160, o->life = 90, o->target = KT.target;
    memcpy(o->pos, tip, sizeof o->pos);
    o->dir[0] = sinf(S.face), o->dir[1] = 0, o->dir[2] = cosf(S.face);
    if (g_kt_aim3d && !KT.locked) memcpy(o->dir, KT.dir, sizeof o->dir);  // (s262: the camera's line, up / down too)
    if (lf_has("limit_raid_trail")) o->fx = sora_fx_event("limit_raid_trail", tip, S.face, to_u);
    int se = g_lf_on ? sora_sfx_event("limit_raid_trail", 1.0f) : -1;
    LOG("[kbthrow] the Keyblade flies (atkp %d power %d x %d%%, out %d frames, target %llx%s, fx %d, SE %d), aim %.2f %.2f %.2f = %.0f deg %s\n", o->atkp, o->power, o->pct,
        o->out, (unsigned long long)o->target, KT.locked ? " (lock-on)" : "", o->fx, se, o->dir[0], o->dir[1], o->dir[2],
        fabsf(asinf(fmaxf(-1, fminf(1, o->dir[1])))) * 57.29578f, o->dir[1] > 0.01f ? "up" : o->dir[1] < -0.01f ? "down" : "level");
}

static bool kt_live_set(const char *key, int v) {
    if (!strcmp(key, "kbthrow")) g_kbthrow = v != 0, LOG("[kbthrow] kbthrow %d: %s\n", g_kbthrow, g_kbthrow ? "Triangle + R1 held with the Keyblade (base form) = the aimed throw" : "off");
    else if (!strcmp(key, "kbthrowmp")) g_kt_mp = v < 0 ? 0 : v > 100 ? 100 : v, LOG("[kbthrow] MP cost %d\n", g_kt_mp);
    else if (!strcmp(key, "kbthrowdmg")) g_kt_pct = v < 1 ? 1 : v > 400 ? 400 : v, LOG("[kbthrow] damage %d%% of Power 200\n", g_kt_pct);
    else if (!strcmp(key, "kbthrowout")) g_kt_out = v < 4 ? 4 : v > 200 ? 200 : v, LOG("[kbthrow] out for %d KH frames (%d cm)\n", g_kt_out, g_kt_out * 30);
    else if (!strcmp(key, "kbthrowlog")) g_kt_log = v != 0;
    else if (!strcmp(key, "kbthrowaim")) g_kt_aim3d = v != 0, LOG("[kbthrow] kbthrowaim %d: the blade leaves %s\n", g_kt_aim3d, g_kt_aim3d ? "along the camera's line, up / down too" : "level (the s261 way)");
    else if (!strcmp(key, "kbthrowcone")) g_kt_cone = v < 0 ? 0 : v > 180 ? 180 : v, LOG("[kbthrow] kbthrowcone %d deg: an auto target %s\n", g_kt_cone, g_kt_cone ? "only inside that cone of the aim" : "anywhere in reach (the s261 way)");
    else return false;
    return true;
}
