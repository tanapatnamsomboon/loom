#version 460 core

in  vec3 vLocalPos;
out vec4 oColor;

uniform sampler2D uEquirect;

const vec2 kInvAtan = vec2(0.1591, 0.3183); // 1/(2π), 1/π

// Maps a 3D direction on the unit sphere to a UV in a 2:1 equirectangular map.
vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= kInvAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec3 dir   = normalize(vLocalPos);
    vec2 uv    = SampleSphericalMap(dir);
    vec3 color = texture(uEquirect, uv).rgb;
    oColor = vec4(color, 1.0);
}
