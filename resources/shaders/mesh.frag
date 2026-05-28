#version 460 core

layout(location = 0) out vec4 oColor;
layout(location = 1) out int  oEntityID;

in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vTexCoord;
in vec3 vWorldTangent;
in vec3 vWorldBitangent;
in vec4 vLightSpacePos0;
in vec4 vLightSpacePos1;
in vec4 vLightSpacePos2;
in vec4 vLightSpacePos3;

#define MAX_DIR_LIGHTS   4
#define MAX_POINT_LIGHTS 16
#define CASCADE_COUNT    4

uniform sampler2D   uAlbedoTexture;
uniform sampler2D   uORMTexture;       // ORM packing: R=AO, G=roughness, B=metallic. White fallback = no occlusion, factors only.
uniform sampler2D   uEmissiveTexture;  // glTF: sRGB, multiplied by uEmissiveFactor
uniform sampler2D   uNormalMapTexture; // tangent-space normal map (unit 10). Flat fallback = (128,128,255) -> (0,0,1) -> passes through geometry normal.
uniform sampler2D   uShadowMap0;
uniform sampler2D   uShadowMap1;
uniform sampler2D   uShadowMap2;
uniform sampler2D   uShadowMap3;
uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilterMap;
uniform sampler2D   uBRDFLUT;
uniform int         uHasIBL;
uniform int         uHasPrefilter;
uniform float       uMaxReflectionLOD;   // log2(prefilter face size)

// Debug visualizations. 0 = PBR (default), >0 = bypass PBR and write the
// requested intermediate value (post tonemap+gamma where appropriate).
// Set via Renderer3D::SetDebugViz from the toolbar dropdown.
//   1 = raw irradiance sample at this fragment's world normal (cubemap content
//       check — if this looks like recognizable environment features, the
//       irradiance convolution didn't actually low-pass the source)
//   2 = world-space normal encoded as color (sanity-check the mesh normals)
//   3 = NdotL on the first directional light (verifies light direction)
//   4 = NdotV (verifies view-vector / camera position is sane)
//   5 = albedo only (no lighting)
//   6 = prefilter sample along reflection vector at the surface's roughness
//       (verifies B.3 prefilter quality — low roughness should look mirror-
//       like, high roughness should look like the diffuse irradiance)
//   7 = BRDF LUT lookup as (R=scale, G=bias) — should be a smooth gradient,
//       bias dark at low NdotV/roughness, scale dim at high roughness
uniform int         uDebugViz;
uniform float     uCascadeSplits[CASCADE_COUNT]; // world-space far distance per cascade
uniform vec4      uAlbedoColor;
uniform float     uRoughness;
uniform float     uMetallic;
uniform vec3      uEmissiveFactor;   // linear-space, HDR-allowed
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

const float kPI    = 3.14159265359;
const float kGamma = 2.2; // used only for sRGB → linear conversion of input textures

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
// to the next cascade. bias_scale compensates for the cascade's world-units-
// per-texel growth (cascade 0 = 1, cascade 1 = 2, etc.); without it, far
// cascades produce visibly different shadow brightness at boundaries.
float SampleShadowCascade(sampler2D shadow_map, vec4 light_space_pos,
                          vec3 N, vec3 L, float bias_scale,
                          out bool inside_frustum) {
    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj = proj * 0.5 + 0.5;
    inside_frustum = (proj.x >= 0.0 && proj.x <= 1.0 &&
                      proj.y >= 0.0 && proj.y <= 1.0 &&
                      proj.z >= 0.0 && proj.z <= 1.0);
    if (!inside_frustum) return 1.0;

    float bias = max(0.002 * (1.0 - dot(N, L)), 0.0003) * bias_scale;

    // 5x5 PCF kernel = 25 samples per cascade. Edge transition spans 5 texels,
    // visibly softer than the previous 3x3 (3-texel) gradient without crossing
    // into "obviously blurry" territory.
    vec2  texel_size = 1.0 / vec2(textureSize(shadow_map, 0));
    float visibility = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2  offset       = vec2(x, y) * texel_size;
            float depth_in_map = texture(shadow_map, proj.xy + offset).r;
            visibility += (proj.z - bias) > depth_in_map ? 0.0 : 1.0;
        }
    }
    return visibility / 25.0;
}

