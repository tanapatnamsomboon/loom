#version 460 core

layout (location = 0) out vec4 oColor;
layout (location = 1) out int  oEntityID;

in vec3 vDirection;

uniform samplerCube uSkybox;

// Inline tonemap + sRGB to match mesh.frag's output pipeline. Without this the
// skybox would look much brighter than equivalent lit geometry, because lit
// geometry goes through ACES + gamma but the skybox would write raw HDR linear
// values.
const float kGamma = 2.2;

vec3 ACESFilm(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr        = texture(uSkybox, normalize(vDirection)).rgb;
    vec3 tonemapped = ACESFilm(hdr);
    vec3 display    = pow(tonemapped, vec3(1.0 / kGamma));

    oColor    = vec4(display, 1.0);
    oEntityID = -1;
}
