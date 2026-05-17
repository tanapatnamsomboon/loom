#version 460 core

layout(location = 0) out vec4 oColor;
layout(location = 1) out int  oEntityID;

in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vTexCoord;
in vec4 vLightSpacePos0;
in vec4 vLightSpacePos1;
in vec4 vLightSpacePos2;
in vec4 vLightSpacePos3;

#define MAX_DIR_LIGHTS   4
#define MAX_POINT_LIGHTS 16
#define CASCADE_COUNT    4

uniform sampler2D uAlbedoTexture;
uniform sampler2D uShadowMap0;
uniform sampler2D uShadowMap1;
uniform sampler2D uShadowMap2;
uniform sampler2D uShadowMap3;
uniform float     uCascadeSplits[CASCADE_COUNT]; // world-space far distance per cascade
uniform vec4      uAlbedoColor;
uniform float     uRoughness;
uniform float     uMetallic;   // unused until PBR slice
uniform int       uEntityID;
uniform vec3      uViewPos;
uniform int       uShadowsEnabled;

uniform int   uDirLightCount;
uniform vec3  uDirLightDir[MAX_DIR_LIGHTS];
uniform vec3  uDirLightColor[MAX_DIR_LIGHTS];

uniform int   uPointLightCount;
uniform vec3  uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3  uPointLightColor[MAX_POINT_LIGHTS];
uniform float uPointLightRange[MAX_POINT_LIGHTS];

const vec3 kFallbackAmbient = vec3(0.20);

// PCF-sampled shadow visibility from one cascade. Returns 1.0 (lit) when the
// fragment falls outside the cascade's frustum so the caller can fall through
// to the next cascade.
float SampleShadowCascade(sampler2D shadow_map, vec4 light_space_pos,
                          vec3 N, vec3 L, out bool inside_frustum) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;
    inside_frustum = (proj.x >= 0.0 && proj.x <= 1.0 &&
                      proj.y >= 0.0 && proj.y <= 1.0 &&
                      proj.z >= 0.0 && proj.z <= 1.0);
    if (!inside_frustum) return 1.0;

    float bias = max(0.002 * (1.0 - dot(N, L)), 0.0003);

    vec2  texel_size = 1.0 / vec2(textureSize(shadow_map, 0));
    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2  offset       = vec2(x, y) * texel_size;
            float depth_in_map = texture(shadow_map, proj.xy + offset).r;
            visibility += (proj.z - bias) > depth_in_map ? 0.0 : 1.0;
        }
    }
    return visibility / 9.0;
}

// Picks the appropriate cascade based on view-space depth and PCF-samples it.
// GLSL forbids dynamic indexing of sampler arrays, hence the hand-unrolled
// branches (CASCADE_COUNT matched against Renderer3D::kCascadeCount).
float SampleShadow(vec3 N, vec3 L) {
    if (uShadowsEnabled == 0) return 1.0;

    // View-space depth derived from the perspective divide. gl_FragCoord.z is
    // [0,1] window depth; we'd need to un-project to get linear view depth, but
    // distance from camera is a fine proxy for cascade pick.
    float view_depth = distance(vWorldPos, uViewPos);

    bool  inside;
    float v;

    if (view_depth < uCascadeSplits[0]) {
        v = SampleShadowCascade(uShadowMap0, vLightSpacePos0, N, L, inside);
        if (inside) return v;
    }
    if (view_depth < uCascadeSplits[1]) {
        v = SampleShadowCascade(uShadowMap1, vLightSpacePos1, N, L, inside);
        if (inside) return v;
    }
    if (view_depth < uCascadeSplits[2]) {
        v = SampleShadowCascade(uShadowMap2, vLightSpacePos2, N, L, inside);
        if (inside) return v;
    }
    if (view_depth < uCascadeSplits[3]) {
        v = SampleShadowCascade(uShadowMap3, vLightSpacePos3, N, L, inside);
        if (inside) return v;
    }
    return 1.0; // beyond the last cascade -> fully lit
}

void main() {
    vec4 albedo = texture(uAlbedoTexture, vTexCoord) * uAlbedoColor;
    vec3 N      = normalize(vWorldNormal);
    vec3 V      = normalize(uViewPos - vWorldPos);

    float shininess = mix(2.0, 256.0, 1.0 - clamp(uRoughness, 0.0, 1.0));

    vec3 light_sum = kFallbackAmbient * albedo.rgb;

    vec3  shadow_L          = (uDirLightCount > 0) ? normalize(-uDirLightDir[0]) : vec3(0.0, 1.0, 0.0);
    float shadow_visibility = SampleShadow(N, shadow_L);

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

        float atten = 1.0 - smoothstep(0.0, range, dist);
        atten *= atten;

        light_sum += uPointLightColor[i] * (albedo.rgb * diff + vec3(spec)) * atten;
    }

    oColor    = vec4(light_sum, albedo.a);
    oEntityID = uEntityID;
}
