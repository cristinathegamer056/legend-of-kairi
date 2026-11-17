// host test for runtime/sora_hdr_core.h (s250 hdr: the KH2 HUD layer on 0.6.0's float HDR picture).
//   clang -O1 -Wall -Wextra -Iruntime runtime/test/hdr_test.c -o hdr_test.exe && ./hdr_test.exe
#include <stdio.h>
#include <string.h>
#include "sora_hdr_core.h"

static int runs, fails;
#define CHECK(c) do { runs++; if (!(c)) { fails++; printf("  FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#define NEAR(a, b, eps) (fabsf((a) - (b)) <= (eps))

// What the 8-bit window showed in SDR: the HUD's premultiplied colour over the game's sRGB-encoded pixel, UNORM-clamped,
// then read back as linear (the reference shc_compose must hit for picture values 0..1).
static float sdr_ref(float bg, float lay_c, float lay_a) {
    float o = lay_c + (1.0f - lay_a) * shc_srgb_enc(bg);
    o = o < 0 ? 0 : o > 1 ? 1 : o;
    return shc_srgb_dec(o);
}

int main(void) {
    // ---- the sRGB curve: fixed points, the knee, round trips, above 1.0
    CHECK(shc_srgb_enc(0) == 0 && shc_srgb_dec(0) == 0);
    CHECK(NEAR(shc_srgb_enc(1), 1.0f, 1e-6f) && NEAR(shc_srgb_dec(1), 1.0f, 1e-6f));
    CHECK(NEAR(shc_srgb_enc(0.0031308f), 0.04045f, 2e-5f));                 // (both segments meet at the knee)
    CHECK(NEAR(shc_srgb_dec(0.5f), 0.214041f, 1e-5f));                      // (sRGB 128/255-ish mid grey)
    CHECK(NEAR(shc_srgb_enc(0.18f), 0.461356f, 1e-5f));
    CHECK(shc_srgb_enc(-1) == 0 && shc_srgb_dec(-0.5f) == 0);               // (negatives: 0, never NaN)
    CHECK(shc_srgb_enc(NAN) == 0 && shc_srgb_dec(NAN) == 0);
    float worst = 0;
    for (int i = 0; i <= 4000; i++) {  // 0 .. 4.0 (HDR highlights up to 4x the HUD white)
        float l = i / 1000.0f, r = shc_srgb_dec(shc_srgb_enc(l)), d = fabsf(r - l) / (l > 1e-3f ? l : 1e-3f);
        if (d > worst) worst = d;
    }
    CHECK(worst < 2e-5f);
    CHECK(shc_srgb_enc(2.0f) > 1.0f && shc_srgb_dec(1.2f) > 1.0f);          // (the power segment goes on above 1)
    int mono = 1;
    for (int i = 1; i <= 3000; i++) mono &= shc_srgb_enc(i / 1000.0f) > shc_srgb_enc((i - 1) / 1000.0f);
    CHECK(mono);

    // ---- the layer over the picture
    float out[3];
    {   // empty layer pixel: the picture exactly as it was, highlights included
        float bg[3] = {0.25f, 1.0f, 3.75f}, L[4] = {0, 0, 0, 0};
        shc_compose(bg, L, out);
        CHECK(out[0] == bg[0] && out[1] == bg[1] && out[2] == bg[2]);
    }
    {   // an opaque white HUD texel = 1.0 = the HUD white, whatever is behind it (also a highlight)
        float bg[3] = {0.1f, 0.9f, 6.0f}, L[4] = {1, 1, 1, 1};
        shc_compose(bg, L, out);
        CHECK(NEAR(out[0], 1, 1e-6f) && NEAR(out[1], 1, 1e-6f) && NEAR(out[2], 1, 1e-6f));
        CHECK(NEAR(shc_nits(out[0], 203, 250), 203, 1e-3f));                // (the present pass: the HUD white at hdr_ui_nits)
        CHECK(NEAR(shc_nits(out[0], 360, 1000), 360, 1e-3f));
    }
    {   // opaque KH2 colours keep their SDR value: the sRGB texel decoded (the HP green 0x7EE794-ish)
        float bg[3] = {0.3f, 0.3f, 0.3f}, L[4] = {126 / 255.0f, 231 / 255.0f, 148 / 255.0f, 1};
        shc_compose(bg, L, out);
        CHECK(NEAR(out[0], shc_srgb_dec(126 / 255.0f), 1e-6f) && NEAR(out[1], shc_srgb_dec(231 / 255.0f), 1e-6f));
    }
    {   // every blend of an SDR picture value = what the 8-bit window showed (alpha over, additive, subtract)
        float wmax = 0;
        for (int ib = 0; ib <= 20; ib++)
            for (int ic = 0; ic <= 10; ic++)
                for (int ia = 0; ia <= 10; ia++) {
                    float bgv = ib / 20.0f, c = ic / 10.0f, a = ia / 10.0f;
                    float bg[3] = {bgv, bgv, bgv};
                    float over[4] = {c * a, c * a, c * a, a};    // premultiplied "over" (the HUD's blend 0, ImGui-like)
                    float add[4] = {c * a, c * a, c * a, 0};     // additive (HUD mode 4, camp blend 1: coverage kept)
                    float sub[4] = {-c * a, -c * a, -c * a, 0};  // subtract (camp blend 2: dst - src)
                    shc_compose(bg, over, out);
                    float d = fabsf(out[0] - sdr_ref(bgv, c * a, a));
                    shc_compose(bg, add, out);
                    d = fmaxf(d, fabsf(out[1] - sdr_ref(bgv, c * a, 0)));
                    shc_compose(bg, sub, out);
                    d = fmaxf(d, fabsf(out[2] - sdr_ref(bgv, -c * a, 0)));
                    if (d > wmax) wmax = d;
                }
        CHECK(wmax < 1e-5f);
        printf("  SDR blends on the float picture: worst difference %.2e (linear)\n", wmax);
    }
    {   // the dim / camp backdrop: 50 %% black over mid grey darkens as in SDR (sRGB-space), not as a linear blend
        float bg[3] = {0.5f, 0.5f, 0.5f}, L[4] = {0, 0, 0, 0.5f};
        shc_compose(bg, L, out);
        CHECK(NEAR(out[0], shc_srgb_dec(0.5f * shc_srgb_enc(0.5f)), 1e-6f));
        CHECK(out[0] < 0.25f - 0.05f);  // (a linear blend would give 0.25)
    }
    {   // two HUD layers in a row (premultiplied accumulation) = the two blends in SDR one after the other
        float bgv = 0.4f, c1 = 0.8f, a1 = 0.6f, c2 = 0.2f, a2 = 0.3f;
        float Lrgb = c2 * a2 + (1 - a2) * (c1 * a1), La = a2 + (1 - a2) * a1;  // (the layer after both draws)
        float bg[3] = {bgv, bgv, bgv}, L[4] = {Lrgb, Lrgb, Lrgb, La};
        shc_compose(bg, L, out);
        float sdr = c2 * a2 + (1 - a2) * (c1 * a1 + (1 - a1) * shc_srgb_enc(bgv));
        CHECK(NEAR(out[0], shc_srgb_dec(sdr), 1e-6f));
    }
    {   // over a highlight above the HUD white: an additive glow never brightens it, a translucent panel darkens it,
        // the result stays finite and >= 0
        float bg[3] = {4.0f, 4.0f, 4.0f}, glow[4] = {0.5f, 0.5f, 0.5f, 0}, panel[4] = {0, 0, 0, 0.5f};
        shc_compose(bg, glow, out);
        CHECK(NEAR(out[0], 4.0f, 1e-4f));
        shc_compose(bg, panel, out);
        CHECK(out[0] < 4.0f && out[0] > 0.5f && isfinite(out[0]));
        float neg[4] = {-5, -5, -5, 0};
        shc_compose(bg, neg, out);
        CHECK(out[0] == 0);
        float over_one[4] = {3, 3, 3, 1};  // (a layer value above 1: clamped to the HUD white like the 8-bit image)
        float dark[3] = {0.2f, 0.2f, 0.2f};
        shc_compose(dark, over_one, out);
        CHECK(NEAR(out[0], 1, 1e-6f));
    }
    // ---- the white levels: upstream's own numbers only
    CHECK(shc_scene_scale(203, 203) == 1.0f);                // (the defaults: Sora's scene pass unchanged)
    CHECK(NEAR(shc_scene_scale(250, 203), 250 / 203.0f, 1e-6f));
    CHECK(shc_scene_scale(203, 0) == 1.0f && shc_scene_scale(0, 203) == 1.0f);
    CHECK(shc_nits(2.0f, 203, 250) == 250 && shc_nits(2.0f, 203, 0) == 406 && shc_nits(-1, 203, 250) == 0);
    CHECK(strstr(SHC_GLSL, "0.0031308") && strstr(SHC_GLSL, "0.04045") && strstr(SHC_GLSL, "1.055"));
    printf("hdr_test: %d checks, %d failed -> %s\n", runs, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