// Picks the appropriate cascade based on view-space depth and PCF-samples it.
// GLSL forbids dynamic indexing of sampler arrays, hence the hand-unrolled
// branches (CASCADE_COUNT matched against Renderer3D::kCascadeCount).
//
// Cascade-boundary blending: in the outer kBlendFrac of each cascade we also
// sample the next cascade and lerp by blend alpha. This hides the otherwise-
// visible "shooting-target" rings centered on the camera that result from
// the texel-size + bias step at each cascade boundary. Cost: one extra cascade
// fetch (25 more PCF taps) per fragment, but only inside the thin boundary
// band — most fragments take the cheap single-cascade path.
const float kBlendFrac = 0.15;

float SampleShadow(vec3 N, vec3 L) {
    if (uShadowsEnabled == 0) return 1.0;

    // View-space depth derived from the perspective divide. gl_FragCoord.z is
    // [0,1] window depth; we'd need to un-project to get linear view depth, but
    // distance from camera is a fine proxy for cascade pick.
    float view_depth = distance(vWorldPos, uViewPos);

    bool  inside_a, inside_b;
    float v_a, v_b;

    if (view_depth < uCascadeSplits[0]) {
        v_a = SampleShadowCascade(uShadowMap0, vLightSpacePos0, N, L, 1.0, inside_a);
        if (!inside_a) v_a = 1.0;

        float blend_zone = uCascadeSplits[0] * kBlendFrac;
        float blend = clamp((view_depth - (uCascadeSplits[0] - blend_zone)) / blend_zone, 0.0, 1.0);
        if (blend > 0.0) {
            v_b = SampleShadowCascade(uShadowMap1, vLightSpacePos1, N, L, 2.0, inside_b);
            if (!inside_b) v_b = 1.0;
            v_a = mix(v_a, v_b, blend);
        }
        return v_a;
    }
    if (view_depth < uCascadeSplits[1]) {
        v_a = SampleShadowCascade(uShadowMap1, vLightSpacePos1, N, L, 2.0, inside_a);
        if (!inside_a) v_a = 1.0;

        float blend_zone = (uCascadeSplits[1] - uCascadeSplits[0]) * kBlendFrac;
        float blend = clamp((view_depth - (uCascadeSplits[1] - blend_zone)) / blend_zone, 0.0, 1.0);
        if (blend > 0.0) {
            v_b = SampleShadowCascade(uShadowMap2, vLightSpacePos2, N, L, 4.0, inside_b);
            if (!inside_b) v_b = 1.0;
            v_a = mix(v_a, v_b, blend);
        }
        return v_a;
    }
    if (view_depth < uCascadeSplits[2]) {
        v_a = SampleShadowCascade(uShadowMap2, vLightSpacePos2, N, L, 4.0, inside_a);
        if (!inside_a) v_a = 1.0;

        float blend_zone = (uCascadeSplits[2] - uCascadeSplits[1]) * kBlendFrac;
        float blend = clamp((view_depth - (uCascadeSplits[2] - blend_zone)) / blend_zone, 0.0, 1.0);
        if (blend > 0.0) {
            v_b = SampleShadowCascade(uShadowMap3, vLightSpacePos3, N, L, 8.0, inside_b);
            if (!inside_b) v_b = 1.0;
            v_a = mix(v_a, v_b, blend);
        }
        return v_a;
    }
    if (view_depth < uCascadeSplits[3]) {
        // Last cascade: blend toward the "fully lit" fallback so geometry past
        // kShadowMaxDistance doesn't pop from shadowed to bright in one pixel.
        v_a = SampleShadowCascade(uShadowMap3, vLightSpacePos3, N, L, 8.0, inside_a);
        if (!inside_a) v_a = 1.0;

        float blend_zone = (uCascadeSplits[3] - uCascadeSplits[2]) * kBlendFrac;
        float blend = clamp((view_depth - (uCascadeSplits[3] - blend_zone)) / blend_zone, 0.0, 1.0);
        return mix(v_a, 1.0, blend);
    }
    return 1.0; // beyond the last cascade -> fully lit
}

