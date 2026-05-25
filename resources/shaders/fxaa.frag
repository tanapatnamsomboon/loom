#version 460 core
// FXAA — Lottes-derived "FXAA Console" variant (public-domain NVIDIA shader).
// Single fullscreen pass that runs AFTER tonemap on the LDR sRGB display
// buffer. Luma thresholds are tuned for sRGB-space input; running FXAA on
// linear HDR data would over-smooth low-contrast dark regions.
//
// Algorithm:
//   1. Sample center + 4 diagonal corners. Compute per-pixel luma.
//   2. If luma range (max - min) is below the threshold band, the pixel is
//      flat / inside a smooth area — pass center through unchanged. (Fast
//      path covers the majority of pixels.)
//   3. Otherwise compute an edge direction from the diagonal luma gradient
//      and walk along it for two sample-blends, picking whichever stays
//      within the original luma range.

in  vec2 vUV;
out vec4 oColor;

uniform sampler2D uSource;
uniform vec2      uTexelSize; // 1.0 / framebuffer size in pixels

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec3 cc = texture(uSource, vUV).rgb;
    vec3 nw = texture(uSource, vUV + vec2(-1.0, -1.0) * uTexelSize).rgb;
    vec3 ne = texture(uSource, vUV + vec2( 1.0, -1.0) * uTexelSize).rgb;
    vec3 sw = texture(uSource, vUV + vec2(-1.0,  1.0) * uTexelSize).rgb;
    vec3 se = texture(uSource, vUV + vec2( 1.0,  1.0) * uTexelSize).rgb;

    float lc  = luma(cc);
    float lnw = luma(nw);
    float lne = luma(ne);
    float lsw = luma(sw);
    float lse = luma(se);

    float l_min = min(lc, min(min(lnw, lne), min(lsw, lse)));
    float l_max = max(lc, max(max(lnw, lne), max(lsw, lse)));
    float l_range = l_max - l_min;

    // Fast path — flat or near-flat region. The threshold band has both an
    // absolute floor (kEdgeMin, ~0.0312) and a relative ceiling (kEdgeRel,
    // ~0.125 of l_max) so dim regions still anti-alias proportionally.
    const float kEdgeMin = 0.0312;
    const float kEdgeRel = 0.125;
    if (l_range < max(kEdgeMin, l_max * kEdgeRel)) {
        oColor = vec4(cc, 1.0);
        return;
    }

    // Edge direction from diagonal luma gradients. Length proportional to the
    // contrast — rcp_dir_min then normalizes so a 1-pixel-wide edge gets a
    // ~1-texel offset and high-contrast edges get clamped to 8 texels max.
    vec2 dir;
    dir.x = -((lnw + lne) - (lsw + lse));
    dir.y =  ((lnw + lsw) - (lne + lse));

    float dir_reduce  = max((lnw + lne + lsw + lse) * (0.25 * 0.125), 1e-4);
    float rcp_dir_min = 1.0 / (min(abs(dir.x), abs(dir.y)) + dir_reduce);
    dir = clamp(dir * rcp_dir_min, vec2(-8.0), vec2(8.0)) * uTexelSize;

    // Two-tier sample blend. r1 = average of two mid-offset samples; r2 mixes
    // r1 with a wider pair. If r2's luma falls outside the original range,
    // the edge walk overshot — fall back to r1.
    vec3 r1 = 0.5 * (
        texture(uSource, vUV + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(uSource, vUV + dir * (2.0 / 3.0 - 0.5)).rgb);

    vec3 r2 = r1 * 0.5 + 0.25 * (
        texture(uSource, vUV + dir * -0.5).rgb +
        texture(uSource, vUV + dir *  0.5).rgb);

    float l2 = luma(r2);
    oColor = (l2 < l_min || l2 > l_max) ? vec4(r1, 1.0) : vec4(r2, 1.0);
}
