// (s250 hdr) Sora in BotW on Savage Wind 0.6.0's float HDR picture (SAVAGEWIND_SORA_HDR_GAME=1 with SAVAGEWIND_SORA):
// the colour maths, in one place for device.c (the F5 / `shot` picture on the CPU), its present pass (the same formulas
// as GLSL: SHC_GLSL) and the host test (runtime/test/hdr_test.c).
//
// Picture units (upstream, hdr.c + device.c hdr_game_write): linear BT.709, 1.0 = the HUD / menu white. The game's
// tonemap writes the scene times hdr_game_nits / hdr_ui_nits (vk_hdr_params v[0]), BotW's own HUD is drawn at 0..1,
// and the present pass shows 1.0 at hdr_ui_nits, everything capped at hdr_peak_nits. No nits number of our own.
//
// The KH2 HUD, the camp / pause menu, name tags, the notice and Sora's present-time draws (KH2 Game Over screen, the
// "no composite" fallbacks) are made for the 8-bit window image: sRGB-encoded values, blended in that encoding. On the
// float picture they go into a layer of their own (RGBA16F, cleared to 0, premultiplied, sRGB-encoded, a = coverage),
// and the layer is put over the picture here, in the sRGB encoding:
//     out = dec(clamp(L.rgb + (1 - L.a) * enc(bg), 0, max(1, (1 - L.a) * enc(bg))))
// For a picture value bg in 0..1 this is exactly what the 8-bit window showed in SDR (the same blends on the same
// encoded values, clamped at 1 as a UNORM image clamps), so the HUD keeps its SDR look and sits at the HUD white. Over
// a highlight above 1.0 the HUD blends in the same encoding; it never pushes a pixel above what the picture had there.
// enc / dec = the piecewise sRGB curve (the game's own SRGB-format copy and upstream's menu decode, k_hdr_ui_fs), its
// power segment continued above 1.0.
#pragma once
#include <math.h>

static inline float shc_srgb_enc(float l) {  // linear -> sRGB code value (0 below 0; above 1.0 the power segment goes on)
    if (!(l > 0.0f)) return 0.0f;
    return l <= 0.0031308f ? 12.92f * l : 1.055f * powf(l, 1.0f / 2.4f) - 0.055f;
}

static inline float shc_srgb_dec(float e) {  // sRGB code value -> linear (the inverse of shc_srgb_enc)
    if (!(e > 0.0f)) return 0.0f;
    return e <= 0.04045f ? e / 12.92f : powf((e + 0.055f) / 1.055f, 2.4f);
}

// The layer over one picture pixel: bg = linear picture rgb, lay = the layer's rgba (premultiplied sRGB-encoded rgb,
// coverage a). An empty layer pixel (all 0) leaves bg exactly as it was.
static inline void shc_compose(const float bg[3], const float lay[4], float out[3]) {
    if (lay[0] == 0.0f && lay[1] == 0.0f && lay[2] == 0.0f && lay[3] == 0.0f) {
        out[0] = bg[0], out[1] = bg[1], out[2] = bg[2];
        return;
    }
    float k = 1.0f - lay[3];
    k = k < 0.0f ? 0.0f : k > 1.0f ? 1.0f : k;
    for (int c = 0; c < 3; c++) {
        float e = shc_srgb_enc(bg[c]) * k, o = lay[c] + e, cap = e > 1.0f ? e : 1.0f;
        o = o < 0.0f ? 0.0f : o > cap ? cap : o;
        out[c] = shc_srgb_dec(o);
    }
}

// Sora's scene pass draws into the game's final image after the tonemap, where the scene's white is game / ui
// (hdr.c vk_hdr_params v[0]: "the scene's white relative to the HUD's"): his values are put at that white too.
static inline float shc_scene_scale(float game_nits, float ui_nits) {
    return game_nits > 0.0f && ui_nits > 0.0f ? game_nits / ui_nits : 1.0f;
}

// What the present pass sends for a picture value v (device.c k_hdr_pq_fs: v * ui nits, capped at the peak; peak <= 0 =
// no cap), before the BT.2020 matrix and PQ.
static inline float shc_nits(float v, float ui_nits, float peak_nits) {
    float n = (v > 0.0f ? v : 0.0f) * ui_nits;
    return peak_nits > 0.0f && n > peak_nits ? peak_nits : n;
}

// The same as GLSL (device.c's present pass for the float picture with the layer).
#define SHC_GLSL                                                                                                        \
    "vec3 shc_enc(vec3 l) {\n"                                                                                         \
    "  l = max(l, vec3(0.0));\n"                                                                                       \
    "  return mix(12.92 * l, 1.055 * pow(l, vec3(1.0 / 2.4)) - 0.055, greaterThan(l, vec3(0.0031308)));\n"             \
    "}\n"                                                                                                              \
    "vec3 shc_dec(vec3 e) {\n"                                                                                         \
    "  e = max(e, vec3(0.0));\n"                                                                                       \
    "  return mix(e / 12.92, pow((e + 0.055) / 1.055, vec3(2.4)), greaterThan(e, vec3(0.04045)));\n"                   \
    "}\n"                                                                                                              \
    "vec3 shc_compose(vec3 bg, vec4 L) {\n"                                                                            \
    "  if (L == vec4(0.0)) return bg;\n"                                                                               \
    "  float k = clamp(1.0 - L.a, 0.0, 1.0);\n"                                                                        \
    "  vec3 e = shc_enc(bg) * k;\n"                                                                                    \
    "  return shc_dec(clamp(L.rgb + e, vec3(0.0), max(e, vec3(1.0))));\n"                                              \
    "}\n"
