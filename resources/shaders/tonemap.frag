#version 460 core

out vec4 oColor;

uniform sampler2D uHDRScene; // linear HDR scene color (unit 0)

const float kGamma = 2.2;

// ACES filmic tonemap — Krzysztof Narkowicz's curve-fit approximation.
// Maps open-ended linear HDR to [0,1] with film-like highlight roll-off.
vec3 ACESFilm(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec2 uv  = gl_FragCoord.xy / vec2(textureSize(uHDRScene, 0));
    vec3 hdr = texture(uHDRScene, uv).rgb;

    // ACES tonemap then linear → sRGB encode for the display framebuffer.
    vec3 ldr = pow(ACESFilm(hdr), vec3(1.0 / kGamma));

    oColor = vec4(ldr, 1.0);
}
