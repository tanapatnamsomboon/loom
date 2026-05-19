#version 460 core

in  vec3 vLocalPos;
out vec4 oColor;

uniform samplerCube uEnvironment;
// Per-face resolution of the source env cubemap. Used to choose a source-mip
// LOD that matches our integration-sample density so bright HDR pinpoints get
// pre-averaged instead of aliased into "dot swirl" patterns in the output.
uniform float       uSourceFaceSize;

const float PI = 3.14159265359;

// Lambertian diffuse irradiance via brute-force hemisphere convolution.
// For each output cubemap texel (direction N), accumulate
// `texture(env, omega) * cos(theta) * sin(theta)` over the hemisphere
// oriented around N, then normalize. Runs once at HDR-load time.
//
// Integer-counted loops on purpose: float-counted GLSL loops have shown
// driver-dependent iteration counts (precision drift on the loop variable
// causes early exit). Integer-counted is bulletproof. ~32k samples per
// output texel, enough to smooth even high-variance HDR (bright lights
// on dark backgrounds) without visible noise.
const int   kPhiSteps   = 256;  // 0 .. 2π
const int   kThetaSteps = 96;   // 0 .. π/2  (≈ 24,576 samples — 2× the
                                // float-loop baseline; combined with the
                                // Karis LOD bias below this keeps the
                                // output cubemap free of bright-pixel
                                // variance even on studio HDRs)
const float kPhiDelta   = 2.0 * PI / float(kPhiSteps);
const float kThetaDelta = 0.5 * PI / float(kThetaSteps);

void main() {
    vec3 N = normalize(vLocalPos);

    // Orthonormal basis (right, up, N). Swap up to (0,0,1) when N parallels
    // world up to avoid a degenerate cross product.
    vec3 up    = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));

    // Karis 2014 mipmap pre-filtering trick: choose a source-mip LOD whose
    // texel solid angle matches our integration sample's solid angle. Each
    // integration sample then averages over the matching chunk of source
    // HDR, so bright pinpoint lights don't alias into noisy variance
    // between adjacent output texels (which on a sphere reads as dotted
    // moiré swirls). Requires the source cubemap to have a mip chain.
    //
    //   saTexel = solid angle of one source-cubemap texel
    //   saSample = solid angle assigned per integration sample (here we
    //              integrate the upper hemisphere = 2π divided over N
    //              uniformly-weighted samples)
    //   LOD     = 0.5 * log2(saSample / saTexel)
    float saTexel  = (4.0 * PI / 6.0) / (uSourceFaceSize * uSourceFaceSize);
    float saSample = 2.0 * PI / float(kPhiSteps * kThetaSteps);
    // +2 bias on top of the Karis match: studio HDRs with bright pinpoint
    // lights need extra pre-averaging to avoid output-texel variance from
    // the regular sample grid hitting bright spots inconsistently across
    // adjacent output texels (visible as faint concentric rings on a sphere).
    // Diffuse is low-frequency enough that the extra blur is invisible.
    float lod      = max(0.5 * log2(saSample / saTexel) + 2.0, 0.0);

    vec3  irradiance = vec3(0.0);
    float n_samples  = 0.0;

    for (int p = 0; p < kPhiSteps; ++p) {
        float phi = float(p) * kPhiDelta;
        float cos_phi = cos(phi);
        float sin_phi = sin(phi);
        for (int t = 0; t < kThetaSteps; ++t) {
            float theta     = float(t) * kThetaDelta;
            float cos_theta = cos(theta);
            float sin_theta = sin(theta);

            // Spherical → tangent-space → world-space direction.
            vec3 tangent    = vec3(sin_theta * cos_phi,
                                   sin_theta * sin_phi,
                                   cos_theta);
            vec3 sample_dir = tangent.x * right + tangent.y * up + tangent.z * N;

            irradiance += textureLod(uEnvironment, sample_dir, lod).rgb
                        * cos_theta * sin_theta;
            n_samples  += 1.0;
        }
    }
    irradiance = PI * irradiance / n_samples;

    oColor = vec4(irradiance, 1.0);
}
