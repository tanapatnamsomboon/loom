#version 460 core

in  vec3 vLocalPos;
out vec4 oColor;

uniform samplerCube uEnvironment;
uniform float       uRoughness;        // 0..1, varies per mip level
uniform float       uSourceFaceSize;   // face size of the source env cubemap (for Karis LOD)

const float PI         = 3.14159265359;
const uint  kSampleCnt = 1024u;        // 1024 GGX importance samples per output texel

// ── Hammersley low-discrepancy sequence ──
// Pairs with GGX importance sampling below. Far better convergence than a
// regular grid: the sample distribution adapts to the GGX lobe, so even at
// 1024 samples mid-roughness mips converge to within imperceptible noise.
float RadicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
vec2 Hammersley(uint i, uint n) {
    return vec2(float(i) / float(n), RadicalInverseVdC(i));
}

// GGX importance sampling — returns a half-vector H in world space whose
// distribution matches D(GGX, roughness). Surface normal N defines the
// tangent frame; the resulting H clusters along N for low roughness and
// spreads to a wide lobe for high roughness.
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;

    float phi      = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H_tan = vec3(sinTheta * cos(phi),
                      sinTheta * sin(phi),
                      cosTheta);

    // Tangent → world via an orthonormal basis built around N.
    vec3 up      = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitan   = cross(N, tangent);

    return normalize(tangent * H_tan.x + bitan * H_tan.y + N * H_tan.z);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

void main() {
    vec3 N = normalize(vLocalPos);
    // Split-sum approximation (Karis 2013): the prefilter integrand assumes
    // V == R == N. Removes V from the integral so the result is purely a
    // function of (N, roughness), letting us bake it into a cubemap.
    vec3 R = N;
    vec3 V = R;

    float total_weight = 0.0;
    vec3  prefiltered  = vec3(0.0);

    for (uint i = 0u; i < kSampleCnt; ++i) {
        vec2 Xi = Hammersley(i, kSampleCnt);
        vec3 H  = ImportanceSampleGGX(Xi, N, uRoughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) continue;

        // Karis 2014 mipmap pre-filtering trick — same idea as in the
        // irradiance shader. Choose a source-mip LOD whose texel solid
        // angle matches the importance-sample's solid angle so bright HDR
        // pinpoint lights get pre-averaged instead of aliasing into bright
        // dots. Without this, mid-roughness mips show "fireflies" — single
        // bright texels surrounded by dark.
        //
        //   pdf     = GGX importance-sample density at this H
        //   saTexel = source texel solid angle on the cubemap
        //   saSample = ideal sample solid angle = 1 / (kSampleCnt * pdf)
        //   LOD     = 0.5 * log2(saSample / saTexel), clamped to [0, ...]
        float D    = DistributionGGX(N, H, uRoughness);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);
        float pdf   = D * NdotH / (4.0 * HdotV) + 1e-4;

        float saTexel  = 4.0 * PI / (6.0 * uSourceFaceSize * uSourceFaceSize);
        float saSample = 1.0 / (float(kSampleCnt) * pdf + 1e-4);
        // Roughness=0 needs no convolution (perfect mirror — sample the
        // source directly). Without the floor, the formula above produces
        // LOD < 0 which still works on most drivers but is technically UB.
        float lod = uRoughness == 0.0 ? 0.0
                                      : 0.5 * log2(saSample / saTexel);

        prefiltered  += textureLod(uEnvironment, L, lod).rgb * NdotL;
        total_weight += NdotL;
    }

    prefiltered /= max(total_weight, 1e-4);
    oColor       = vec4(prefiltered, 1.0);
}
