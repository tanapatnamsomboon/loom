#version 460 core
// Bloom upsample — 3x3 tent filter on the smaller-mip bloom result. The
// engine calls this with additive blending enabled (GL_ONE, GL_ONE), so
// the output is added on top of the current-level mip's existing
// downsampled content. Walking the chain from smallest mip up to mip 0
// progressively spreads the brights into a wide, soft glow.
in  vec2 vUV;
out vec4 oColor;

uniform sampler2D uSource;       // smaller (more blurred) mip
uniform vec2      uSrcTexelSize; // 1.0 / size of the smaller mip

void main() {
    vec3 a = textureLod(uSource, vUV + uSrcTexelSize * vec2(-1.0, -1.0), 0.0).rgb;
    vec3 b = textureLod(uSource, vUV + uSrcTexelSize * vec2( 0.0, -1.0), 0.0).rgb;
    vec3 c = textureLod(uSource, vUV + uSrcTexelSize * vec2( 1.0, -1.0), 0.0).rgb;
    vec3 d = textureLod(uSource, vUV + uSrcTexelSize * vec2(-1.0,  0.0), 0.0).rgb;
    vec3 e = textureLod(uSource, vUV,                                    0.0).rgb;
    vec3 f = textureLod(uSource, vUV + uSrcTexelSize * vec2( 1.0,  0.0), 0.0).rgb;
    vec3 g = textureLod(uSource, vUV + uSrcTexelSize * vec2(-1.0,  1.0), 0.0).rgb;
    vec3 h = textureLod(uSource, vUV + uSrcTexelSize * vec2( 0.0,  1.0), 0.0).rgb;
    vec3 i = textureLod(uSource, vUV + uSrcTexelSize * vec2( 1.0,  1.0), 0.0).rgb;

    // Tent weights: center 4, edges 2, corners 1, normalized by 1/16.
    vec3 result = (a + c + g + i) * (1.0 / 16.0)
                + (b + d + f + h) * (2.0 / 16.0)
                +  e              * (4.0 / 16.0);
    oColor = vec4(result, 1.0);
}
