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

// Returns a visibility scalar in [0, 1] for the fragment. 1.0 = fully lit, 0.0 =
// fully shadowed; values in between come from the PCF kernel partially passing
// the depth test (the "P" in PCF — averaged texel coverage). Returns 1.0 for any
// fragment outside the shadow frustum (the shadow map's border samples as 1.0,
// see DEPTH32F setup in opengl_framebuffer.cpp).
//
// Bias is slope-scale: surfaces nearly parallel to the light direction (low N.L)
// would otherwise self-shadow due to depth-precision wobble, so we widen the
// tolerance there. The 0.0005 floor stops perpendicular surfaces from peter-panning.
float SampleShadow(vec4 light_space_pos, vec3 N, vec3 L) {
    if (uShadowsEnabled == 0) return 1.0;

    vec3 proj = light_space_pos.xyz / light_space_pos.w; // perspective-style divide (safe for ortho too)
    proj = proj * 0.5 + 0.5;                              // NDC [-1,1] -> [0,1] for texture sampling

    // Outside the orthographic frustum -> always lit. Behind the near plane (z<0)
    // also counts as lit so points "behind the light" don't fall into shadow.
    if (proj.z > 1.0 || proj.z < 0.0) return 1.0;

    // Slope-scale bias: widest at grazing angles (where depth precision wobble
    // would cause shadow acne), tightest at perpendicular surfaces. The shadow
    // map records the caster's FRONT face (no culling tweak in the depth pass),
    // so the contact point with the ground naturally sits at different depths
    // and there's no peter-panning to fight.
    float bias = max(0.002 * (1.0 - dot(N, L)), 0.0003);

    // 3x3 PCF — averages 9 single-texel-offset samples. Hard edges become a
    // single-pixel soft gradient; combined with front-face culling in the
    // depth pass it kills the wavy silhouette + acne pattern.
    vec2  texel_size = 1.0 / vec2(textureSize(uShadowMap, 0));
    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2  offset       = vec2(x, y) * texel_size;
            float depth_in_map = texture(uShadowMap, proj.xy + offset).r;
            visibility += (proj.z - bias) > depth_in_map ? 0.0 : 1.0;
        }
    }
    return visibility / 9.0;
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
    // the one Scene::OnUpdate* uses to build uLightVP. Slope-scale bias needs the
    // surface normal and that same light's L vector, so compute it once up-front.
    vec3  shadow_L          = (uDirLightCount > 0) ? normalize(-uDirLightDir[0]) : vec3(0.0, 1.0, 0.0);
    float shadow_visibility = SampleShadow(vLightSpacePos, N, shadow_L);

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
