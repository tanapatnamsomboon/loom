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
uniform float     uMetallic;
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

// Always-on ambient term — placeholder until the IBL slice replaces it with
// diffuse irradiance + specular prefilter sampling. Restored to a properly
// low PBR value (0.03) now that tonemap + gamma at output lift the perceived
// brightness back to a reasonable level.
const vec3  kFallbackAmbient = vec3(0.03);
const float kPI              = 3.14159265359;
const float kGamma           = 2.2;

// ACES filmic tonemap — Krzysztof Narkowicz's curve-fit approximation. Maps
// open-ended HDR radiance to [0, 1] LDR with film-like roll-off in highlights
// and bottom-end contrast; matches what Unity HDRP / Unreal use by default.
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// ── Cook-Torrance microfacet BRDF ─────────────────────────────────────────
// Standard metallic-roughness model. References:
//   - Karis "Real Shading in Unreal Engine 4" (2013)
//   - "Moving Frostbite to Physically Based Rendering" (Lagarde 2014)

// Normal Distribution Function — GGX / Trowbridge-Reitz.
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness * roughness;        // perceptual -> linear roughness
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (kPI * denom * denom);
}

// Geometry function — Smith with Schlick-GGX, direct-lighting k remap.
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Fresnel — Schlick approximation. F0 = 0.04 for dielectrics, albedo for metals.
vec3 FresnelSchlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

// Evaluates the per-light radiance contribution for one analytic light.
// `radiance` is the light's incident color (already includes intensity + any
// attenuation); `L` is the unit vector from surface to light.
vec3 EvaluatePBRLight(vec3 N, vec3 V, vec3 L, vec3 radiance,
                      vec3 albedo, float roughness, float metallic, vec3 F0) {
    vec3  H     = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3  F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    // Specular = D * F * G / (4 * NdotV * NdotL); add epsilon to avoid div-by-zero.
    float NdotV    = max(dot(N, V), 0.0);
    vec3  specular = (D * F * G) / (4.0 * NdotV * NdotL + 1e-4);

    // Energy conservation: kS is the specular fraction (Fresnel); the remainder
    // goes to diffuse, and metallic surfaces have no diffuse contribution at all.
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    return (kD * albedo / kPI + specular) * radiance * NdotL;
}

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
    vec4 albedo_sample = texture(uAlbedoTexture, vTexCoord) * uAlbedoColor;
    // sRGB -> linear: color textures + inspector color picker values are stored
    // in display (sRGB) space; PBR math must run in linear space. Alpha is
    // unitless and passes through unchanged.
    vec3 albedo        = pow(albedo_sample.rgb, vec3(kGamma));
    vec3 N             = normalize(vWorldNormal);
    vec3 V             = normalize(uViewPos - vWorldPos);

    float roughness = clamp(uRoughness, 0.04, 1.0); // floor avoids NaN at perfect mirror
    float metallic  = clamp(uMetallic,  0.0,  1.0);

    // F0 = reflectance at normal incidence. Dielectrics share ~0.04; metals use albedo
    // as their tint (the metallic flow's whole point).
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3  shadow_L          = (uDirLightCount > 0) ? normalize(-uDirLightDir[0]) : vec3(0.0, 1.0, 0.0);
    float shadow_visibility = SampleShadow(N, shadow_L);

    // Ambient placeholder. IBL slice will replace with diffuse-irradiance +
    // prefiltered-specular sampling.
    vec3 lit = kFallbackAmbient * albedo;

    for (int i = 0; i < uDirLightCount; ++i) {
        vec3 L         = normalize(-uDirLightDir[i]);
        vec3 radiance  = uDirLightColor[i];
        vec3 contrib   = EvaluatePBRLight(N, V, L, radiance, albedo, roughness, metallic, F0);
        if (i == 0) contrib *= shadow_visibility;
        lit += contrib;
    }

    for (int i = 0; i < uPointLightCount; ++i) {
        vec3  to_light = uPointLightPos[i] - vWorldPos;
        float dist     = length(to_light);
        float range    = uPointLightRange[i];
        if (dist > range) continue;

        vec3  L = to_light / max(dist, 1e-4);

        // Smoothstep falloff over Range — designer-friendly, no inverse-square
        // singularity. Squared for a softer near-falloff curve.
        float atten = 1.0 - smoothstep(0.0, range, dist);
        atten *= atten;

        vec3 radiance = uPointLightColor[i] * atten;
        lit += EvaluatePBRLight(N, V, L, radiance, albedo, roughness, metallic, F0);
    }

    // HDR -> LDR via ACES tonemapping, then linear -> sRGB for the framebuffer
    // (which is RGBA8 displayed verbatim — no GPU sRGB conversion in the chain).
    vec3 tonemapped = ACESFilm(lit);
    vec3 display    = pow(tonemapped, vec3(1.0 / kGamma));

    oColor    = vec4(display, albedo_sample.a);
    oEntityID = uEntityID;
}
