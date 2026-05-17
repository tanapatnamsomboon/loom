#version 460 core

layout(location = 0) out vec4 oColor;
layout(location = 1) out int  oEntityID;

in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vTexCoord;
in vec4 vLightSpacePos;

#define MAX_DIR_LIGHTS   4
#define MAX_POINT_LIGHTS 16

uniform sampler2D uAlbedoTexture;
uniform sampler2D uShadowMap;
uniform vec4      uAlbedoColor;
uniform float     uRoughness;
uniform float     uMetallic;   // unused until PBR slice
uniform int       uEntityID;
uniform vec3      uViewPos;
uniform int       uShadowsEnabled;

uniform int   uDirLightCount;
uniform vec3  uDirLightDir[MAX_DIR_LIGHTS];   // direction light propagates
uniform vec3  uDirLightColor[MAX_DIR_LIGHTS]; // intensity baked in

uniform int   uPointLightCount;
uniform vec3  uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3  uPointLightColor[MAX_POINT_LIGHTS];
uniform float uPointLightRange[MAX_POINT_LIGHTS];

// Always-on ambient so meshes are visible even when no lights are placed.
const vec3 kFallbackAmbient = vec3(0.20);

// Constant depth bias for hard shadows. Slice B replaces this with a
// slope-scale bias derived from the normal-vs-light angle.
const float kShadowBias = 0.0015;

// Returns 1.0 when the fragment is lit, 0.0 when occluded. Returns 1.0 for any
// fragment outside the shadow frustum (the shadow map's border samples as 1.0,
// see DEPTH32F setup in opengl_framebuffer.cpp).
float SampleShadow(vec4 light_space_pos) {
    if (uShadowsEnabled == 0) return 1.0;

    vec3 proj = light_space_pos.xyz / light_space_pos.w; // perspective-style divide (safe for ortho too)
    proj = proj * 0.5 + 0.5;                              // NDC [-1,1] -> [0,1] for texture sampling

    // Outside the orthographic frustum -> always lit. Behind the near plane (z<0)
    // also counts as lit so points "behind the light" don't fall into shadow.
    if (proj.z > 1.0 || proj.z < 0.0) return 1.0;

    float depth_in_map = texture(uShadowMap, proj.xy).r;
    return (proj.z - kShadowBias) > depth_in_map ? 0.0 : 1.0;
}

void main() {
    vec4 albedo = texture(uAlbedoTexture, vTexCoord) * uAlbedoColor;
    vec3 N      = normalize(vWorldNormal);
    vec3 V      = normalize(uViewPos - vWorldPos);

    // Shininess derived from roughness:
    //   roughness 0 -> shiny  (256)
    //   roughness 1 -> matte  (2)
    float shininess = mix(2.0, 256.0, 1.0 - clamp(uRoughness, 0.0, 1.0));

    vec3 light_sum = kFallbackAmbient * albedo.rgb;

    // Shadow visibility tied to the first directional light (i==0) only — that's
    // the one Scene::OnUpdate* uses to build uLightVP. Slice B may extend to
    // multiple shadow-casting lights via an array of maps.
    float shadow_visibility = SampleShadow(vLightSpacePos);

    for (int i = 0; i < uDirLightCount; ++i) {
        vec3  L    = normalize(-uDirLightDir[i]);
        vec3  H    = normalize(L + V);
        float diff = max(dot(N, L), 0.0);
        float spec = (diff > 0.0) ? pow(max(dot(N, H), 0.0), shininess) : 0.0;
        vec3  contribution = uDirLightColor[i] * (albedo.rgb * diff + vec3(spec));
        if (i == 0) contribution *= shadow_visibility;
        light_sum += contribution;
    }

    for (int i = 0; i < uPointLightCount; ++i) {
        vec3  to_light = uPointLightPos[i] - vWorldPos;
        float dist     = length(to_light);
        float range    = uPointLightRange[i];
        if (dist > range) continue;

        vec3  L = to_light / max(dist, 1e-4);
        vec3  H = normalize(L + V);
        float diff = max(dot(N, L), 0.0);
        float spec = (diff > 0.0) ? pow(max(dot(N, H), 0.0), shininess) : 0.0;

        // Smoothstep falloff over Range - predictable, designer-friendly,
        // no inverse-square surprises near small ranges.
        float atten = 1.0 - smoothstep(0.0, range, dist);
        atten *= atten;

        light_sum += uPointLightColor[i] * (albedo.rgb * diff + vec3(spec)) * atten;
    }

    oColor    = vec4(light_sum, albedo.a);
    oEntityID = uEntityID;
}
