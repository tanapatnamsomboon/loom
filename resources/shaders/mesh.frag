#version 460 core

layout(location = 0) out vec4 oColor;
layout(location = 1) out int  oEntityID;

in vec3 vWorldNormal;
in vec2 vTexCoord;

uniform sampler2D uAlbedoTexture;
uniform vec4      uAlbedoColor;
uniform float     uRoughness;  // unused until PBR slice
uniform float     uMetallic;   // unused until PBR slice
uniform int       uEntityID;

// Hardcoded directional light. Replaced with scene-driven DirectionalLightComponent
// in the next slice ("Basic lighting") — exists here only so meshes are visible.
const vec3  kLightDir   = normalize(vec3(-0.4, -1.0, -0.3));
const vec3  kLightColor = vec3(1.0, 0.97, 0.92);
const float kAmbient    = 0.25;

void main() {
    vec4 albedo = texture(uAlbedoTexture, vTexCoord) * uAlbedoColor;

    float n_dot_l = max(dot(normalize(vWorldNormal), -kLightDir), 0.0);
    vec3  shading = kLightColor * (kAmbient + (1.0 - kAmbient) * n_dot_l);

    oColor    = vec4(albedo.rgb * shading, albedo.a);
    oEntityID = uEntityID;
}
