#version 460 core

layout(location = 0) in vec3  aPosition;
layout(location = 1) in vec3  aNormal;
layout(location = 2) in vec2  aTexCoord;
layout(location = 3) in vec4  aTangent; // xyz = tangent (MESH-local for skinned meshes), w = handedness sign
layout(location = 4) in ivec4 aJoints;
layout(location = 5) in vec4  aWeights;

layout(std140, binding = 0) uniform Camera {
    mat4 uViewProjection;
};

// 128 joints x 64 bytes = 8 KB. Well within OpenGL's guaranteed minimum UBO
// size (16 KB). Updated once per skinned-mesh Submit().
layout(std140, binding = 1) uniform Bones {
    mat4 uBoneMatrices[128];
};

uniform mat4 uModel;
uniform mat4 uLightVP0;
uniform mat4 uLightVP1;
uniform mat4 uLightVP2;
uniform mat4 uLightVP3;
uniform int  uShadowsEnabled;

out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vTexCoord;
out vec3 vWorldTangent;
out vec3 vWorldBitangent;
out vec4 vLightSpacePos0;
out vec4 vLightSpacePos1;
out vec4 vLightSpacePos2;
out vec4 vLightSpacePos3;

void main() {
    // Linear blend skinning: weighted sum of skin matrices for the 4 bones
    // that influence this vertex. Weights sum to 1 (importer renormalizes).
    mat4 skin = aWeights.x * uBoneMatrices[aJoints.x]
              + aWeights.y * uBoneMatrices[aJoints.y]
              + aWeights.z * uBoneMatrices[aJoints.z]
              + aWeights.w * uBoneMatrices[aJoints.w];

    vec4 world = uModel * skin * vec4(aPosition, 1.0);
    vWorldPos  = world.xyz;

    // Normal goes through model3 * skin3 (skin's mat3 part), then inverse-transpose
    // for any non-uniform scale folded into uModel. Joint matrices are assumed
    // rigid (rotation + translation) so skin3 needs no inverse-transpose itself.
    mat3 model3        = mat3(uModel);
    mat3 skin3         = mat3(skin);
    mat3 normal_matrix = transpose(inverse(model3 * skin3));
    vec3 N             = normalize(normal_matrix * aNormal);
    vWorldNormal       = N;

    // Tangent transforms by the upper-3x3 (NOT inverse-transpose). Re-orthogonalize
    // against N to correct interpolation drift, then derive B.
    vec3 T_raw = model3 * (skin3 * aTangent.xyz);
    vec3 T     = normalize(T_raw - N * dot(N, T_raw));
    vec3 B     = cross(N, T) * aTangent.w;
    vWorldTangent   = T;
    vWorldBitangent = B;

    vTexCoord = aTexCoord;

    vLightSpacePos0 = uLightVP0 * world;
    vLightSpacePos1 = uLightVP1 * world;
    vLightSpacePos2 = uLightVP2 * world;
    vLightSpacePos3 = uLightVP3 * world;

    gl_Position = uViewProjection * world;
}
