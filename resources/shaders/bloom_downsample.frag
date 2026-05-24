#version 460 core
// Bloom downsample — Jimenez 2014 13-tap kernel (COD: Advanced Warfare bloom).
// Sums an inner 2x2 block at half-texel offsets with a 3x3 outer ring, giving
// a smoother downsample than naive 4-tap box filtering. When uPrefilter is 1
// (the first iteration only), applies a hard luminance threshold so the chain
// only spreads bright pixels — the classic "bright pass" step folded into the
// first downsample for efficiency.
in  vec2 vUV;
out vec4 oColor;

uniform sampler2D uSource;
uniform vec2      uSrcTexelSize; // 1.0 / size of the source texture
uniform int       uPrefilter;    // 1 = apply threshold; 0 = pure downsample
uniform float     uThreshold;

// Anti-firefly toolkit applied during the prefilter (first downsample):
//   1. Per-sample brightness cap — single hot HDR pixels (skybox sun, sharp
//      specular) capped to a sane max before any averaging.
//   2. Karis 2013 weighted average — weight = 1 / (1 + max_channel) compresses
//      bright outliers further. Max-channel (not luminance) catches saturated
//      single-channel brights that luminance would underweight.
//   3. Quadratic soft-knee threshold — smooth on/off transition across a band
//      around uThreshold, eliminates the discrete pop that hard cutoffs cause
//      when sub-pixel camera motion shifts a tap across the threshold.
// Non-prefilter downsample passes use the plain partial average since their
// input has already been smoothed by the prefilter.

vec3  safe(vec3 c)          { return min(c, vec3(50.0)); }
float karis_lum(vec3 c)     { return max(c.r, max(c.g, c.b)); }

vec3 karis(vec3 a, vec3 b, vec3 c, vec3 d) {
    float wa = 1.0 / (1.0 + karis_lum(a));
    float wb = 1.0 / (1.0 + karis_lum(b));
    float wc = 1.0 / (1.0 + karis_lum(c));
    float wd = 1.0 / (1.0 + karis_lum(d));
    return (a*wa + b*wb + c*wc + d*wd) / (wa + wb + wc + wd);
}
vec3 partial(vec3 a, vec3 b, vec3 c, vec3 d) { return (a + b + c + d) * 0.25; }

void main() {
    vec3 a = textureLod(uSource, vUV + uSrcTexelSize * vec2(-1.0, -1.0), 0.0).rgb;
    vec3 b = textureLod(uSource, vUV + uSrcTexelSize * vec2( 0.0, -1.0), 0.0).rgb;
    vec3 c = textureLod(uSource, vUV + uSrcTexelSize * vec2( 1.0, -1.0), 0.0).rgb;
    vec3 d = textureLod(uSource, vUV + uSrcTexelSize * vec2(-0.5, -0.5), 0.0).rgb;
    vec3 e = textureLod(uSource, vUV + uSrcTexelSize * vec2( 0.5, -0.5), 0.0).rgb;
    vec3 f = textureLod(uSource, vUV + uSrcTexelSize * vec2(-1.0,  0.0), 0.0).rgb;
    vec3 g = textureLod(uSource, vUV,                                    0.0).rgb;
    vec3 h = textureLod(uSource, vUV + uSrcTexelSize * vec2( 1.0,  0.0), 0.0).rgb;
    vec3 i = textureLod(uSource, vUV + uSrcTexelSize * vec2(-0.5,  0.5), 0.0).rgb;
    vec3 j = textureLod(uSource, vUV + uSrcTexelSize * vec2( 0.5,  0.5), 0.0).rgb;
    vec3 k = textureLod(uSource, vUV + uSrcTexelSize * vec2(-1.0,  1.0), 0.0).rgb;
    vec3 l = textureLod(uSource, vUV + uSrcTexelSize * vec2( 0.0,  1.0), 0.0).rgb;
    vec3 m = textureLod(uSource, vUV + uSrcTexelSize * vec2( 1.0,  1.0), 0.0).rgb;

    // Group the 13 taps into 5 overlapping quads with Jimenez weights:
    //   center 2x2 (d,e,i,j) -> 0.5
    //   four outer quads     -> 0.125 each
    vec3 color;
    if (uPrefilter == 1) {
        // Pre-cap each sample, then Karis-weight per quad.
        a = safe(a); b = safe(b); c = safe(c); d = safe(d); e = safe(e);
        f = safe(f); g = safe(g); h = safe(h); i = safe(i); j = safe(j);
        k = safe(k); l = safe(l); m = safe(m);

        color = karis(d, e, i, j) * 0.5
              + karis(a, b, d, f) * 0.125
              + karis(b, c, e, h) * 0.125
              + karis(f, i, k, l) * 0.125
              + karis(h, j, l, m) * 0.125;

        // Soft-knee quadratic threshold (Unity URP / Strzelecki). knee = half
        // the threshold gives a smooth band [threshold - knee, threshold + knee]
        // where the contribution ramps up quadratically instead of snapping on.
        float brightness = max(color.r, max(color.g, color.b));
        float knee       = uThreshold * 0.5;
        float soft       = clamp(brightness - uThreshold + knee, 0.0, 2.0 * knee);
        soft             = soft * soft / (4.0 * knee + 1e-4);
        float contribution = max(soft, brightness - uThreshold) / max(brightness, 1e-5);
        color *= contribution;
    } else {
        color = partial(d, e, i, j) * 0.5
              + partial(a, b, d, f) * 0.125
              + partial(b, c, e, h) * 0.125
              + partial(f, i, k, l) * 0.125
              + partial(h, j, l, m) * 0.125;
    }
    oColor = vec4(color, 1.0);
}