void main() {
    vec4 albedo_sample = texture(uAlbedoTexture, vTexCoord) * uAlbedoColor;
    // sRGB -> linear: color textures + inspector color picker values are stored
    // in display (sRGB) space; PBR math must run in linear space. Alpha is
    // unitless and passes through unchanged.
    vec3 albedo = pow(albedo_sample.rgb, vec3(kGamma));

    // Build the per-fragment normal from the tangent-space normal map.
    // Re-normalize the interpolated TBN basis (interpolation un-normalizes it),
    // then decode the map sample from [0,1] -> [-1,1] and transform to world space.
    vec3 T = normalize(vWorldTangent);
    vec3 B = normalize(vWorldBitangent);
    vec3 Ng = normalize(vWorldNormal);
    // Gram-Schmidt re-orthogonalize T against Ng in the fragment shader to
    // handle the rare case where vertex-shader Gram-Schmidt + interpolation still
    // drifts (e.g., sharp creases with very different normals per vertex).
    T = normalize(T - Ng * dot(Ng, T));
    B = cross(Ng, T);
    mat3 TBN          = mat3(T, B, Ng);
    vec3 normal_ts    = texture(uNormalMapTexture, vTexCoord).rgb * 2.0 - 1.0;
    vec3 N            = normalize(TBN * normal_ts);

    vec3 V             = normalize(uViewPos - vWorldPos);

    // ── Debug visualizations ──────────────────────────────────────────────
    // These short-circuit before any PBR math so they output the raw quantity.
    if (uDebugViz != 0) {
        vec3 dbg = vec3(0.0);
        // All debug modes output linear values — the tonemap pass applies
        // ACES + sRGB so HDR modes (1, 6) roll off correctly and LDR modes
        // (2–5, 7) pass through nearly unchanged.
        if (uDebugViz == 1) {
            dbg = textureLod(uIrradianceMap, N, 0.0).rgb;
        } else if (uDebugViz == 2) {
            dbg = N * 0.5 + 0.5;
        } else if (uDebugViz == 3) {
            vec3 L = (uDirLightCount > 0) ? normalize(-uDirLightDir[0]) : vec3(0.0, 1.0, 0.0);
            dbg = vec3(max(dot(N, L), 0.0));
        } else if (uDebugViz == 4) {
            dbg = vec3(max(dot(N, V), 0.0));
        } else if (uDebugViz == 5) {
            dbg = albedo; // linear-space albedo; tonemap pass converts to display
        } else if (uDebugViz == 6) {
            float r_dbg = clamp(uRoughness, 0.04, 1.0);
            vec3  R     = reflect(-V, N);
            dbg         = textureLod(uPrefilterMap, R, r_dbg * uMaxReflectionLOD).rgb;
        } else if (uDebugViz == 7) {
            float r_dbg = clamp(uRoughness, 0.04, 1.0);
            float cosNV = max(dot(N, V), 0.0);
            vec2  lut   = texture(uBRDFLUT, vec2(cosNV, r_dbg)).rg;
            dbg         = vec3(lut, 0.0);
        }
        oColor    = vec4(dbg, 1.0);
        oEntityID = uEntityID;
        return;
    }

    // ORM-packed map — R=AO, G=roughness, B=metallic. One fetch feeds all
    // three terms. The Roughness/Metallic factors multiply the sampled
    // channels (a 1x1 white fallback leaves them as-is). The roughness floor
    // avoids NaN at perfect-mirror values. AO (the .r component) is applied
    // below, gated on the IBL path.
    vec3  ormSample = texture(uORMTexture, vTexCoord).rgb;
    float ao        = ormSample.r;
    float roughness = clamp(uRoughness * ormSample.g, 0.04, 1.0);
    float metallic  = clamp(uMetallic  * ormSample.b, 0.0,  1.0);

    // F0 = reflectance at normal incidence. Dielectrics share ~0.04; metals use albedo
    // as their tint (the metallic flow's whole point).
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3  shadow_L          = (uDirLightCount > 0) ? normalize(-uDirLightDir[0]) : vec3(0.0, 1.0, 0.0);
    float shadow_visibility = SampleShadow(N, shadow_L);

    // Ambient: split-sum IBL — diffuse irradiance (low-freq Lambertian)
    // + roughness-convolved specular prefilter modulated by the precomputed
    // BRDF LUT. No fallback grey: the editor binds its own fallback IBL
    // during level construction; play mode with no scene environment
    // intentionally has zero ambient so materials reveal direct-light-only
    // behavior (which is what the shipped game will show).
    vec3 lit = vec3(0.0);
    if (uHasIBL == 1) {
        // Roughness-aware Fresnel. Standard Schlick saturates F to 1.0 at
        // grazing angles regardless of roughness — physically correct for
        // mirrors but too bright for rough surfaces, which scatter most of
        // the grazing reflection. Lazarov-style "F0 grows with smoothness"
        // term softens this for IBL specifically.
        float cosNV   = max(dot(N, V), 0.0);
        vec3  F_at_N  = F0 + (max(vec3(1.0 - roughness), F0) - F0)
                              * pow(clamp(1.0 - cosNV, 0.0, 1.0), 5.0);
        vec3  kS      = F_at_N;
        vec3  kD      = (vec3(1.0) - kS) * (1.0 - metallic);

        // Diffuse IBL — textureLod at mip 0 because the irradiance map is
        // low-frequency by construction (Lambertian-convolved). Letting the
        // driver pick mips from screen-space derivatives produces faint
        // concentric rings on curved surfaces ("LOD lottery").
        vec3 irradiance = textureLod(uIrradianceMap, N, 0.0).rgb;
        vec3 diffuseIBL = kD * irradiance * albedo;

        // Specular IBL — Karis 2013 split-sum:
        //   prefilter(R, roughness) * (F * envBRDF.r + envBRDF.g)
        // The prefilter cubemap is roughness-convolved per mip, so we
        // sample at LOD = roughness * maxLOD. The BRDF LUT is environment-
        // independent and stores (scale, bias) such that the specular
        // contribution reconstructs as F0*scale + bias.
        vec3 specularIBL = vec3(0.0);
        if (uHasPrefilter == 1) {
            vec3  R           = reflect(-V, N);
            vec3  prefiltered = textureLod(uPrefilterMap, R, roughness * uMaxReflectionLOD).rgb;
            vec2  envBRDF     = texture(uBRDFLUT, vec2(cosNV, roughness)).rg;
            specularIBL       = prefiltered * (F_at_N * envBRDF.x + envBRDF.y);
        }

        lit = (diffuseIBL + specularIBL) * ao;
    }

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

    // Emissive — glTF stores the texture in sRGB and the factor in linear
    // space; linearize the texture and multiply. White fallback texture is
    // (1,1,1) post-gamma, so a zero factor still mutes the material (the
    // common no-emissive case). HDR factors are allowed and pass through
    // the ACES tonemap below.
    vec3 emissive_sample = pow(texture(uEmissiveTexture, vTexCoord).rgb, vec3(kGamma));
    lit += emissive_sample * uEmissiveFactor;

    // Output linear HDR — the post-process tonemap pass (tonemap.frag) applies
    // ACES + sRGB encode to the whole scene in one unified step.
    oColor    = vec4(lit, albedo_sample.a);
    oEntityID = uEntityID;
}
